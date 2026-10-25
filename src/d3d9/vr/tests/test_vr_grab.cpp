#include "test.h"

#include "../vr_grab.h"
#include "../vr_math.h"

using namespace dxvk;

namespace {

  constexpr float Frame = 1.0f / 90.0f;

  // Facing north (+Y), Z up
  VrGameCameraPose northHand(const VrVector3& position) {
    VrGameCameraPose hand;
    hand.forward  = { 0.0f, 1.0f, 0.0f };
    hand.up       = { 0.0f, 0.0f, 1.0f };
    hand.right    = { 1.0f, 0.0f, 0.0f };
    hand.position = position;
    return hand;
  }

  // Facing east (+X), Z up
  VrGameCameraPose eastHand(const VrVector3& position) {
    VrGameCameraPose hand;
    hand.forward  = { 1.0f, 0.0f, 0.0f };
    hand.up       = { 0.0f, 0.0f, 1.0f };
    hand.right    = { 0.0f, -1.0f, 0.0f };
    hand.position = position;
    return hand;
  }

  VrGrabHandInput input(const VrGameCameraPose* hand, bool grip, bool free = true) {
    VrGrabHandInput result;
    result.hand = hand;
    result.grip = grip;
    result.free = free;
    result.dt   = Frame;
    return result;
  }

}


TEST_CASE(grab_probe_rays_end_at_the_centre_from_the_sphere) {
  VrVector3 centre = { 10.0f, -4.0f, 100.0f };
  auto rays = vrGrabProbeRays(centre, 7.0f);

  for (const VrGrabRay& ray : rays) {
    CHECK_NEAR(vrLength(ray.to - centre), 0.0f, 1e-4f);
    CHECK_NEAR(vrLength(ray.from - centre), 7.0f, 1e-4f);
  }

  // Straight down is one of them, for an object under the hand
  bool below = false;

  for (const VrGrabRay& ray : rays)
    below = below || (ray.from.z < centre.z - 6.9f);

  CHECK(below);
}


TEST_CASE(grab_released_body_is_clear_of_the_player_horizontally) {
  VrVector3 player = { 0.0f, 0.0f, 0.0f };

  CHECK(!vrGrabClearOfPlayer({ 30.0f, 0.0f, 0.0f }, player, 42.0f));
  // High above the player is still over the capsule
  CHECK(!vrGrabClearOfPlayer({ 30.0f, 0.0f, 150.0f }, player, 42.0f));
  CHECK(vrGrabClearOfPlayer({ 30.0f, 30.0f, 0.0f }, player, 42.0f));
}


TEST_CASE(grab_press_while_free_looks_for_a_short_time) {
  VrGameCameraPose hand = northHand({ 0.0f, 0.0f, 0.0f });
  VrGrabHand grab;

  CHECK(grab.update(input(&hand, false)) == VrGrabRequest::None);
  CHECK(grab.update(input(&hand, true)) == VrGrabRequest::Grab);

  uint32_t frames = 1;

  while (grab.update(input(&hand, true)) == VrGrabRequest::Grab)
    frames++;

  CHECK(frames >= uint32_t(VrGrabHand::GrabWindow / Frame) - 1);
  CHECK(frames <= uint32_t(VrGrabHand::GrabWindow / Frame) + 1);

  // Holding the grip longer does not grab, a new press does
  CHECK(grab.update(input(&hand, true)) == VrGrabRequest::None);
  CHECK(grab.update(input(&hand, false)) == VrGrabRequest::None);
  CHECK(grab.update(input(&hand, true)) == VrGrabRequest::Grab);
}


TEST_CASE(grab_press_while_busy_never_grabs) {
  VrGameCameraPose hand = northHand({ 0.0f, 0.0f, 0.0f });
  VrGrabHand grab;

  CHECK(grab.update(input(&hand, true, false)) == VrGrabRequest::None);
  // The hand becoming free with the grip still held does not grab
  CHECK(grab.update(input(&hand, true, true)) == VrGrabRequest::None);
  // Nor does a press of an untracked hand
  grab.update(input(nullptr, false));
  CHECK(grab.update(input(nullptr, true)) == VrGrabRequest::None);
}


