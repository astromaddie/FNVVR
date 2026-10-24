#include "test.h"

#include "../vr_math.h"
#include "../vr_throw.h"

using namespace dxvk;

namespace {

  constexpr float Frame = 1.0f / 90.0f;

  const VrVector3 Centre = { 0.0f, 1.0f, 0.0f };
  constexpr float Radius = 0.02f;

  // Within every reach of the part, and well outside it
  const VrVector3 Near = { 0.03f, 1.0f, 0.0f };
  const VrVector3 Far  = { 0.50f, 1.0f, 0.0f };

  VrThrowArmInput armInput(const VrVector3& hand, bool grip, bool trigger) {
    VrThrowArmInput input;
    input.hand      = hand;
    input.fingertip = Far;
    input.flame     = Far;
    input.target    = Centre;
    input.radius    = Radius;
    input.grip      = grip;
    input.trigger   = trigger;
    input.dt        = Frame;
    return input;
  }

  VrThrowArmInput flameInput(const VrVector3& flame, bool trigger) {
    VrThrowArmInput input = armInput(Far, false, trigger);
    input.flame = flame;
    return input;
  }

  VrThrowArmInput fingertipInput(const VrVector3& fingertip) {
    VrThrowArmInput input = armInput(Far, false, false);
    input.fingertip = fingertip;
    return input;
  }

}


TEST_CASE(throw_parts_are_told_by_name) {
  // Grenade pins, not words that hold "pin"
  CHECK(vrThrowPartKind("##NGPin") == VrThrowPart::Pin);
  CHECK(vrThrowPartKind("##NGPin:0") == VrThrowPart::Pin);
  CHECK(vrThrowPartKind("GrenadePin01") == VrThrowPart::Pin);
  CHECK(vrThrowPartKind("Spine") == VrThrowPart::None);
  CHECK(vrThrowPartKind("Pinecone") == VrThrowPart::None);

  // The lighter, its flame, and its switch which is not a button
  CHECK(vrThrowPartKind("##Lighter") == VrThrowPart::Lighter);
  CHECK(vrThrowPartKind("##Lighter:0") == VrThrowPart::Lighter);
  CHECK(vrThrowPartKind("LighterFlame01") == VrThrowPart::Flame);
  CHECK(vrThrowPartKind("##LighterSwitch") == VrThrowPart::None);

  // Buttons and lights
  CHECK(vrThrowPartKind("##BCMSwitch") == VrThrowPart::Button);
  CHECK(vrThrowPartKind("##Dial") == VrThrowPart::Button);
  CHECK(vrThrowPartKind("##NGPlunger:0") == VrThrowPart::Button);
  CHECK(vrThrowPartKind("##LightOn") == VrThrowPart::LightOn);
  CHECK(vrThrowPartKind("#LightOff:0") == VrThrowPart::LightOff);

  CHECK(vrThrowPartKind("Dynamite:0") == VrThrowPart::None);
  CHECK(vrThrowPartKind("###") == VrThrowPart::None);
  CHECK(vrThrowPartKind(nullptr) == VrThrowPart::None);
}


