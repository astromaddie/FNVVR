#include "test.h"

#include "../vr_magazine.h"

using namespace dxvk;

namespace {

  constexpr float M = VrGameUnitsPerMetre;

  // Head at 1.7 m, looking north
  VrGameCameraPose northHead() {
    VrGameCameraPose pose;
    pose.forward  = { 0.0f, 1.0f, 0.0f };
    pose.up       = { 0.0f, 0.0f, 1.0f };
    pose.right    = { 1.0f, 0.0f, 0.0f };
    pose.position = { 0.0f, 0.0f, 1.7f * M };
    return pose;
  }

  // Over the left shoulder: 15 cm behind the head, 15 cm to its left
  const VrVector3 Pouch = { -0.15f * M, -0.15f * M, 1.6f * M };

  // Magazine well in front of the body
  const VrVector3 Well = { 0.0f, 0.4f * M, 1.2f * M };

  VrMagazineInput input(bool grip, const VrVector3& hand) {
    VrMagazineInput in;
    in.active     = true;
    in.grip       = grip;
    in.hand       = hand;
    in.well       = Well;
    in.head       = northHead();
    in.roundsHeld = true;
    return in;
  }

  VrVector3 nearWell(float metres) {
    return { Well.x + metres * M, Well.y, Well.z };
  }

}


TEST_CASE(magazine_parts_are_found_by_keyword) {
  CHECK(vrClassifyWeaponPart("##Clip") == VrWeaponPartKind::Magazine);
  CHECK(vrClassifyWeaponPart("##nmClip") == VrWeaponPartKind::Magazine);
  CHECK(vrClassifyWeaponPart("##SMGClip:0") == VrWeaponPartKind::Magazine);
  CHECK(vrClassifyWeaponPart("##Magazine") == VrWeaponPartKind::Magazine);
  CHECK(vrClassifyWeaponPart("##MicroFusionCell") == VrWeaponPartKind::Magazine);
  CHECK(vrClassifyWeaponPart("##PPEnergyCell") == VrWeaponPartKind::Magazine);
  CHECK(vrClassifyWeaponPart("##PLRBattery") == VrWeaponPartKind::Magazine);
  CHECK(vrClassifyWeaponPart("##CSDrum") == VrWeaponPartKind::Magazine);
}


TEST_CASE(parts_next_to_the_magazine_are_not_magazines) {
  CHECK(vrClassifyWeaponPart("##LR_CellRelease") == VrWeaponPartKind::None);
  CHECK(vrClassifyWeaponPart("##LR_CellReleaseLatch") == VrWeaponPartKind::None);
  CHECK(vrClassifyWeaponPart("##MagRelease") == VrWeaponPartKind::None);
  CHECK(vrClassifyWeaponPart("##ClipPeg") == VrWeaponPartKind::None);
  CHECK(vrClassifyWeaponPart("##MBatteryBay") == VrWeaponPartKind::None);
  CHECK(vrClassifyWeaponPart("##PowerCellAssembly") == VrWeaponPartKind::None);
  CHECK(vrClassifyWeaponPart("##MoonClip") == VrWeaponPartKind::None);
  CHECK(vrClassifyWeaponPart("##SRClipHigh") == VrWeaponPartKind::None);
  CHECK(vrClassifyWeaponPart("##TCECCell") == VrWeaponPartKind::None);
}


TEST_CASE(other_parts_are_not_magazines) {
  CHECK(vrClassifyWeaponPart("##Slide") == VrWeaponPartKind::None);
  CHECK(vrClassifyWeaponPart("##Cylinder") == VrWeaponPartKind::None);
  CHECK(vrClassifyWeaponPart("##Shells") == VrWeaponPartKind::None);
  CHECK(vrClassifyWeaponPart("Clip") == VrWeaponPartKind::None);
  CHECK(vrClassifyWeaponPart("ProjectileNode") == VrWeaponPartKind::None);
  CHECK(vrClassifyWeaponPart(nullptr) == VrWeaponPartKind::None);
}


TEST_CASE(pouch_is_behind_the_left_shoulder) {
  VrMagazinePouch pouch;
  VrGameCameraPose head = northHead();

  CHECK(vrIsAtMagazinePouch(head, Pouch, pouch));
  // In front of the head
  CHECK(!vrIsAtMagazinePouch(head, { -0.15f * M, 0.2f * M, 1.6f * M }, pouch));
  // Behind the right shoulder
  CHECK(!vrIsAtMagazinePouch(head, { 0.2f * M, -0.15f * M, 1.6f * M }, pouch));
  // At the hip, behind the back
  CHECK(!vrIsAtMagazinePouch(head, { -0.15f * M, -0.15f * M, 1.0f * M }, pouch));
  // Out of reach
  CHECK(!vrIsAtMagazinePouch(head, { -0.5f * M, -0.5f * M, 1.7f * M }, pouch));
}