TEST_CASE(grab_hold_lets_go_with_the_grip_or_the_hand) {
  VrGameCameraPose hand = northHand({ 0.0f, 0.0f, 0.0f });

  {
    VrGrabHand grab;
    grab.update(input(&hand, true));
    grab.hold(hand, { 0.0f, 5.0f, 0.0f });
    CHECK(grab.holding());
    CHECK(grab.update(input(&hand, true)) == VrGrabRequest::None);
    CHECK(grab.update(input(&hand, false)) == VrGrabRequest::Release);
    grab.drop();
    CHECK(!grab.holding());
  }

  {
    VrGrabHand grab;
    grab.update(input(&hand, true));
    grab.hold(hand, { 0.0f, 5.0f, 0.0f });
    CHECK(grab.update(input(&hand, true, false)) == VrGrabRequest::Release);
  }

  {
    VrGrabHand grab;
    grab.update(input(&hand, true));
    grab.hold(hand, { 0.0f, 5.0f, 0.0f });
    CHECK(grab.update(input(nullptr, true)) == VrGrabRequest::Release);
  }
}


TEST_CASE(grab_lost_object_needs_a_new_press) {
  VrGameCameraPose hand = northHand({ 0.0f, 0.0f, 0.0f });
  VrGrabHand grab;

  grab.update(input(&hand, true));
  grab.hold(hand, { 0.0f, 5.0f, 0.0f });
  grab.drop();

  CHECK(grab.update(input(&hand, true)) == VrGrabRequest::None);
  CHECK(grab.update(input(&hand, false)) == VrGrabRequest::None);
  CHECK(grab.update(input(&hand, true)) == VrGrabRequest::Grab);
}


TEST_CASE(grab_held_point_follows_the_hand_and_turns_with_it) {
  VrGameCameraPose hand = northHand({ 100.0f, 200.0f, 50.0f });
  VrGrabHand grab;

  grab.update(input(&hand, true));
  // Five units ahead of the hand and two above
  grab.hold(hand, { 100.0f, 205.0f, 52.0f });

  VrVector3 still = grab.target(hand);
  CHECK_NEAR(still.x, 100.0f, 1e-4f);
  CHECK_NEAR(still.y, 205.0f, 1e-4f);
  CHECK_NEAR(still.z, 52.0f, 1e-4f);

  // Moved and turned to face east: still ahead and above
  VrGameCameraPose turned = eastHand({ 0.0f, 0.0f, 0.0f });
  VrVector3 moved = grab.target(turned);
  CHECK_NEAR(moved.x, 5.0f, 1e-4f);
  CHECK_NEAR(moved.y, 0.0f, 1e-4f);
  CHECK_NEAR(moved.z, 2.0f, 1e-4f);
}


TEST_CASE(grab_pulled_point_comes_to_the_hand_and_stays) {
  VrGameCameraPose hand = northHand({ 0.0f, 0.0f, 0.0f });
  VrGrabHand grab;

  float distance = 2.0f * VrGameUnitsPerMetre;
  grab.update(input(&hand, true));
  grab.pull(hand, { 0.0f, distance, 0.0f });
  CHECK(grab.pulling());
  CHECK_NEAR(grab.target(hand).y, distance, 1e-3f);

  // Straight along the line from the hand, at the set speed
  CHECK(grab.update(input(&hand, true)) == VrGrabRequest::None);
  CHECK_NEAR(grab.target(hand).y, distance - VrGrabHand::PullSpeed * Frame, 1e-2f);
  CHECK_NEAR(grab.target(hand).x, 0.0f, 1e-4f);

  uint32_t frames = 1;

  while (grab.pulling() && frames < 1000) {
    grab.update(input(&hand, true));
    frames++;
  }

  CHECK(frames <= uint32_t(distance / (VrGrabHand::PullSpeed * Frame)) + 1);
  CHECK_NEAR(vrLength(grab.target(hand)), 0.0f, 1e-4f);

  // Then held in the hand's centre wherever the hand goes
  VrGameCameraPose moved = eastHand({ 50.0f, 20.0f, 10.0f });
  CHECK(grab.update(input(&moved, true)) == VrGrabRequest::None);
  CHECK_NEAR(vrLength(grab.target(moved) - moved.position), 0.0f, 1e-4f);
}


TEST_CASE(grab_pulled_point_follows_the_hand_on_its_way) {
  VrGameCameraPose hand = northHand({ 0.0f, 0.0f, 0.0f });
  VrGrabHand grab;

  grab.update(input(&hand, true));
  grab.pull(hand, { 0.0f, 100.0f, 0.0f });
  grab.update(input(&hand, true));

  // Turned to the east, the point is in front of the hand there
  VrGameCameraPose turned = eastHand({ 0.0f, 0.0f, 0.0f });
  VrVector3 target = grab.target(turned);
  CHECK(target.x > 90.0f);
  CHECK_NEAR(target.y, 0.0f, 1e-3f);

  // Letting go stops the pull
  CHECK(grab.update(input(&turned, false)) == VrGrabRequest::Release);
  grab.drop();
  CHECK(!grab.pulling());
}
