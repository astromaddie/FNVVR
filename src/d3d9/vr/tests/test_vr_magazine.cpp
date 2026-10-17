#include <cmath>
#include <vector>

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

  // Magazine well in front of the body: the magazine comes out downwards
  const VrVector3 Well = { 0.0f, 0.4f * M, 1.2f * M };
  const VrVector3 Down = { 0.0f, 0.0f, -1.0f };

  // The magazine is held at the hand
  VrMagazineInput input(bool grip, const VrVector3& hand) {
    VrMagazineInput in;
    in.active     = true;
    in.grip       = grip;
    in.hand       = hand;
    in.held       = hand;
    in.well       = Well;
    in.axis       = Down;
    in.head       = northHead();
    in.roundsHeld = true;
    return in;
  }

  // A point beside the well, or below it along the way out
  VrVector3 nearWell(float metres) {
    return { Well.x + metres * M, Well.y, Well.z };
  }

  VrVector3 belowWell(float metres) {
    return { Well.x, Well.y, Well.z - metres * M };
  }

  // Takes the magazine out and lets go of it
  void emptyWeapon(VrMagazineHand& magazine) {
    magazine.update(input(true, nearWell(0.0f)));
    magazine.update(input(true, belowWell(0.1f)));
    magazine.update(input(false, belowWell(0.3f)));
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
  CHECK(magazine.state() == VrMagazineState::Seating);
  CHECK(magazine.seat() == 0.0f);
}


TEST_CASE(magazine_is_ejected_from_below) {
  VrMagazineHand magazine;

  // The left hand cannot come near a pistol's magazine, inside the right
  // hand: it takes it from below, along the way out of the well
  VrVector3 below = belowWell(0.14f);
  below.x += 0.1f * M;
  CHECK(magazine.update(input(true, below)) == VrMagazineEvent::Ejected);

  VrMagazineHand far;
  CHECK(far.update(input(true, belowWell(0.3f))) == VrMagazineEvent::None);
}


TEST_CASE(holding_the_grip_does_not_eject) {
  VrMagazineHand magazine;

  CHECK(magazine.update(input(true, nearWell(0.3f))) == VrMagazineEvent::None);
  CHECK(magazine.update(input(true, nearWell(0.05f))) == VrMagazineEvent::None);
  CHECK(magazine.state() == VrMagazineState::InWeapon);
}


TEST_CASE(ejected_magazine_slides_out_of_its_well) {
  VrMagazineHand magazine;

  // Taken 5 cm beside the magazine: the hand moves it from there
  VrVector3 hand = nearWell(0.05f);
  CHECK(magazine.update(input(true, hand)) == VrMagazineEvent::Ejected);

  hand.z -= 0.02f * M;
  CHECK(magazine.update(input(true, hand)) == VrMagazineEvent::None);
  CHECK(std::abs(magazine.seat() - 0.02f) < 1e-4f);

  // Pushing up does not go past its place
  hand.z += 0.01f * M;
  CHECK(magazine.update(input(true, hand)) == VrMagazineEvent::None);
  CHECK(std::abs(magazine.seat() - 0.01f) < 1e-4f);

  hand.z -= 0.05f * M;
  CHECK(magazine.update(input(true, hand)) == VrMagazineEvent::Cleared);
  CHECK(magazine.state() == VrMagazineState::InHand);
}


TEST_CASE(magazine_comes_out_sideways) {
  VrMagazineHand magazine;

  CHECK(magazine.update(input(true, nearWell(0.0f))) == VrMagazineEvent::Ejected);
  CHECK(magazine.update(input(true, nearWell(0.05f))) == VrMagazineEvent::None);
  CHECK(magazine.update(input(true, nearWell(0.07f))) == VrMagazineEvent::Cleared);
}


TEST_CASE(ejected_magazine_pushed_back_goes_in) {
  VrMagazineHand magazine;

  // Pushed back before it moved, it stays where it is
  CHECK(magazine.update(input(true, nearWell(0.0f))) == VrMagazineEvent::Ejected);
  CHECK(magazine.update(input(true, belowWell(-0.01f))) == VrMagazineEvent::None);

  CHECK(magazine.update(input(true, belowWell(0.02f))) == VrMagazineEvent::None);
  CHECK(magazine.update(input(true, belowWell(0.0f))) == VrMagazineEvent::Inserted);
  CHECK(magazine.state() == VrMagazineState::InWeapon);
}


