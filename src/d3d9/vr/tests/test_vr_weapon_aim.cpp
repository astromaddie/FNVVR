#include <cmath>

#include "test.h"

#include "../vr_weapon_aim.h"

using namespace dxvk;

namespace {

  constexpr float Pi = 3.14159265358979f;

  void checkVector(const VrVector3& v, float x, float y, float z) {
    CHECK_NEAR(v.x, x, 1e-4);
    CHECK_NEAR(v.y, y, 1e-4);
    CHECK_NEAR(v.z, z, 1e-4);
  }

  VrGameCameraPose northFacing(const VrVector3& position) {
    VrGameCameraPose pose;
    pose.forward  = { 0.0f, 1.0f, 0.0f };
    pose.up       = { 0.0f, 0.0f, 1.0f };
    pose.right    = { 1.0f, 0.0f, 0.0f };
    pose.position = position;
    return pose;
  }

  VrGameCameraPose eastFacing(const VrVector3& position) {
    VrGameCameraPose pose;
    pose.forward  = { 1.0f, 0.0f, 0.0f };
    pose.up       = { 0.0f, 0.0f, 1.0f };
    pose.right    = { 0.0f, -1.0f, 0.0f };
    pose.position = position;
    return pose;
  }

  VrVector3 apply(const VrGameTransform& t, const VrVector3& v) {
    return t.rotate * v + t.translate;
  }

  // The game's NiMatrix3::MakeXRotation, MakeYRotation and MakeZRotation
  VrGameRotation rotationX(float angle) {
    float c = std::cos(angle), s = std::sin(angle);
    VrGameRotation r;
    r.m[1][1] = c; r.m[1][2] = s;
    r.m[2][1] = -s; r.m[2][2] = c;
    return r;
  }

  VrGameRotation rotationY(float angle) {
    float c = std::cos(angle), s = std::sin(angle);
    VrGameRotation r;
    r.m[0][0] = c; r.m[0][2] = -s;
    r.m[2][0] = s; r.m[2][2] = c;
    return r;
  }

  VrGameRotation rotationZ(float angle) {
    float c = std::cos(angle), s = std::sin(angle);
    VrGameRotation r;
    r.m[0][0] = c; r.m[0][1] = s;
    r.m[1][0] = -s; r.m[1][1] = c;
    return r;
  }

}


TEST_CASE(camera_rotation_maps_local_axes_to_camera_axes) {
  VrGameRotation r = vrCameraRotation(eastFacing({ }));

  // The camera looks along its local +X, up is +Y and right is +Z
  checkVector(r * VrVector3 { 1.0f, 0.0f, 0.0f }, 1.0f, 0.0f, 0.0f);
  checkVector(r * VrVector3 { 0.0f, 1.0f, 0.0f }, 0.0f, 0.0f, 1.0f);
  checkVector(r * VrVector3 { 0.0f, 0.0f, 1.0f }, 0.0f, -1.0f, 0.0f);
}


TEST_CASE(rig_moves_weapon_onto_hand) {
  VrGameCameraPose camera = northFacing({ 0.0f, 0.0f, 120.0f });
  VrGameCameraPose hand   = northFacing({ 15.0f, 30.0f, 90.0f });
  VrVector3 weapon = { 8.0f, 20.0f, 110.0f };

  VrGameTransform rig = vrComputeRigToHand(camera, weapon, hand);
  checkVector(apply(rig, weapon), 15.0f, 30.0f, 90.0f);
}


TEST_CASE(rig_turns_with_hand_relative_to_camera) {
  // The game holds the weapon pointing north; the hand points east
  VrGameCameraPose camera = northFacing({ });
  VrGameCameraPose hand   = eastFacing({ 50.0f, 0.0f, 0.0f });
  VrVector3 weapon = { 0.0f, 10.0f, 0.0f };

  VrGameTransform rig = vrComputeRigToHand(camera, weapon, hand);

  // The barrel, along the camera's forward, now points east
  checkVector(rig.rotate * VrVector3 { 0.0f, 1.0f, 0.0f }, 1.0f, 0.0f, 0.0f);
  checkVector(apply(rig, weapon), 50.0f, 0.0f, 0.0f);

  // A muzzle further along the barrel stays in front of the hand
  checkVector(apply(rig, VrVector3 { 0.0f, 30.0f, 0.0f }), 70.0f, 0.0f, 0.0f);
}


