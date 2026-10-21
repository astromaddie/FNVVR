#include <algorithm>
#include <cmath>

#include "test.h"

#include "../vr_hand_fingers.h"

using namespace dxvk;

namespace {

  constexpr float DegreesPerRadian = 57.2957795f;

  // Angle of the rotation between two rotations, in degrees
  float angleBetween(const VrGameRotation& a, const VrGameRotation& b) {
    VrGameRotation d = vrTranspose(a) * b;
    float cosine = (d.m[0][0] + d.m[1][1] + d.m[2][2] - 1.0f) * 0.5f;
    return std::acos(std::max(-1.0f, std::min(1.0f, cosine))) * DegreesPerRadian;
  }

  uint32_t findBone(const char* name) {
    VrHand hand;
    uint32_t index = VrFingerBoneCount;
    vrFindFingerBone(name, hand, index);
    return index;
  }

  VrFingerCurls curled(VrFinger finger, float curl) {
    VrFingerCurls curls = { };
    curls[uint32_t(finger)] = curl;
    return curls;
  }

}


TEST_CASE(finger_bones_are_found_on_both_hands_only) {
  VrHand hand = VrHand::Right;
  uint32_t left = VrFingerBoneCount, right = VrFingerBoneCount;

  CHECK(vrFindFingerBone("Bip01 L Finger21", hand, left));
  CHECK(hand == VrHand::Left);
  CHECK(vrFindFingerBone("Bip01 R Finger21", hand, right));
  CHECK(hand == VrHand::Right);
  CHECK(left == right);
  CHECK(vrFingerOfBone(left) == VrFinger::Middle);

  uint32_t index;
  CHECK(!vrFindFingerBone("Bip01 L Hand", hand, index));
  CHECK(!vrFindFingerBone("Bip01 L ForeTwist", hand, index));
  CHECK(!vrFindFingerBone("Bip01 X Finger1", hand, index));
  CHECK(!vrFindFingerBone("Weapon", hand, index));
}


TEST_CASE(each_finger_has_its_bones) {
  uint32_t counts[VrFingerCount] = { };

  for (uint32_t i = 0; i < VrFingerBoneCount; i++)
    counts[uint32_t(vrFingerOfBone(i))]++;

  CHECK(counts[uint32_t(VrFinger::Thumb)] == 3);

  for (uint32_t f = uint32_t(VrFinger::Index); f < VrFingerCount; f++)
    CHECK(counts[f] == 4);

  CHECK(vrFingerOfBone(findBone("Bip01 L Thumb12")) == VrFinger::Thumb);
  CHECK(vrFingerOfBone(findBone("Bip01 L Hand01")) == VrFinger::Index);
  CHECK(vrFingerOfBone(findBone("Bip01 L Finger42")) == VrFinger::Little);
}


TEST_CASE(a_curl_turns_the_finger_from_open_to_the_fist) {
  uint32_t joint = findBone("Bip01 L Finger11");

  VrGameRotation open   = vrPoseFingerBone(joint, VrHand::Left, VrFingerCurls());
  VrGameRotation closed = vrPoseFingerBone(joint, VrHand::Left, curled(VrFinger::Index, 1.0f));
  VrGameRotation half   = vrPoseFingerBone(joint, VrHand::Left, curled(VrFinger::Index, 0.5f));

  // The index's middle joint is barely bent when open, and closes by about
  // a hundred degrees into the fist
  float travel = angleBetween(open, closed);
  CHECK(travel > 90.0f && travel < 120.0f);

  // Half a curl is half way along the shortest turn
  CHECK_NEAR(angleBetween(open, half), travel * 0.5f, 0.1);
  CHECK_NEAR(angleBetween(half, closed), travel * 0.5f, 0.1);

  // Curls out of range are clamped
  CHECK_NEAR(angleBetween(closed, vrPoseFingerBone(joint, VrHand::Left, curled(VrFinger::Index, 2.0f))), 0.0f, 0.1);
  CHECK_NEAR(angleBetween(open, vrPoseFingerBone(joint, VrHand::Left, curled(VrFinger::Index, -1.0f))), 0.0f, 0.1);
}


TEST_CASE(a_bone_follows_only_its_own_finger) {
  uint32_t ring = findBone("Bip01 L Finger31");

  VrFingerCurls others = { 1.0f, 1.0f, 1.0f, 0.0f, 1.0f };

  CHECK_NEAR(angleBetween(
    vrPoseFingerBone(ring, VrHand::Left, VrFingerCurls()),
    vrPoseFingerBone(ring, VrHand::Left, others)), 0.0f, 0.1);
}


TEST_CASE(right_finger_bones_are_the_left_ones_mirrored) {
  VrFingerCurls curls = { 0.2f, 0.4f, 0.6f, 0.8f, 1.0f };

  for (uint32_t i = 0; i < VrFingerBoneCount; i++) {
    VrGameRotation left  = vrPoseFingerBone(i, VrHand::Left, curls);
    VrGameRotation right = vrPoseFingerBone(i, VrHand::Right, curls);
    VrGameRotation back  = vrMirrorHandRotation(right);

    for (uint32_t r = 0; r < 3; r++) {
      for (uint32_t c = 0; c < 3; c++)
        CHECK_NEAR(back.m[r][c], left.m[r][c], 1e-6);
    }
  }
}
