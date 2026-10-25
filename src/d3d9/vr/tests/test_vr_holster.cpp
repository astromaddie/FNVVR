#include "test.h"

#include "../vr_holster.h"

using namespace dxvk;

namespace {

  // Facing north (+Y) at 1.7 m, Z up
  VrGameCameraPose northHead() {
    VrGameCameraPose head;
    head.forward  = { 0.0f, 1.0f, 0.0f };
    head.up       = { 0.0f, 0.0f, 1.0f };
    head.right    = { 1.0f, 0.0f, 0.0f };
    head.position = { 0.0f, 0.0f, 1.7f * VrGameUnitsPerMetre };
    return head;
  }

  // A point east, north and up of the head, in metres
  VrVector3 fromHead(float east, float north, float up) {
    VrVector3 head = northHead().position;
    return { head.x + east * VrGameUnitsPerMetre, head.y + north * VrGameUnitsPerMetre,
      head.z + up * VrGameUnitsPerMetre };
  }

  VrHolsterInput atPoint(const VrVector3& hand, bool grip, VrHolsterKind kind, bool drawn) {
    VrHolsterInput input;
    input.active = true;
    input.head   = northHead();
    input.hand   = hand;
    input.grip   = grip;
    input.kind   = kind;
    input.drawn  = drawn;
    return input;
  }

}


TEST_CASE(holster_weapon_types_go_to_the_hip_or_the_back) {
  CHECK(vrHolsterForWeaponType(-1) == VrHolsterKind::None);
  CHECK(vrHolsterForWeaponType(0)  == VrHolsterKind::None);
  CHECK(vrHolsterForWeaponType(1)  == VrHolsterKind::Hip);
  CHECK(vrHolsterForWeaponType(2)  == VrHolsterKind::Back);
  CHECK(vrHolsterForWeaponType(3)  == VrHolsterKind::Hip);
  CHECK(vrHolsterForWeaponType(4)  == VrHolsterKind::Hip);

  for (int32_t type = 5; type <= 9; type++)
    CHECK(vrHolsterForWeaponType(type) == VrHolsterKind::Back);

  for (int32_t type = 10; type <= 13; type++)
    CHECK(vrHolsterForWeaponType(type) == VrHolsterKind::Hip);

  CHECK(vrHolsterForWeaponType(14) == VrHolsterKind::None);
}


TEST_CASE(holster_hip_is_right_and_well_below_the_head) {
  VrGameCameraPose head = northHead();
  VrHolsterZones zones;

  CHECK(vrIsAtHolster(head, fromHead(0.25f, 0.0f, -0.70f), VrHolsterKind::Hip, zones));
  // Left hip, in front of the face, at the shoulder
  CHECK(!vrIsAtHolster(head, fromHead(-0.25f, 0.0f, -0.70f), VrHolsterKind::Hip, zones));
  CHECK(!vrIsAtHolster(head, fromHead(0.25f, 0.50f, -0.70f), VrHolsterKind::Hip, zones));
  CHECK(!vrIsAtHolster(head, fromHead(0.25f, 0.0f, -0.20f), VrHolsterKind::Hip, zones));
  // The other holster is not the hip
  CHECK(!vrIsAtHolster(head, fromHead(0.25f, 0.0f, -0.70f), VrHolsterKind::Back, zones));
  CHECK(!vrIsAtHolster(head, fromHead(0.25f, 0.0f, -0.70f), VrHolsterKind::None, zones));
}


TEST_CASE(holster_back_is_behind_the_head_over_the_right_shoulder) {
  VrGameCameraPose head = northHead();
  VrHolsterZones zones;

  CHECK(vrIsAtHolster(head, fromHead(0.15f, -0.20f, -0.10f), VrHolsterKind::Back, zones));
  CHECK(vrIsAtHolster(head, fromHead(0.0f, -0.20f, 0.10f), VrHolsterKind::Back, zones));
  // Over the left shoulder is the magazine pouch
  CHECK(!vrIsAtHolster(head, fromHead(-0.15f, -0.20f, -0.10f), VrHolsterKind::Back, zones));
  // In front of the head, far below it, out of reach
  CHECK(!vrIsAtHolster(head, fromHead(0.15f, 0.10f, -0.10f), VrHolsterKind::Back, zones));
  CHECK(!vrIsAtHolster(head, fromHead(0.15f, -0.20f, -0.50f), VrHolsterKind::Back, zones));
  CHECK(!vrIsAtHolster(head, fromHead(0.50f, -0.50f, 0.0f), VrHolsterKind::Back, zones));
}


