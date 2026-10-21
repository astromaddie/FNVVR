#pragma once

#include "vr_types.h"
#include "vr_weapon_aim.h"

namespace dxvk {

  /**
   * \brief Number of finger bones below each hand bone of the first-person
   *    model: three for the thumb, a metacarpal and three for each finger
   */
  constexpr uint32_t VrFingerBoneCount = 19;

  /**
   * \brief Finds a finger bone of the first-person model by its name
   *
   * \param [in] name Bone name, such as \c "Bip01 R Finger21"
   * \param [out] hand Hand the bone belongs to
   * \param [out] index Index of the bone, below VrFingerBoneCount
   * \returns \c true if the name is a finger bone of either hand
   */
  bool vrFindFingerBone(const char* name, VrHand& hand, uint32_t& index);

  /**
   * \brief Finger a finger bone belongs to
   */
  VrFinger vrFingerOfBone(uint32_t index);

  /**
   * \brief Local rotation of a finger bone for a hand's finger curls
   *
   * Each bone turns from the open pose, the free hand's relaxed pose, to
   * the closed pose, the game's own fist, by the curl of its finger. The
   * right hand's poses are the left hand's mirrored.
   *
   * \param [in] index Index of the bone, below VrFingerBoneCount
   * \param [in] hand Hand the bone belongs to
   * \param [in] curls Curl of each finger of that hand
   */
  VrGameRotation vrPoseFingerBone(uint32_t index, VrHand hand, const VrFingerCurls& curls);

}
