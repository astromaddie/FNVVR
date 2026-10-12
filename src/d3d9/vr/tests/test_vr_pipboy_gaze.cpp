#include <cmath>

#include "test.h"

#include "../vr_math.h"
#include "../vr_pipboy_gaze.h"

using namespace dxvk;

namespace {

  constexpr float Frame = 1.0f / 90.0f;

  // Head at eye height, looking straight ahead (-Z)
  const VrPose Head = { { 0.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 1.6f, 0.0f } };

  // In front of and below the head, about 20 degrees down
  const VrVector3 Watched = { 0.0f, 1.6f - 0.14f, -0.4f };
  // Far to the side, out of the gaze
  const VrVector3 Away    = { 0.5f, 1.0f, 0.0f };

  struct Gaze {
    VrPipBoyGaze  gaze;
    VrPipBoyState state    = VrPipBoyState::Closed;
    bool          canOpen  = true;
    bool          canClose = true;

    // Runs frames for a duration and returns the last request
    VrPipBoyRequest run(const VrVector3& screen, float duration) {
      VrPipBoyRequest request = VrPipBoyRequest::None;

      for (float t = 0.0f; t < duration; t += Frame) {
        VrPipBoyGazeInput input;
        input.head     = &Head;
        input.screen   = &screen;
        input.state    = state;
        input.canOpen  = canOpen;
        input.canClose = canClose;
        input.dt       = Frame;
        request = gaze.update(input);
      }

      return request;
    }
  };

}


TEST_CASE(looking_at_needs_angle_and_distance) {
  CHECK(vrIsLookingAt(Head, Watched, VrPipBoyGaze::MaxAngle, VrPipBoyGaze::MaxDistance));
  CHECK(!vrIsLookingAt(Head, Away, VrPipBoyGaze::MaxAngle, VrPipBoyGaze::MaxDistance));

  // Behind the head
  CHECK(!vrIsLookingAt(Head, { 0.0f, 1.6f, 0.4f }, VrPipBoyGaze::MaxAngle, VrPipBoyGaze::MaxDistance));
  // Straight ahead but too far
  CHECK(!vrIsLookingAt(Head, { 0.0f, 1.6f, -1.5f }, VrPipBoyGaze::MaxAngle, VrPipBoyGaze::MaxDistance));
}


TEST_CASE(looking_at_follows_the_head) {
  // Turned 90 degrees to the left, the head looks along -X
  VrPose head = Head;
  head.orientation = vrQuaternionFromAxisAngle({ 0.0f, 1.0f, 0.0f }, 1.5707963f);

  CHECK(vrIsLookingAt(head, { -0.4f, 1.5f, 0.0f }, VrPipBoyGaze::MaxAngle, VrPipBoyGaze::MaxDistance));
  CHECK(!vrIsLookingAt(head, Watched, VrPipBoyGaze::MaxAngle, VrPipBoyGaze::MaxDistance));
}


TEST_CASE(controller_point_uses_the_aim_frame) {
  VrControllerState controller;
  controller.isActive = true;
  controller.gripPose.position = { 1.0f, 1.0f, 1.0f };
  // Aim turned 90 degrees to the left: forward is -X, right is -Z
  controller.aimPose.orientation = vrQuaternionFromAxisAngle({ 0.0f, 1.0f, 0.0f }, 1.5707963f);

  VrVector3 p = vrComputeControllerPoint(controller, { 0.1f, 0.2f, 0.3f });
  CHECK_NEAR(p.x, 1.0f - 0.1f, 1e-4);
  CHECK_NEAR(p.y, 1.0f + 0.2f, 1e-4);
  CHECK_NEAR(p.z, 1.0f - 0.3f, 1e-4);
}


TEST_CASE(gaze_opens_after_a_delay) {
  Gaze g;
  g.run(Away, 0.1f);

  CHECK(g.run(Watched, VrPipBoyGaze::OpenDelay * 0.5f) == VrPipBoyRequest::None);
  CHECK(g.run(Watched, VrPipBoyGaze::OpenDelay) == VrPipBoyRequest::Open);
}


TEST_CASE(gaze_repeats_the_request_until_the_game_opens) {
  Gaze g;
  g.run(Away, 0.1f);
  CHECK(g.run(Watched, 1.0f) == VrPipBoyRequest::Open);
  CHECK(g.run(Watched, 0.5f) == VrPipBoyRequest::Open);

  g.state = VrPipBoyState::Changing;
  CHECK(g.run(Watched, 0.5f) == VrPipBoyRequest::None);
}


TEST_CASE(gaze_does_not_open_when_not_allowed) {
  Gaze g;
  g.canOpen = false;
  g.run(Away, 0.1f);
  CHECK(g.run(Watched, 1.0f) == VrPipBoyRequest::None);
}


TEST_CASE(gaze_needs_to_look_away_first) {
  // Already looking at the wrist when the gaze starts
  Gaze g;
  CHECK(g.run(Watched, 1.0f) == VrPipBoyRequest::None);

  g.run(Away, Frame);
  CHECK(g.run(Watched, 1.0f) == VrPipBoyRequest::Open);
}


TEST_CASE(gaze_closes_after_looking_away) {
  Gaze g;
  g.state = VrPipBoyState::Open;

  CHECK(g.run(Watched, 2.0f) == VrPipBoyRequest::None);
  CHECK(g.run(Away, VrPipBoyGaze::CloseDelay * 0.5f) == VrPipBoyRequest::None);
  CHECK(g.run(Away, VrPipBoyGaze::CloseDelay) == VrPipBoyRequest::Close);
}


TEST_CASE(gaze_keeps_the_pipboy_open_under_a_menu) {
  Gaze g;
  g.state    = VrPipBoyState::Open;
  g.canClose = false;
  CHECK(g.run(Away, 2.0f) == VrPipBoyRequest::None);

  g.canClose = true;
  CHECK(g.run(Away, Frame) == VrPipBoyRequest::Close);
}


TEST_CASE(closing_with_a_button_does_not_reopen) {
  Gaze g;
  g.state = VrPipBoyState::Open;
  g.run(Watched, 1.0f);

  // Closed by the game while the player still looks at the wrist
  g.state = VrPipBoyState::Closed;
  CHECK(g.run(Watched, 2.0f) == VrPipBoyRequest::None);

  g.run(Away, Frame);
  CHECK(g.run(Watched, 1.0f) == VrPipBoyRequest::Open);
}


TEST_CASE(unknown_screen_counts_as_away) {
  VrPipBoyGaze gaze;
  VrPipBoyGazeInput input;
  input.head    = &Head;
  input.canOpen = true;
  input.dt      = 1.0f;

  CHECK(gaze.update(input) == VrPipBoyRequest::None);
  CHECK(!gaze.isLooking());
}