TEST_CASE(throw_arming_follows_type_parts_and_model) {
  VrThrowParts none;
  VrThrowParts pin;
  pin.pin = true;
  pin.button = true;
  VrThrowParts lighter;
  lighter.lighter = true;
  VrThrowParts button;
  button.button = true;

  // The model's parts decide first
  CHECK(vrThrowArming(10, "Weapons\\NukaGrenade.NIF", pin) == VrThrowArming::Pin);
  CHECK(vrThrowArming(10, "Weapons\\firebomb.NIF", lighter) == VrThrowArming::Fuse);
  CHECK(vrThrowArming(10, "Weapons\\Something.NIF", button) == VrThrowArming::Button);
  CHECK(vrThrowArming(12, "Weapons\\TimeBomb.NIF", button) == VrThrowArming::Button);

  // Mines without a button are pressed anywhere
  CHECK(vrThrowArming(11, "Weapons\\1HandMine\\FragMine.NIF", none) == VrThrowArming::Button);
  CHECK(vrThrowArming(12, nullptr, none) == VrThrowArming::Button);

  // Grenades without parts, by file name only
  CHECK(vrThrowArming(10, "Weapons\\1HandGrenade\\FragGrenade.NIF", none) == VrThrowArming::Pin);
  CHECK(vrThrowArming(10, "weapons/1handgrenade/DYNAMITE.nif", none) == VrThrowArming::Fuse);
  CHECK(vrThrowArming(10, "Weapons\\1HandGrenade\\PlasmaGrenade.NIF", none) == VrThrowArming::Button);
  CHECK(vrThrowArming(10, "Weapons\\1HandGrenade\\NukaGrenade.NIF", none) == VrThrowArming::None);
  CHECK(vrThrowArming(10, nullptr, none) == VrThrowArming::None);
  CHECK(vrThrowArming(10, "Frag\\Other.NIF", none) == VrThrowArming::None);

  // Thrown weapons and weapons that are not thrown are never armed
  CHECK(vrThrowArming(13, "Weapons\\1HandThrown\\ThrowingSpear.NIF", pin) == VrThrowArming::None);
  CHECK(vrThrowArming(3, "Weapons\\FragPistol.NIF", none) == VrThrowArming::None);
}


TEST_CASE(throw_mines_are_placed) {
  CHECK(vrThrowIsPlaced(11));
  CHECK(vrThrowIsPlaced(12));
  CHECK(!vrThrowIsPlaced(10));
  CHECK(!vrThrowIsPlaced(13));
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
  CHECK(arm.update(armInput(Near, true, false), config) == VrArmEvent::None);
  CHECK(arm.update(armInput(Near, false, false), config) == VrArmEvent::None);

  // Gripped away from the pin, nothing
  CHECK(arm.update(armInput(Far, true, false), config) == VrArmEvent::None);
  CHECK(arm.update(armInput(Far, false, false), config) == VrArmEvent::None);

  CHECK(arm.update(armInput(Near, true, false), config) == VrArmEvent::PinTaken);
  CHECK(arm.holdsPin());
  CHECK(arm.busy());

  // Not out yet halfway
  VrVector3 halfway = { Near.x + config.pinPull * 0.5f, 1.0f, 0.0f };
  CHECK(arm.update(armInput(halfway, true, false), config) == VrArmEvent::None);
  CHECK(!arm.armed());

  VrVector3 out = { Near.x + config.pinPull * 1.1f, 1.0f, 0.0f };
  CHECK(arm.update(armInput(out, true, false), config) == VrArmEvent::Armed);
  CHECK(arm.armed());

  // The pin stays in the hand until the grip lets it go
  CHECK(arm.holdsPin());
  arm.update(armInput(Far, false, false), config);
  CHECK(!arm.holdsPin());
  CHECK(!arm.busy());
}


TEST_CASE(throw_arm_pin_let_go_stays_in) {
  VrThrowConfig config;
  VrThrowArm arm;
  arm.reset(VrThrowArming::Pin);

  arm.update(armInput(Near, false, false), config);
  CHECK(arm.update(armInput(Near, true, false), config) == VrArmEvent::PinTaken);
  CHECK(arm.update(armInput(Near, false, false), config) == VrArmEvent::PinDropped);
  CHECK(arm.update(armInput(Far, false, false), config) == VrArmEvent::None);
  CHECK(!arm.armed());
  CHECK(!arm.holdsPin());
}


TEST_CASE(throw_arm_pin_is_pinched_with_the_trigger) {
  VrThrowConfig config;
  VrThrowArm arm;
  arm.reset(VrThrowArming::Pin);

  arm.update(armInput(Near, false, false), config);
  CHECK(arm.update(armInput(Near, false, true), config) == VrArmEvent::PinTaken);

  VrVector3 out = { Near.x + config.pinPull * 1.1f, 1.0f, 0.0f };
  CHECK(arm.update(armInput(out, false, true), config) == VrArmEvent::Armed);
  CHECK(arm.holdsPin());

  arm.update(armInput(out, false, false), config);
  CHECK(!arm.holdsPin());
}