TEST_CASE(rig_holding_weapon_undoes_its_animation) {
  VrGameCameraPose camera = northFacing({ });
  VrGameCameraPose hand   = eastFacing({ 50.0f, 0.0f, 0.0f });
  VrVector3 weapon = { 0.0f, 10.0f, 0.0f };

  // Held as the game first animated it: the same as following the camera
  VrGameRotation idle = rotationZ(0.3f);
  VrGameRotation held = vrComputeHeldRotation(camera, idle);
  VrGameTransform follow = vrComputeRigToHand(camera, weapon, hand);
  VrGameTransform hold   = vrComputeRigHoldingWeapon(idle, weapon, held, hand);

  for (uint32_t i = 0; i < 3; i++) {
    for (uint32_t j = 0; j < 3; j++)
      CHECK_NEAR(hold.rotate.m[i][j], follow.rotate.m[i][j], 1e-5);
  }

  checkVector(apply(hold, weapon), 50.0f, 0.0f, 0.0f);

  // The game raises the weapon to block: it stays as it was in the hand
  VrGameRotation raised = rotationX(0.8f) * idle;
  hold = vrComputeRigHoldingWeapon(raised, weapon, held, hand);
  VrGameRotation drawn = hold.rotate * raised;
  VrGameRotation expected = follow.rotate * idle;

  for (uint32_t i = 0; i < 3; i++) {
    for (uint32_t j = 0; j < 3; j++)
      CHECK_NEAR(drawn.m[i][j], expected.m[i][j], 1e-5);
  }

  checkVector(apply(hold, weapon), 50.0f, 0.0f, 0.0f);
}


TEST_CASE(rotation_angle_between_rotations) {
  CHECK_NEAR(vrRotationAngle(rotationZ(0.3f), rotationZ(0.3f)), 0.0f, 1e-3);
  CHECK_NEAR(vrRotationAngle(rotationZ(0.3f), rotationZ(0.8f)), 0.5f, 1e-4);
  CHECK_NEAR(vrRotationAngle(rotationX(0.2f) * rotationZ(0.3f), rotationZ(0.3f)), 0.2f, 1e-4);
  CHECK_NEAR(vrRotationAngle({ }, rotationY(Pi)), Pi, 1e-3);
}


TEST_CASE(held_rotation_waits_for_the_game_to_settle) {
  constexpr float Frame = 1.0f / 72.0f;
  VrHeldRotationSettle settle;

  // The game blends the weapon from the Pip-Boy pose to its idle pose
  float angle = 1.2f;

  while (angle > 0.0f) {
    CHECK(!settle.update(rotationZ(angle), Frame));
    angle -= 0.03f;
  }

  CHECK(!settle.settled());

  // Idle, with a slow sway well within the tolerance
  bool took = false;
  float time = 0.0f;

  for (uint32_t i = 0; i < 30 && !took; i++) {
    took = settle.update(rotationX(0.001f * float(i)), Frame);
    time += Frame;
  }

  CHECK(took);
  CHECK(settle.settled());
  CHECK(!settle.timedOut());
  CHECK(time >= VrHeldRotationSettle::SettleTime);
  CHECK(vrRotationAngle(settle.rotation(), { }) < VrHeldRotationSettle::Tolerance);

  // Taken once: blocking afterwards does not change it
  VrGameRotation taken = settle.rotation();
  CHECK(!settle.update(rotationX(0.8f), Frame));
  CHECK_NEAR(vrRotationAngle(settle.rotation(), taken), 0.0f, 1e-3);

  // A new weapon waits again
  settle.reset();
  CHECK(!settle.settled());
  CHECK(!settle.update(rotationZ(0.5f), Frame));
}


TEST_CASE(held_rotation_is_taken_if_the_game_keeps_animating) {
  constexpr float Frame = 1.0f / 72.0f;
  VrHeldRotationSettle settle;

  float angle = 0.0f;
  float time  = 0.0f;

  while (!settle.update(rotationZ(angle), Frame)) {
    angle += 0.1f;
    time  += Frame;
    CHECK(time < VrHeldRotationSettle::MaxWait + Frame);
  }

  CHECK(settle.timedOut());
  CHECK_NEAR(vrRotationAngle(settle.rotation(), rotationZ(angle)), 0.0f, 1e-3);
}


TEST_CASE(aim_angles_follow_game_convention) {
  VrHeadAngles north = vrComputeAimAngles({ 0.0f, 2.0f, 0.0f });
  CHECK_NEAR(north.yaw, 0.0f, 1e-5);
  CHECK_NEAR(north.pitch, 0.0f, 1e-5);

  VrHeadAngles east = vrComputeAimAngles({ 1.0f, 0.0f, 0.0f });
  CHECK_NEAR(east.yaw, Pi / 2.0f, 1e-5);

  VrHeadAngles west = vrComputeAimAngles({ -1.0f, 0.0f, 0.0f });
  CHECK_NEAR(west.yaw, 3.0f * Pi / 2.0f, 1e-5);

  // Positive pitch looks down
  VrHeadAngles down = vrComputeAimAngles({ 0.0f, 1.0f, -1.0f });
  CHECK_NEAR(down.pitch, Pi / 4.0f, 1e-5);
}


