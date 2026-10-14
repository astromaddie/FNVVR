#include <cmath>

#include "test.h"

#include "../vr_hand_hud.h"
#include "../vr_math.h"

using namespace dxvk;

namespace {

  constexpr float Frame = 1.0f / 90.0f;

  // The game's layout for a 4:3 screen without safe zone: health bar and
  // compass in the bottom left corner, prompt at the bottom centre
  const VrInterfaceRect HitPoints = {  10.0f, 833.0f, 379.0f, 960.0f };
  const VrInterfaceRect Info      = { 480.0f, 850.0f, 800.0f, 930.0f };
  const VrVector2       Screen    = { 1280.0f, 960.0f };

  VrControllerState controllerAt(const VrVector3& position) {
    VrControllerState controller;
    controller.isActive          = true;
    controller.gripPose.position = position;
    controller.aimPose.position  = position;
    return controller;
  }

  bool overlaps(const VrInterfaceRect& a, const VrInterfaceRect& b) {
    return a.left < b.right && b.left < a.right && a.top < b.bottom && b.top < a.bottom;
  }

}


TEST_CASE(interface_size_keeps_the_short_side) {
  VrVector2 headset = vrComputeInterfaceSize(2560, 1920);
  CHECK_NEAR(headset.x, 1280.0f, 1e-3f);
  CHECK_NEAR(headset.y,  960.0f, 1e-3f);

  VrVector2 wide = vrComputeInterfaceSize(1920, 1080);
  CHECK_NEAR(wide.x, 960.0f * 16.0f / 9.0f, 1e-2f);
  CHECK_NEAR(wide.y, 960.0f, 1e-3f);

  VrVector2 portrait = vrComputeInterfaceSize(1080, 1920);
  CHECK_NEAR(portrait.x, 1280.0f, 1e-3f);
  CHECK_NEAR(portrait.y, 1280.0f * 16.0f / 9.0f, 1e-2f);
}


TEST_CASE(hand_hud_crops_hold_each_element) {
  VrHandHudCrops crops = vrComputeHandHudCrops(HitPoints, Info, Screen);

  // The health bar's group with a margin, inside the screen
  CHECK_NEAR(crops.wrist.left,     0.0f, 1e-3f);
  CHECK_NEAR(crops.wrist.top,    821.0f, 1e-3f);
  CHECK_NEAR(crops.wrist.right,  391.0f, 1e-3f);
  CHECK_NEAR(crops.wrist.bottom, 960.0f, 1e-3f);

  // Centred on the prompt, from above its group to the bottom of the screen
  CHECK_NEAR(crops.prompt.left,   440.0f, 1e-3f);
  CHECK_NEAR(crops.prompt.right,  840.0f, 1e-3f);
  CHECK_NEAR(crops.prompt.top,    770.0f, 1e-3f);
  CHECK_NEAR(crops.prompt.bottom, 960.0f, 1e-3f);

  CHECK(!overlaps(crops.wrist, crops.prompt));
}


TEST_CASE(hand_hud_prompt_crop_avoids_the_health_bar) {
  // A wide prompt group would reach into the health bar's corner
  VrInterfaceRect wideInfo = { 300.0f, 850.0f, 980.0f, 930.0f };
  VrHandHudCrops crops = vrComputeHandHudCrops(HitPoints, wideInfo, Screen);

  CHECK(!overlaps(crops.wrist, crops.prompt));
  CHECK_NEAR(crops.prompt.left,  391.0f, 1e-3f);
  CHECK_NEAR(crops.prompt.right, 889.0f, 1e-3f);
  CHECK(!crops.prompt.isEmpty());
}


TEST_CASE(wrist_panel_lies_on_the_back_of_the_hand) {
  VrControllerState hand = controllerAt({ 0.0f, 1.0f, -0.3f });
  VrPose pose = vrComputeWristPanelPose(hand);

  // Right of, above and slightly behind the grip
  CHECK_NEAR(pose.position.x,  0.05f, 1e-5f);
  CHECK_NEAR(pose.position.y,  1.02f, 1e-5f);
  CHECK_NEAR(pose.position.z, -0.28f, 1e-5f);

  // Faces the hand's right axis raised by the face-up angle
  VrVector3 normal = vrRotate(pose.orientation, { 0.0f, 0.0f, 1.0f });
  CHECK_NEAR(normal.x, std::cos(VrHandHudLayout::WristFaceUpAngle), 1e-5f);
  CHECK_NEAR(normal.y, std::sin(VrHandHudLayout::WristFaceUpAngle), 1e-5f);
  CHECK_NEAR(normal.z, 0.0f, 1e-5f);

  // Its up axis runs towards the fingers
  VrVector3 up = vrRotate(pose.orientation, { 0.0f, 1.0f, 0.0f });
  CHECK_NEAR(up.x,  0.0f, 1e-5f);
  CHECK_NEAR(up.y,  0.0f, 1e-5f);
  CHECK_NEAR(up.z, -1.0f, 1e-5f);

  // Turning the hand turns the panel with it
  hand.aimPose.orientation = vrQuaternionFromAxisAngle({ 0.0f, 1.0f, 0.0f }, 1.0f);
  VrPose turned = vrComputeWristPanelPose(hand);
  VrVector3 turnedUp = vrRotate(turned.orientation, { 0.0f, 1.0f, 0.0f });
  VrVector3 forward  = vrRotate(hand.aimPose.orientation, { 0.0f, 0.0f, -1.0f });
  CHECK_NEAR(vrDot(turnedUp, forward), 1.0f, 1e-5f);
}


