#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "vr_weapon_aim.h"

namespace dxvk {

  /**
   * \brief A sound of the game's reload animation
   */
  struct VrReloadSound {
    /// The TESSound form, or \c nullptr if the animation has none there
    const uint8_t* sound  = nullptr;
    float          volume = 1.0f;
    float          pitch  = 1.0f;
  };

  /**
   * \brief Rotation of a bone in its parent, by the bone's name
   */
  struct VrBonePose {
    std::string    name;
    VrGameRotation rotate;
  };

  /**
   * \brief What the game's reload animation does with a weapon's magazine
   *
   * Read from the first-person reload animation the game would play for
   * the weapon, without playing it: its tracks are evaluated at chosen
   * times, see findings/reload.md §8.2.
   */
  struct VrReloadAnimation {
    /// The magazine part in the left hand bone's frame while the hand
    /// holds the new magazine, in units of the hand's scale
    VrGameTransform         magazineInHand;
    /// Rotations of the bones below the left hand in their parents while
    /// it holds the new magazine
    std::vector<VrBonePose> fingers;
    /// Direction the magazine comes out of the weapon along, unit, in the
    /// weapon node's frame
    VrVector3               axis;
    /// The magazine going out and going in
    VrReloadSound           out;
    VrReloadSound           in;
  };

  /**
   * \brief Reads the reload animation of the weapon in the player's hand
   *
   * The animation the game would play for the weapon (its reload group,
   * or its modded one with the extended magazine) is looked up in the
   * player's first-person animations. The magazine's track tells when it
   * goes out and in and along which direction, the left hand's tracks how
   * the hand holds it, and the animation's sounds nearest those times are
   * the magazine's.
   *
   * Only called on the game's main thread, while the animation is not
   * played. Evaluating a track keeps nothing but its last time.
   *
   * \param [in] player The PlayerCharacter
   * \param [in] root Root of the first-person model
   * \param [in] weapon The weapon node in it
   * \param [in] magazine The magazine part below the weapon node
   * \param [in] leftHand The left hand bone
   * \param [out] animation What the animation does
   * \returns \c false if the weapon has no first-person reload animation
   *    that takes the magazine out and puts it back
   */
  bool vrReadReloadAnimation(
          uint8_t*              player,
          uint8_t*              root,
          uint8_t*              weapon,
          uint8_t*              magazine,
          uint8_t*              leftHand,
          VrReloadAnimation&    animation);

  /**
   * \brief Plays a sound of the reload animation for the player
   *
   * As the game plays the sounds of the player's first-person animations,
   * at the reload speed, but from a point of the world.
   *
   * \param [in] sound The sound
   * \param [in] position Where it is played from, in the game world
   */
  void vrPlayReloadSound(
    const VrReloadSound&        sound,
    const VrVector3&            position);

}