TEST_CASE(holster_follows_the_heading_when_looking_down) {
  // Looking straight down while facing east: forward is down, up is east
  VrGameCameraPose head = northHead();
  head.forward = { 0.0f, 0.0f, -1.0f };
  head.up      = { 1.0f, 0.0f, 0.0f };
  head.right   = { 0.0f, -1.0f, 0.0f };

  VrHolsterZones zones;

  // The right of a player facing east is south
  CHECK(vrIsAtHolster(head, fromHead(0.0f, -0.25f, -0.70f), VrHolsterKind::Hip, zones));
  CHECK(!vrIsAtHolster(head, fromHead(0.0f, 0.25f, -0.70f), VrHolsterKind::Hip, zones));
}


TEST_CASE(holster_press_at_the_holster_holsters_or_draws) {
  VrHolsterHand holster;
  VrVector3 hip  = fromHead(0.25f, 0.0f, -0.70f);
  VrVector3 away = fromHead(0.25f, 0.40f, -0.30f);

  CHECK(holster.update(atPoint(away, false, VrHolsterKind::Hip, true)) == VrHolsterEvent::None);
  CHECK(!holster.atHolster());
  CHECK(holster.update(atPoint(hip, false, VrHolsterKind::Hip, true)) == VrHolsterEvent::Entered);
  CHECK(holster.atHolster());
  CHECK(holster.update(atPoint(hip, false, VrHolsterKind::Hip, true)) == VrHolsterEvent::None);
  CHECK(holster.update(atPoint(hip, true, VrHolsterKind::Hip, true)) == VrHolsterEvent::Holstered);
  CHECK(holster.update(atPoint(hip, true, VrHolsterKind::Hip, false)) == VrHolsterEvent::None);
  CHECK(holster.update(atPoint(hip, false, VrHolsterKind::Hip, false)) == VrHolsterEvent::None);
  CHECK(holster.update(atPoint(hip, true, VrHolsterKind::Hip, false)) == VrHolsterEvent::Drawn);
}


TEST_CASE(holster_grip_held_before_reaching_does_nothing) {
  VrHolsterHand holster;
  VrVector3 hip  = fromHead(0.25f, 0.0f, -0.70f);
  VrVector3 away = fromHead(0.25f, 0.40f, -0.30f);

  holster.update(atPoint(away, true, VrHolsterKind::Hip, true));
  CHECK(holster.update(atPoint(hip, true, VrHolsterKind::Hip, true)) == VrHolsterEvent::Entered);
  CHECK(holster.update(atPoint(hip, true, VrHolsterKind::Hip, true)) == VrHolsterEvent::None);

  // A press away from the holster does nothing either
  holster.update(atPoint(away, false, VrHolsterKind::Hip, true));
  CHECK(holster.update(atPoint(away, true, VrHolsterKind::Hip, true)) == VrHolsterEvent::None);
}


TEST_CASE(holster_does_nothing_inactive_or_without_a_holster) {
  VrHolsterHand holster;
  VrVector3 hip = fromHead(0.25f, 0.0f, -0.70f);

  VrHolsterInput input = atPoint(hip, false, VrHolsterKind::Hip, true);
  input.active = false;
  CHECK(holster.update(input) == VrHolsterEvent::None);
  input.grip = true;
  CHECK(holster.update(input) == VrHolsterEvent::None);

  CHECK(holster.update(atPoint(hip, false, VrHolsterKind::None, true)) == VrHolsterEvent::None);
  CHECK(holster.update(atPoint(hip, true, VrHolsterKind::None, true)) == VrHolsterEvent::None);
  CHECK(!holster.atHolster());
}