TEST_CASE(two_handed_pose_turns_towards_the_supporting_hand) {
  VrGameCameraPose hand = northFacing({ 5.0f, 6.0f, 7.0f });

  VrGameCameraPose turned = vrComputeTwoHandedPose(hand, { 0.0f, 20.0f, 0.0f }, { 30.0f, 0.0f, 0.0f });
  checkVector(turned.forward, 1.0f, 0.0f, 0.0f);
  checkVector(turned.up, 0.0f, 0.0f, 1.0f);
  checkVector(turned.right, 0.0f, -1.0f, 0.0f);
  checkVector(turned.position, 5.0f, 6.0f, 7.0f);

  float h = std::sqrt(0.5f);
  VrGameCameraPose raised = vrComputeTwoHandedPose(hand, { 0.0f, 20.0f, 0.0f }, { 0.0f, 10.0f, 10.0f });
  checkVector(raised.forward, 0.0f, h, h);
  checkVector(raised.up, 0.0f, -h, h);
  checkVector(raised.right, 1.0f, 0.0f, 0.0f);
}


TEST_CASE(two_handed_pose_keeps_the_hand_when_aligned) {
  VrGameCameraPose hand = northFacing({ 0.0f, 0.0f, 0.0f });

  VrGameCameraPose same = vrComputeTwoHandedPose(hand, { 0.0f, 20.0f, -5.0f }, { 0.0f, 40.0f, -10.0f });
  checkVector(same.forward, 0.0f, 1.0f, 0.0f);
  checkVector(same.up, 0.0f, 0.0f, 1.0f);

  VrGameCameraPose none = vrComputeTwoHandedPose(hand, { 0.0f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f });
  checkVector(none.forward, 0.0f, 1.0f, 0.0f);
}


TEST_CASE(held_transform_follows_its_controller) {
  // Half a turn around the controller's forward axis, held 10 units in
  // front of it and 2 units to its right
  VrGameTransform local;
  local.rotate.m[1][1] = -1.0f;
  local.rotate.m[2][2] = -1.0f;
  local.translate = { 10.0f, 0.0f, 2.0f };

  VrGameTransform held = vrComputeHeldTransform(eastFacing({ 1.0f, 2.0f, 3.0f }), local);

  // Facing east, right is south: the held up axis points down
  checkVector(held.translate, 11.0f, 0.0f, 3.0f);
  checkVector(held.rotate * VrVector3 { 1.0f, 0.0f, 0.0f }, 1.0f, 0.0f, 0.0f);
  checkVector(held.rotate * VrVector3 { 0.0f, 1.0f, 0.0f }, 0.0f, 0.0f, -1.0f);
  checkVector(held.rotate * VrVector3 { 0.0f, 0.0f, 1.0f }, 0.0f, 1.0f, 0.0f);
}


TEST_CASE(forearm_point_follows_the_hand) {
  VrGameTransform hand;
  hand.translate = { 1.0f, 2.0f, 3.0f };

  VrGameRotation identity;
  checkVector(vrComputeForearmPoint(hand, identity, identity,
    { 18.0f, 0.0f, 0.0f }, { 9.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }), -8.0f, 3.0f, 3.0f);

  // Whatever the rotations, the hand bone's own position lands on the hand
  hand.rotate = vrCameraRotation(eastFacing({ }));

  VrGameRotation wrist;
  wrist.m[0][0] = 0.0f; wrist.m[0][1] = -1.0f;
  wrist.m[1][0] = 1.0f; wrist.m[1][1] =  0.0f;

  VrGameRotation twist;
  twist.m[1][1] = 0.0f; twist.m[1][2] = -1.0f;
  twist.m[2][1] = 1.0f; twist.m[2][2] =  0.0f;

  VrVector3 wristOffset = { 18.0f, 1.0f, 0.0f };
  VrVector3 twistOffset = {  9.0f, 0.0f, 0.0f };

  // twistOffset + twist * point == wristOffset
  VrVector3 point = vrTranspose(twist) * (wristOffset - twistOffset);
  checkVector(vrComputeForearmPoint(hand, wrist, twist, wristOffset, twistOffset, point), 1.0f, 2.0f, 3.0f);
}


TEST_CASE(inverse_undoes_a_transform) {
  VrGameTransform t;
  t.rotate    = vrCameraRotation(eastFacing({ }));
  t.translate = { 3.0f, -4.0f, 5.0f };

  VrVector3 v = { 1.0f, 2.0f, 3.0f };
  VrVector3 back = apply(vrInverse(t), apply(t, v));
  checkVector(back, 1.0f, 2.0f, 3.0f);
}


TEST_CASE(barrel_direction_gives_the_node_heading_and_pitch) {
  // The game's FromEulerAnglesZXY, whose inverse it uses to fire from a
  // third-person weapon's projectile node
  float heading = 0.7f;
  float pitch   = -0.3f;

  for (float roll : { 0.0f, 1.2f, -2.5f }) {
    VrGameRotation node = rotationZ(heading) * rotationX(pitch) * rotationY(roll);
    VrHeadAngles angles = vrComputeAimAngles(vrComputeBarrelDirection(node));
    CHECK_NEAR(angles.yaw, heading, 1e-4);
    CHECK_NEAR(angles.pitch, pitch, 1e-4);
  }
}
