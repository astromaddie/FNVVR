#include "test.h"

#include "../vr_sneak_rules.h"

using namespace dxvk;

namespace {

  constexpr float Frame = 1.0f / 90.0f;

}


TEST_CASE(crouch_starts_at_the_depth_and_ends_above_it) {
  VrPhysicalCrouch crouch((VrCrouchConfig()));

  CHECK(!crouch.update(1.65f, Frame));
  CHECK(!crouch.update(1.40f, Frame));
  CHECK(crouch.update(1.29f, Frame));

  // Between the two thresholds the crouch is kept
  CHECK(crouch.update(1.38f, Frame));
  CHECK(!crouch.update(1.41f, Frame));
  CHECK(!crouch.update(1.38f, Frame));
}


TEST_CASE(crouch_depth_is_measured_from_the_standing_height_not_the_floor) {
  VrCrouchConfig config;
  config.depth = 0.4f;
  VrPhysicalCrouch crouch(config);

  // A tracking floor half a metre off
  CHECK(!crouch.update(2.15f, Frame));
  CHECK(!crouch.update(1.80f, Frame));
  CHECK(crouch.update(1.74f, Frame));
}


TEST_CASE(crouch_standing_height_follows_the_head_up_slowly) {
  VrPhysicalCrouch crouch((VrCrouchConfig()));

  // Taken seated, then the player stands up
  crouch.update(1.20f, Frame);

  for (int i = 0; i < 90; i++)
    crouch.update(1.65f, Frame);

  CHECK_NEAR(crouch.standingHeight(), 1.20f + VrPhysicalCrouch::StandingRiseSpeed, 1e-4);
  CHECK(!crouch.crouched());

  for (int i = 0; i < 900; i++)
    crouch.update(1.65f, Frame);

  CHECK_NEAR(crouch.standingHeight(), 1.65f, 1e-5);

  // A short jump barely raises it
  for (int i = 0; i < 27; i++)
    crouch.update(1.85f, Frame);

  CHECK(crouch.standingHeight() < 1.69f);
}


TEST_CASE(crouch_standing_height_never_follows_the_head_down) {
  VrPhysicalCrouch crouch((VrCrouchConfig()));
  crouch.update(1.65f, Frame);

  for (int i = 0; i < 90 * 60; i++)
    crouch.update(1.10f, Frame);

  CHECK_NEAR(crouch.standingHeight(), 1.65f, 1e-6);
  CHECK(crouch.crouched());

  crouch.reset();
  CHECK(!crouch.update(1.10f, Frame));
  CHECK_NEAR(crouch.standingHeight(), 1.10f, 1e-6);
}


TEST_CASE(sneak_toggle_presses_once_then_waits) {
  VrSneakToggle toggle;

  CHECK(!toggle.update(false, false, true, Frame));

  // Held for the press time
  int held = 0;

  while (toggle.update(true, false, true, Frame))
    held++;

  CHECK(held >= int(VrSneakToggle::PressTime / Frame) - 1);
  CHECK(held <= int(VrSneakToggle::PressTime / Frame) + 1);

  // Released while the game settles, even if it did not toggle
  int released = 1;

  while (!toggle.update(true, false, true, Frame) && released < 1000)
    released++;

  CHECK(released >= int(VrSneakToggle::SettleTime / Frame) - 1);
  CHECK(released <= int(VrSneakToggle::SettleTime / Frame) + 2);
}


TEST_CASE(sneak_toggle_stops_once_the_game_sneaks_as_wanted) {
  VrSneakToggle toggle;
  CHECK(toggle.update(true, false, true, Frame));

  for (int i = 0; i < 90; i++)
    toggle.update(true, true, true, Frame);

  CHECK(!toggle.update(true, true, true, Frame));
  CHECK(!toggle.update(false, false, true, Frame));
}


TEST_CASE(sneak_toggle_does_not_press_while_not_allowed) {
  VrSneakToggle toggle;
  CHECK(!toggle.update(true, false, false, Frame));

  // A press is cut short
  CHECK(toggle.update(true, false, true, Frame));
  CHECK(!toggle.update(true, false, false, Frame));
}


TEST_CASE(sneak_camera_cancels_the_game_drop_while_sneaking) {
  VrSneakCamera camera;
  float standing = 60.0f;

  for (int i = 0; i < 900; i++)
    CHECK_NEAR(camera.update(&standing, false, Frame), 0.0f, 1e-6);

  // The game lowers its camera over a few frames
  for (float height : { 58.0f, 52.0f, 45.0f, 40.0f }) {
    float raise = camera.update(&height, true, Frame);
    CHECK_NEAR(height + raise, standing, 1e-3);
  }

  // Stopped sneaking: still raised while the camera comes back up
  for (float height : { 42.0f, 50.0f, 57.0f }) {
    float raise = camera.update(&height, false, Frame);
    CHECK_NEAR(height + raise, standing, 1e-3);
  }

  float up = 59.9f;
  CHECK_NEAR(camera.update(&up, false, Frame), 0.0f, 1e-6);
}


TEST_CASE(sneak_camera_learns_nothing_while_sneaking_or_away) {
  VrSneakCamera camera;
  float standing = 60.0f;
  float low      = 40.0f;
  float seated   = 30.0f;

  camera.update(&standing, false, Frame);

  for (int i = 0; i < 900; i++)
    camera.update(&low, true, Frame);

  CHECK_NEAR(camera.raise(), 20.0f, 1e-4);

  // Not following the player: nothing learned, the raise is kept
  CHECK_NEAR(camera.update(nullptr, false, Frame), 20.0f, 1e-4);
  CHECK_NEAR(camera.update(&seated, true, Frame), 30.0f, 1e-4);

  // The raise is limited
  float floor = 0.0f;
  CHECK_NEAR(camera.update(&floor, true, Frame), VrSneakCamera::MaxRaise, 1e-6);
}


TEST_CASE(sneak_camera_raises_nothing_without_a_standing_height) {
  VrSneakCamera camera;
  float low = 40.0f;

  CHECK_NEAR(camera.update(&low, true, Frame), 0.0f, 1e-6);

  // The rise ends after the longest wait if the camera stays low
  for (int i = 0; i < 200; i++)
    camera.update(&low, false, Frame);

  CHECK_NEAR(camera.raise(), 0.0f, 1e-6);
}