TEST_CASE(throw_arm_fuse_is_lit_by_the_flame) {
  VrThrowConfig config;
  VrThrowArm arm;
  arm.reset(VrThrowArming::Fuse);

  // The hand at the fuse does not light it, the flame does
  VrThrowArmInput handOnly = armInput(Centre, false, true);
  arm.update(armInput(Far, false, false), config);
  CHECK(arm.update(handOnly, config) == VrArmEvent::None);

  CHECK(arm.update(flameInput(Near, true), config) == VrArmEvent::Lighting);
  CHECK(arm.busy());

  // Moved away, the count starts over
  arm.update(flameInput(Far, true), config);
  CHECK(!arm.busy());

  CHECK(arm.update(flameInput(Near, true), config) == VrArmEvent::Lighting);

  uint32_t frames = 0;
  VrArmEvent event = VrArmEvent::None;

  while (event != VrArmEvent::Armed && frames < 100) {
    event = arm.update(flameInput(Near, true), config);
    frames++;
  }

  CHECK(arm.armed());
  // 0.3 s at 90 Hz
  CHECK(frames >= 26 && frames <= 28);
}


TEST_CASE(throw_arm_button_is_pressed_by_the_fingertip) {
  VrThrowConfig config;
  VrThrowArm arm;
  arm.reset(VrThrowArming::Button);

  CHECK(arm.update(fingertipInput(Far), config) == VrArmEvent::None);
  CHECK(!arm.armed());

  CHECK(arm.update(fingertipInput(Near), config) == VrArmEvent::Armed);
  CHECK(arm.armed());
}


TEST_CASE(throw_arm_button_is_clicked_at_the_device) {
  VrThrowConfig config;
  VrThrowArm arm;
  arm.reset(VrThrowArming::Button);

  // A trigger held from before does not press it
  CHECK(arm.update(armInput(Near, false, true), config) == VrArmEvent::None);
  arm.update(armInput(Near, false, false), config);

  CHECK(arm.update(armInput(Far, false, true), config) == VrArmEvent::None);
  arm.update(armInput(Far, false, false), config);
  CHECK(!arm.armed());

  CHECK(arm.update(armInput(Near, false, true), config) == VrArmEvent::Armed);
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


TEST_CASE(throw_tracker_finds_the_fastest_motion_before_the_release) {
  VrThrowTracker tracker;
  CHECK(vrLength(tracker.peakVelocity(0.05f, 0.15f)) == 0.0f);

  // Still, 6 m/s forward, then the follow-through: 2 m/s down for 0.1 s
  VrVector3 position = { 0.0f, 0.0f, 0.0f };

  for (uint32_t i = 0; i < 10; i++)
    tracker.add(position, Frame);

  for (uint32_t i = 0; i < 8; i++) {
    position.x += 6.0f * Frame;
    tracker.add(position, Frame);
  }

  for (uint32_t i = 0; i < 9; i++) {
    position.y -= 2.0f * Frame;
    tracker.add(position, Frame);
  }

  // When the trigger lets go the hand comes down
  VrVector3 last = tracker.velocity(0.05f);
  CHECK_NEAR(last.x, 0.0f, 1e-3);
  CHECK_NEAR(last.y, -2.0f, 1e-3);

  // The throw was forward
  VrVector3 peak = tracker.peakVelocity(0.05f, 0.15f);
  CHECK_NEAR(peak.x, 6.0f, 1e-3);
  CHECK_NEAR(peak.y, 0.0f, 1e-3);

  // Too long ago to count
  VrVector3 recent = tracker.peakVelocity(0.05f, 0.03f);
  CHECK_NEAR(recent.x, 0.0f, 1e-3);
}


TEST_CASE(throw_share_grows_with_the_hand) {
  VrThrowConfig config;
  CHECK(vrComputeThrowShare(1.0f, config) == 0.0f);
  CHECK_NEAR(vrComputeThrowShare(3.0f, config), 0.5f, 1e-5);
  CHECK_NEAR(vrComputeThrowShare(6.0f, config), 1.0f, 1e-5);
  CHECK_NEAR(vrComputeThrowShare(9.0f, config), 1.0f, 1e-5);
}