TEST_CASE(prompt_panel_faces_the_head_above_the_fingertip) {
  VrPose head = { { 0.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 1.6f, 0.0f } };
  VrControllerState hand = controllerAt({ 0.2f, 1.0f, -0.4f });

  VrPose pose = vrComputePromptPanelPose(hand, head);

  CHECK_NEAR(pose.position.x, 0.2f, 1e-5f);
  CHECK_NEAR(pose.position.y, 1.0f + VrHandHudLayout::PromptLift, 1e-5f);
  CHECK_NEAR(pose.position.z, -0.4f - VrHandHudLayout::FingertipOffset.x, 1e-5f);

  VrVector3 normal = vrRotate(pose.orientation, { 0.0f, 0.0f, 1.0f });
  VrVector3 toHead = head.position - pose.position;
  CHECK_NEAR(vrDot(normal, toHead), vrLength(toHead), 1e-4f);

  // Upright: its horizontal axis stays level
  VrVector3 right = vrRotate(pose.orientation, { 1.0f, 0.0f, 0.0f });
  CHECK_NEAR(right.y, 0.0f, 1e-5f);
}


TEST_CASE(wrist_gaze_needs_the_display_turned_towards_the_eyes) {
  VrPose head = { { 0.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 1.6f, 0.0f } };

  // In front of and below the eyes, the head tilted down to it
  head.orientation = vrQuaternionFromAxisAngle({ 1.0f, 0.0f, 0.0f }, -0.6f);

  VrPose facing;
  facing.position    = { 0.0f, 1.3f, -0.35f };
  facing.orientation = vrQuaternionFromAxisAngle({ 1.0f, 0.0f, 0.0f }, -0.6f);

  VrWristGaze gaze;
  CHECK(!gaze.update(&head, &facing, Frame));

  bool shown = false;

  for (float t = 0.0f; t < VrWristGaze::ShowDelay + Frame; t += Frame)
    shown = gaze.update(&head, &facing, Frame);

  CHECK(shown);

  // Turned away, as the back of the hand is while the weapon is aimed,
  // the display stays shown for a moment and then disappears
  VrPose away = facing;
  away.orientation = vrQuaternionFromAxisAngle({ 0.0f, 1.0f, 0.0f }, 2.0f);
  CHECK(gaze.update(&head, &away, Frame));

  for (float t = 0.0f; t < VrWristGaze::HideDelay + Frame; t += Frame)
    shown = gaze.update(&head, &away, Frame);

  CHECK(!shown);

  // A display turned away is never shown, nor an untracked hand
  VrWristGaze other;

  for (float t = 0.0f; t < 1.0f; t += Frame) {
    CHECK(!other.update(&head, &away, Frame));
    CHECK(!other.update(&head, nullptr, Frame));
  }
}


TEST_CASE(prompt_hand_is_the_nearest_with_a_margin) {
  std::array<VrControllerState, VrHandCount> hands = {
    controllerAt({ -0.3f, 1.0f, -0.3f }),
    controllerAt({  0.3f, 1.0f, -0.3f }) };

  VrPromptHand chooser;
  VrHand hand = VrHand::Right;

  // Target on the left
  CHECK(chooser.update(hands, { -1.0f, 1.0f, -1.0f }, hand));
  CHECK(hand == VrHand::Left);

  // Nearly in the middle: the prompt stays on the left hand
  CHECK(chooser.update(hands, { 0.02f, 1.0f, -2.0f }, hand));
  CHECK(hand == VrHand::Left);

  // Clearly on the right
  CHECK(chooser.update(hands, { 1.0f, 1.0f, -1.0f }, hand));
  CHECK(hand == VrHand::Right);

  // Only the tracked hand can show it
  hands[uint32_t(VrHand::Right)].isActive = false;
  CHECK(chooser.update(hands, { 1.0f, 1.0f, -1.0f }, hand));
  CHECK(hand == VrHand::Left);

  hands[uint32_t(VrHand::Left)].isActive = false;
  CHECK(!chooser.update(hands, { 1.0f, 1.0f, -1.0f }, hand));
}