TEST_CASE(magazine_let_go_of_is_out) {
  VrMagazineHand magazine;

  CHECK(magazine.update(input(true, nearWell(0.0f))) == VrMagazineEvent::Ejected);
  CHECK(magazine.update(input(false, belowWell(0.01f))) == VrMagazineEvent::Dropped);
  CHECK(magazine.state() == VrMagazineState::Out);

  // Pressing at the well takes nothing: the weapon is empty
  CHECK(magazine.update(input(true, nearWell(0.0f))) == VrMagazineEvent::None);
  CHECK(magazine.state() == VrMagazineState::Out);
}


TEST_CASE(new_magazine_is_taken_behind_the_back) {
  VrMagazineHand magazine;
  emptyWeapon(magazine);

  CHECK(magazine.update(input(true, Pouch)) == VrMagazineEvent::Taken);
  CHECK(magazine.state() == VrMagazineState::InHand);

  // Brought from below: it enters the well at its mouth, 3 cm out
  CHECK(magazine.update(input(true, belowWell(0.2f))) == VrMagazineEvent::None);
  CHECK(magazine.update(input(true, belowWell(0.06f))) == VrMagazineEvent::Engaged);
  CHECK(magazine.state() == VrMagazineState::Seating);
  CHECK(std::abs(magazine.seat() - 0.03f) < 1e-4f);

  CHECK(magazine.update(input(true, belowWell(0.045f))) == VrMagazineEvent::None);
  CHECK(std::abs(magazine.seat() - 0.015f) < 1e-4f);
  CHECK(magazine.update(input(true, belowWell(0.03f))) == VrMagazineEvent::Inserted);
  CHECK(magazine.state() == VrMagazineState::InWeapon);
}


TEST_CASE(magazine_pulled_out_of_the_well_does_not_enter_at_once) {
  VrMagazineHand magazine;
  emptyWeapon(magazine);

  magazine.update(input(true, Pouch));
  CHECK(magazine.update(input(true, belowWell(0.04f))) == VrMagazineEvent::Engaged);
  CHECK(magazine.update(input(true, belowWell(0.1f))) == VrMagazineEvent::Cleared);

  // Back near the mouth: it must move away from the way in first
  CHECK(magazine.update(input(true, belowWell(0.05f))) == VrMagazineEvent::None);
  CHECK(magazine.update(input(true, belowWell(0.2f))) == VrMagazineEvent::None);
  CHECK(magazine.update(input(true, belowWell(0.05f))) == VrMagazineEvent::None);
  CHECK(magazine.update(input(true, belowWell(0.25f))) == VrMagazineEvent::None);
  CHECK(magazine.update(input(true, belowWell(0.05f))) == VrMagazineEvent::Engaged);
}


TEST_CASE(magazine_enters_from_the_way_in) {
  VrMagazineHand magazine;
  emptyWeapon(magazine);
  magazine.update(input(true, Pouch));

  // 10 cm below the mouth and 5 cm aside, it enters at the mouth, and the
  // hand pushes it in from there
  VrVector3 hand = belowWell(0.13f);
  hand.x += 0.05f * M;
  CHECK(magazine.update(input(true, hand)) == VrMagazineEvent::Engaged);
  CHECK(std::abs(magazine.seat() - 0.03f) < 1e-4f);

  hand.z += 0.03f * M;
  CHECK(magazine.update(input(true, hand)) == VrMagazineEvent::Inserted);
}


TEST_CASE(no_magazine_without_rounds) {
  VrMagazineHand magazine;
  emptyWeapon(magazine);

  VrMagazineInput empty = input(true, Pouch);
  empty.roundsHeld = false;

  CHECK(magazine.update(empty) == VrMagazineEvent::NoRounds);
  CHECK(magazine.state() == VrMagazineState::Out);
}


TEST_CASE(magazine_is_dropped_when_the_hand_is_busy) {
  VrMagazineHand magazine;

  magazine.update(input(true, nearWell(0.0f)));

  VrMagazineInput busy = input(true, belowWell(0.01f));
  busy.active = false;

  CHECK(magazine.update(busy) == VrMagazineEvent::Dropped);
  CHECK(magazine.state() == VrMagazineState::Out);
}