TEST_CASE(pouch_follows_the_heading) {
  VrMagazinePouch pouch;
  VrGameCameraPose head = northHead();

  // Facing east, behind is west and left is north
  head.forward = { 1.0f, 0.0f, 0.0f };
  head.right   = { 0.0f, -1.0f, 0.0f };

  CHECK(vrIsAtMagazinePouch(head, { -0.15f * M, 0.15f * M, 1.6f * M }, pouch));
  CHECK(!vrIsAtMagazinePouch(head, Pouch, pouch));
}


TEST_CASE(pouch_works_looking_down) {
  VrMagazinePouch pouch;
  VrGameCameraPose head = northHead();

  // Looking straight down: the top of the head faces north
  head.forward = { 0.0f, 0.0f, -1.0f };
  head.up      = { 0.0f, 1.0f, 0.0f };

  CHECK(vrIsAtMagazinePouch(head, Pouch, pouch));
}


TEST_CASE(magazine_is_ejected_near_the_well) {
  VrMagazineHand magazine;

  // Too far, then a press near the well
  CHECK(magazine.update(input(true, nearWell(0.3f))) == VrMagazineEvent::None);
  CHECK(magazine.update(input(false, nearWell(0.05f))) == VrMagazineEvent::None);
  CHECK(magazine.update(input(true, nearWell(0.05f))) == VrMagazineEvent::Ejected);
  CHECK(magazine.state() == VrMagazineState::InHand);
}


TEST_CASE(holding_the_grip_does_not_eject) {
  VrMagazineHand magazine;

  CHECK(magazine.update(input(true, nearWell(0.3f))) == VrMagazineEvent::None);
  CHECK(magazine.update(input(true, nearWell(0.05f))) == VrMagazineEvent::None);
  CHECK(magazine.state() == VrMagazineState::InWeapon);
}


TEST_CASE(ejected_magazine_does_not_go_back_at_once) {
  VrMagazineHand magazine;

  CHECK(magazine.update(input(true, nearWell(0.0f))) == VrMagazineEvent::Ejected);
  CHECK(magazine.update(input(true, nearWell(0.0f))) == VrMagazineEvent::None);
  CHECK(magazine.update(input(true, nearWell(0.2f))) == VrMagazineEvent::None);
  CHECK(magazine.update(input(true, nearWell(0.05f))) == VrMagazineEvent::Inserted);
  CHECK(magazine.state() == VrMagazineState::InWeapon);
}


TEST_CASE(magazine_let_go_of_is_out) {
  VrMagazineHand magazine;

  CHECK(magazine.update(input(true, nearWell(0.0f))) == VrMagazineEvent::Ejected);
  CHECK(magazine.update(input(false, nearWell(0.3f))) == VrMagazineEvent::Dropped);
  CHECK(magazine.state() == VrMagazineState::Out);

  // Pressing at the well takes nothing: the weapon is empty
  CHECK(magazine.update(input(true, nearWell(0.0f))) == VrMagazineEvent::None);
  CHECK(magazine.state() == VrMagazineState::Out);
}


TEST_CASE(new_magazine_is_taken_behind_the_back) {
  VrMagazineHand magazine;

  magazine.update(input(true, nearWell(0.0f)));
  magazine.update(input(false, nearWell(0.3f)));

  CHECK(magazine.update(input(true, Pouch)) == VrMagazineEvent::Taken);
  CHECK(magazine.state() == VrMagazineState::InHand);
  CHECK(magazine.update(input(true, nearWell(0.03f))) == VrMagazineEvent::Inserted);
}


TEST_CASE(no_magazine_without_rounds) {
  VrMagazineHand magazine;

  magazine.update(input(true, nearWell(0.0f)));
  magazine.update(input(false, nearWell(0.3f)));

  VrMagazineInput empty = input(true, Pouch);
  empty.roundsHeld = false;

  CHECK(magazine.update(empty) == VrMagazineEvent::NoRounds);
  CHECK(magazine.state() == VrMagazineState::Out);
}


TEST_CASE(magazine_is_dropped_when_the_hand_is_busy) {
  VrMagazineHand magazine;

  magazine.update(input(true, nearWell(0.0f)));

  VrMagazineInput busy = input(true, nearWell(0.3f));
  busy.active = false;

  CHECK(magazine.update(busy) == VrMagazineEvent::Dropped);
  CHECK(magazine.state() == VrMagazineState::Out);
}


TEST_CASE(reset_puts_the_magazine_back) {
  VrMagazineHand magazine;

  magazine.update(input(true, nearWell(0.0f)));
  magazine.update(input(false, nearWell(0.3f)));
  magazine.reset();

  CHECK(magazine.state() == VrMagazineState::InWeapon);
}
