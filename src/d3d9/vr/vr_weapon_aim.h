#pragma once

#include "vr_game_space.h"

namespace dxvk {

  /**
   * \brief Rotation in the game's layout
   *
   * Row-major 3x3, like the game's NiMatrix3: it maps a vector from a
   * node's frame to its parent's, and the columns are the node's axes.
   */
  struct VrGameRotation {
    float m[3][3] = {
      { 1.0f, 0.0f, 0.0f },
      { 0.0f, 1.0f, 0.0f },
      { 0.0f, 0.0f, 1.0f },
    };
  };

  /**
   * \brief Rigid transform in game units
   */
  struct VrGameTransform {
    VrGameRotation rotate;
    VrVector3      translate;
  };

  VrGameRotation operator * (const VrGameRotation& a, const VrGameRotation& b);

  VrVector3 operator * (const VrGameRotation& r, const VrVector3& v);

  VrGameRotation vrTranspose(const VrGameRotation& r);

  /**
   * \brief Rotation of a camera pose: forward, up and right as columns
   */
  VrGameRotation vrCameraRotation(const VrGameCameraPose& pose);

  /**
   * \brief Computes how to move the first-person model to the hand
   *
   * The game draws the weapon and the arms as one model held in front of
   * its camera. Moving the whole model by the returned transform keeps
   * what the game animates relative to its camera, the weapon's angle and
   * its recoil included, but relative to the hand: the weapon turns with
   * the rotation from the game's camera to the hand, and its node lands on
   * the hand.
   *
   * \param [in] gameCamera Camera the game placed the model for
   * \param [in] weaponPosition Position of the weapon node
   * \param [in] hand Hand pose: forward is where it points
   * \returns Transform to apply on the left of the model's root
   */
  VrGameTransform vrComputeRigToHand(
    const VrGameCameraPose& gameCamera,
    const VrVector3&        weaponPosition,
    const VrGameCameraPose& hand);

  /**
   * \brief Computes how to move the first-person model to hold the weapon still
   *
   * Unlike vrComputeRigToHand, what the game animates on the weapon is
   * undone: the weapon keeps the same rotation relative to the hand, and
   * the arms follow it. A melee weapon is swung by the hand, so the game's
   * idle, walking and block animations must not turn it.
   *
   * \param [in] weaponRotation World rotation the game animated the weapon to
   * \param [in] weaponPosition Position of the weapon node
   * \param [in] held Weapon rotation relative to the hand's rotation
   *    (vrCameraRotation), see vrComputeHeldRotation
   * \param [in] hand Hand pose: forward is where it points
   * \returns Transform to apply on the left of the model's root
   */
  VrGameTransform vrComputeRigHoldingWeapon(
    const VrGameRotation&   weaponRotation,
    const VrVector3&        weaponPosition,
    const VrGameRotation&   held,
    const VrGameCameraPose& hand);

  /**
   * \brief Rotation of the weapon relative to the hand under vrComputeRigToHand
   *
   * \param [in] gameCamera Camera the game placed the model for
   * \param [in] weaponRotation World rotation the game animated the weapon to
   */
  VrGameRotation vrComputeHeldRotation(
    const VrGameCameraPose& gameCamera,
    const VrGameRotation&   weaponRotation);

  /**
   * \brief Turns the hand holding the weapon towards the supporting hand
   *
   * With both hands on the weapon, the line between the hands decides
   * where it points. The hand is turned by the smallest rotation that
   * brings the point where the game animates the supporting hand onto the
   * line towards the other controller, so that the hand's roll is kept.
   *
   * \param [in] hand Pose of the hand holding the weapon
   * \param [in] support Offset from \p hand to where the supporting hand
   *    holds the weapon, with the weapon held by \p hand alone
   * \param [in] target Offset from \p hand to the supporting hand
   * \returns \p hand turned, or \p hand unchanged if either offset is
   *    zero or the two point in nearly the same or opposite directions
   */
  VrGameCameraPose vrComputeTwoHandedPose(
    const VrGameCameraPose& hand,
    const VrVector3&        support,
    const VrVector3&        target);

  /**
   * \brief Inverse of a rigid transform
   */
  VrGameTransform vrInverse(const VrGameTransform& t);

  /**
   * \brief Places a transform given relative to a controller
   *
   * \param [in] controller Pose of the controller
   * \param [in] local Transform in the controller's frame, whose axes are
   *    forward, up and right as in vrCameraRotation
   * \returns The transform in the frame of the controller's pose
   */
  VrGameTransform vrComputeHeldTransform(
    const VrGameCameraPose&     controller,
    const VrGameTransform&      local);

  /**
   * \brief Places a point carried by the forearm of a hand in a fixed pose
   *
   * The free hand takes a fixed pose in its controller's frame, and the
   * hand bone and the forearm's twist bone take fixed rotations in the
   * forearm. The bones' offsets in the forearm are the model's own and do
   * not depend on those rotations.
   *
   * \param [in] hand Hand bone's transform in the controller's frame
   * \param [in] wrist Hand bone's rotation in the forearm
   * \param [in] twist Twist bone's rotation in the forearm
   * \param [in] wristOffset Hand bone's position in the forearm
   * \param [in] twistOffset Twist bone's position in the forearm
   * \param [in] point Point in the twist bone's frame
   * \returns The point in the controller's frame (forward, up and right)
   */
  VrVector3 vrComputeForearmPoint(
    const VrGameTransform&      hand,
    const VrGameRotation&       wrist,
    const VrGameRotation&       twist,
    const VrVector3&            wristOffset,
    const VrVector3&            twistOffset,
    const VrVector3&            point);

  /**
   * \brief Heading and pitch of a direction in the game's convention
   *
   * Heading 0 faces north (+Y) and grows clockwise, pitch is positive
   * downwards, both in radians, like the player's angles.
   *
   * \param [in] direction Direction in the game world, need not be unit length
   */
  VrHeadAngles vrComputeAimAngles(const VrVector3& direction);

  /**
   * \brief Direction a weapon's projectile node fires along
   *
   * The game turns a node's world rotation into a heading and a pitch with
   * NiMatrix3::ToEulerAnglesZXY when it fires from a third-person weapon:
   * the rotation is Rz(heading) * Rx(pitch) * Ry(roll), so the direction
   * of fire is the node's Y axis, whatever the roll.
   *
   * \param [in] rotation World rotation of the projectile node
   * \returns The node's Y axis in the game world
   */
  VrVector3 vrComputeBarrelDirection(const VrGameRotation& rotation);

}
