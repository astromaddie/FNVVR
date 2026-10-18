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
   * \brief The rack part at a point of its way back
   */
  struct VrRackPose {
    /// How far back along the way it is, in metres
    float           along = 0.0f;
    /// The part in the weapon node's frame
    VrGameTransform pose;
  };

  /**
   * \brief What the game's reload animation does with a weapon's rack part
   */
  struct VrRackAnimation {
    /// The part from home to all the way back, \c along never going down
    std::vector<VrRackPose> poses;
    /// Direction it is pulled back along, unit, in the weapon node's frame
    VrVector3               axis;
    /// How far, in metres
    float                   travel = 0.0f;
    /// The left hand holds it all the way back
    bool                    held = false;
    /// The rack part in the left hand bone's frame then, in units of the
    /// hand's scale
    VrGameTransform         partInHand;
    /// Rotations of the bones below the left hand in their parents then
    std::vector<VrBonePose> fingers;
    /// The weapon chambering a round
    VrReloadSound           sound;
  };

  /**
   * \brief What the game's reload animation does with a weapon's magazine
   *
   * Read from the first-person reload animation the game would play for
   * the weapon, without playing it: its tracks are evaluated at chosen
   * times, see findings/reload.md §8.2.
   */
  struct VrReloadAnimation {
    /// The part is a round loaded by itself (VrWeaponPartKind::Round),
    /// read from the animation that loads one round
    bool                    round = false;
    /// Where a round is in its place, in the weapon node's frame (the
    /// round part's own place in the model may be elsewhere), valid if
    /// \c round
    VrGameTransform         place;
    /// The magazine part in the left hand bone's frame while the hand
    /// holds the new magazine, in units of the hand's scale
    VrGameTransform         magazineInHand;
    /// Rotations of the bones below the left hand in their parents while
    /// it holds the new magazine
    std::vector<VrBonePose> fingers;
    /// Direction the magazine comes out of the weapon along, unit, in the
    /// weapon node's frame
    VrVector3               axis;
    /// The magazine going out and going in (a round has no sound going
    /// out)
    VrReloadSound           out;
    VrReloadSound           in;
    /// How the animation racks the weapon, valid if \c hasRack
    VrRackAnimation         rack;
    bool                    hasRack = false;
  };

  /**
   * \brief Reads the reload animation of the weapon in the player's hand
   *
   * The animation the game would play for the weapon (its reload group,
   * or its modded one with the extended magazine) is looked up in the
   * player's first-person animations. The magazine's track tells when it
   * goes out and in and along which direction, the left hand's tracks how
   * the hand holds it, and the animation's sounds nearest those times are
   * the magazine's. The rack part's track, if there is one, tells how the
   * weapon is racked, and the sound nearest the time it is furthest back
   * is the chambering.
   *
   * For a weapon loaded round by round, the animation is the one that
   * loads one round, and the part is the round it carries in: where it
   * comes to rest is its place, the direction it comes from the way in,
   * and it has no sound going out.
   *
   * Only called on the game's main thread, while the animation is not
   * played. Evaluating a track keeps nothing but its last time.
   *
   * \param [in] player The PlayerCharacter
   * \param [in] root Root of the first-person model
   * \param [in] weapon The weapon node in it
   * \param [in] magazine The magazine part below the weapon node, or the
   *    round part
   * \param [in] round The part is a round
   * \param [in] rack The rack part below the weapon node, or \c nullptr
   * \param [in] leftHand The left hand bone
   * \param [out] animation What the animation does
   * \returns \c false if the weapon has no first-person reload animation
   *    that takes the magazine out and puts it back, or that carries the
   *    round in
   */
  bool vrReadReloadAnimation(
          uint8_t*              player,
          uint8_t*              root,
          uint8_t*              weapon,
          uint8_t*              magazine,
          bool                  round,
          uint8_t*              rack,
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