TEST_CASE(reset_puts_the_magazine_back) {
  VrMagazineHand magazine;
  emptyWeapon(magazine);
  magazine.reset();

  CHECK(magazine.state() == VrMagazineState::InWeapon);
}


TEST_CASE(magazine_timing_follows_the_animation) {
  // Out at 0.5 s, dropped 20 cm, a new one held 6 cm out from 1.0 s,
  // pushed in at 1.3 s, along the weapon's -z
  std::vector<VrMagazineSample> samples;

  for (int32_t i = 0; i <= 100; i++) {
    float t = float(i) * 0.02f;
    float out = 0.0f;

    if (t >= 0.5f && t < 0.8f)
      out = (t - 0.48f) * 0.6f;
    else if (t >= 0.8f && t < 1.2f)
      out = 0.06f;
    else if (t >= 1.2f && t < 1.3f)
      out = 0.06f * (1.3f - t) / 0.1f;

    samples.push_back({ t, { 0.0f, 0.0f, -out } });
  }

  VrMagazineTiming timing;
  CHECK(vrFindMagazineTiming(samples.data(), samples.size(), timing));
  CHECK(std::abs(timing.out - 0.5f) < 0.011f);
  CHECK(std::abs(timing.in - 1.3f) < 0.011f);
  CHECK(timing.grip > 1.2f && timing.grip < 1.25f);
  CHECK(std::abs(timing.axis.z + 1.0f) < 1e-4f);
}


TEST_CASE(magazine_timing_needs_the_magazine_back) {
  std::vector<VrMagazineSample> still = { { 0.0f, { } }, { 0.5f, { } }, { 1.0f, { } } };
  std::vector<VrMagazineSample> gone  = { { 0.0f, { } }, { 0.5f, { 0.0f, 0.0f, -0.1f } } };

  VrMagazineTiming timing;
  CHECK(!vrFindMagazineTiming(still.data(), still.size(), timing));
  CHECK(!vrFindMagazineTiming(gone.data(), gone.size(), timing));
}


TEST_CASE(sounds_are_matched_by_time) {
  // The 10mm pistol: out, in, chamber
  const float times[] = { 0.4f, 1.25f, 1.7f };

  CHECK(vrFindNearestSound(times, 3, 0.5f, 0.35f, -1) == 0);
  CHECK(vrFindNearestSound(times, 3, 1.3f, 0.35f, 0) == 1);
  CHECK(vrFindNearestSound(times, 3, 3.0f, 0.35f, -1) == -1);
  CHECK(vrFindNearestSound(times, 3, 0.45f, 0.35f, 0) == -1);
}


namespace {

  VrGameTransform heldAt(float x, float turn) {
    VrGameTransform t;
    t.translate = { x, 0.0f, 0.0f };
    t.rotate.m[0][0] = std::cos(turn);
    t.rotate.m[0][1] = -std::sin(turn);
    t.rotate.m[1][0] = std::sin(turn);
    t.rotate.m[1][1] = std::cos(turn);
    t.rotate.m[2][2] = 1.0f;
    return t;
  }

}


TEST_CASE(magazine_hold_is_where_the_hand_carries_it) {
  std::vector<VrGameTransform> samples;

  // Falling away from the hand, carried 8 cm from the hand bone, then
  // pushed in by the palm 12 cm from it
  for (int32_t i = 0; i < 10; i++)
    samples.push_back(heldAt(0.05f + 0.02f * float(i), 0.0f));
  for (int32_t i = 0; i < 10; i++)
    samples.push_back(heldAt(0.08f, 0.5f));
  for (int32_t i = 0; i < 10; i++)
    samples.push_back(heldAt(0.12f, 1.0f));

  size_t hold = vrFindMagazineHold(samples.data(), samples.size());
  CHECK(hold >= 10 && hold < 20);
}


TEST_CASE(magazine_hold_needs_the_hand_to_carry_it) {
  std::vector<VrGameTransform> samples;

  // Turning in the hand all the time
  for (int32_t i = 0; i < 10; i++)
    samples.push_back(heldAt(0.08f, 0.1f * float(i)));

  CHECK(vrFindMagazineHold(samples.data(), samples.size()) == samples.size());
  CHECK(vrFindMagazineHold(samples.data(), 0) == 0);
}
