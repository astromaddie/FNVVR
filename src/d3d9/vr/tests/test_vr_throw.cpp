#include "test.h"

#include "../vr_math.h"
#include "../vr_throw.h"

using namespace dxvk;

namespace {

  constexpr float Frame = 1.0f / 90.0f;

  const VrVector3 Centre = { 0.0f, 1.0f, 0.0f };
  constexpr float Radius = 0.05f;

  // Within the reach of the bound, and well outside it
  const VrVector3 Near = { 0.10f, 1.0f, 0.0f };
  const VrVector3 Far  = { 0.50f, 1.0f, 0.0f };

}


TEST_CASE(throw_arming_follows_type_and_model) {
  CHECK(vrThrowArming(13, "Weapons\\1HandThrown\\ThrowingSpear.NIF") == VrThrowArming::None);
  CHECK(vrThrowArming(11, "Weapons\\1HandMine\\FragMine.NIF") == VrThrowArming::Button);
  CHECK(vrThrowArming(12, nullptr) == VrThrowArming::Button);
  CHECK(vrThrowArming(10, "Weapons\\1HandGrenade\\FragGrenade.NIF") == VrThrowArming::Pin);
  CHECK(vrThrowArming(10, "weapons/1handgrenade/DYNAMITE.nif") == VrThrowArming::Fuse);
  CHECK(vrThrowArming(10, "Weapons\\1HandGrenade\\PlasmaGrenade.NIF") == VrThrowArming::Button);
  CHECK(vrThrowArming(10, "Weapons\\1HandGrenade\\NukaGrenade.NIF") == VrThrowArming::None);
  CHECK(vrThrowArming(10, nullptr) == VrThrowArming::None);
  // Only the file name counts, not the folders
  CHECK(vrThrowArming(10, "Frag\\Other.NIF") == VrThrowArming::None);
  // Weapons that are not thrown
  CHECK(vrThrowArming(3, "Weapons\\FragPistol.NIF") == VrThrowArming::None);
}


TEST_CASE(throw_arm_nothing_to_act_on_is_armed) {
  VrThrowArm arm;
  arm.reset(VrThrowArming::None);
  CHECK(arm.armed());
}


TEST_CASE(throw_arm_pin_is_pulled_out) {
  VrThrowConfig config;
  VrThrowArm arm;
  arm.reset(VrThrowArming::Pin);
  CHECK(!arm.armed());

  // A grip already held when the grenade came does not take the pin
  CHECK(arm.update(Near, Centre, Radius, true, false, Frame, config) == VrArmEvent::None);
  CHECK(arm.update(Near, Centre, Radius, false, false, Frame, config) == VrArmEvent::None);

  // Gripped away from the grenade, nothing
  CHECK(arm.update(Far, Centre, Radius, true, false, Frame, config) == VrArmEvent::None);
  CHECK(arm.update(Far, Centre, Radius, false, false, Frame, config) == VrArmEvent::None);

  CHECK(arm.update(Near, Centre, Radius, true, false, Frame, config) == VrArmEvent::PinTaken);
  CHECK(arm.busy());

  // Not out yet just past the reach
  VrVector3 justOut = { Radius + config.reach + config.pinPull * 0.5f, 1.0f, 0.0f };
  CHECK(arm.update(justOut, Centre, Radius, true, false, Frame, config) == VrArmEvent::None);
  CHECK(!arm.armed());

  CHECK(arm.update(Far, Centre, Radius, true, false, Frame, config) == VrArmEvent::Armed);
  CHECK(arm.armed());
  CHECK(!arm.busy());
}


TEST_CASE(throw_arm_pin_let_go_stays_in) {
  VrThrowConfig config;
  VrThrowArm arm;
  arm.reset(VrThrowArming::Pin);

  arm.update(Near, Centre, Radius, false, false, Frame, config);
  CHECK(arm.update(Near, Centre, Radius, true, false, Frame, config) == VrArmEvent::PinTaken);
  CHECK(arm.update(Near, Centre, Radius, false, false, Frame, config) == VrArmEvent::PinDropped);
  CHECK(arm.update(Far, Centre, Radius, false, false, Frame, config) == VrArmEvent::None);
  CHECK(!arm.armed());
}


TEST_CASE(throw_arm_fuse_is_lit_after_a_moment) {
  VrThrowConfig config;
  VrThrowArm arm;
  arm.reset(VrThrowArming::Fuse);

  arm.update(Near, Centre, Radius, false, false, Frame, config);
  CHECK(arm.update(Near, Centre, Radius, false, true, Frame, config) == VrArmEvent::Lighting);
  CHECK(arm.busy());

  // Moved away, the count starts over
  arm.update(Far, Centre, Radius, false, true, Frame, config);
  CHECK(!arm.busy());

  CHECK(arm.update(Near, Centre, Radius, false, true, Frame, config) == VrArmEvent::Lighting);

  uint32_t frames = 0;
  VrArmEvent event = VrArmEvent::None;

  while (event != VrArmEvent::Armed && frames < 100) {
    event = arm.update(Near, Centre, Radius, false, true, Frame, config);
    frames++;
  }

  CHECK(arm.armed());
  // 0.3 s at 90 Hz
  CHECK(frames >= 26 && frames <= 28);
}


TEST_CASE(throw_arm_button_is_clicked_at_the_device) {
  VrThrowConfig config;
  VrThrowArm arm;
  arm.reset(VrThrowArming::Button);

  // A trigger held from before does not press it
  CHECK(arm.update(Near, Centre, Radius, false, true, Frame, config) == VrArmEvent::None);
  arm.update(Near, Centre, Radius, false, false, Frame, config);

  CHECK(arm.update(Far, Centre, Radius, false, true, Frame, config) == VrArmEvent::None);
  arm.update(Far, Centre, Radius, false, false, Frame, config);
  CHECK(!arm.armed());

  CHECK(arm.update(Near, Centre, Radius, false, true, Frame, config) == VrArmEvent::Armed);
  CHECK(arm.armed());
}


TEST_CASE(throw_tracker_measures_the_last_motion) {
  VrThrowTracker tracker;
  CHECK(vrLength(tracker.velocity(0.05f)) == 0.0f);

  // Still, then 6 m/s forward for the last frames
  VrVector3 position = { 0.0f, 0.0f, 0.0f };

  for (uint32_t i = 0; i < 10; i++)
    tracker.add(position, Frame);

  for (uint32_t i = 0; i < 8; i++) {
    position.x += 6.0f * Frame;
    tracker.add(position, Frame);
  }

  VrVector3 v = tracker.velocity(0.05f);
  CHECK_NEAR(v.x, 6.0f, 1e-3);
  CHECK_NEAR(v.y, 0.0f, 1e-6);

  // A window shorter than a frame uses one frame
  CHECK_NEAR(tracker.velocity(0.0f).x, 6.0f, 1e-3);

  tracker.reset();
  CHECK(vrLength(tracker.velocity(0.05f)) == 0.0f);
}


TEST_CASE(throw_speed_scales_the_hand) {
  VrThrowConfig config;
  CHECK(vrComputeThrowSpeed(1.0f, config) == 0.0f);
  CHECK_NEAR(vrComputeThrowSpeed(8.0f, config), 12.0f, 1e-5);
}
