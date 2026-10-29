#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "vr_game_sequence.h"
#include "vr_game_sound.h"
#include "vr_throw.h"

namespace dxvk {

  /**
   * \brief A sound of a throw's animation, at its time
   */
  struct VrThrowSound {
    /// Seconds after the first sound of its group
    float            delay = 0.0f;
    VrAnimationSound sound;
    bool             loops = false;
  };

  /**
   * \brief A part of the model as the animation poses it, in its parent
   */
  struct VrPartPose {
    std::string     name;
    VrGameTransform pose;
    float           scale = 1.0f;
  };

  /**
   * \brief How far from the left hand bone the lighter's meshes may be for
   *    the hand to hold it, in game units: the bone sits at the wrist
   */
  constexpr float VrLighterReach = 0.12f * VrGameUnitsPerMetre;

  /**
   * \brief How the left hand holds the lighter of the dynamite or the
   *    fire bomb
   */
  struct VrLighterHold {
    /// The lighter in the left hand bone's frame, in units of the hand's
    /// scale
    VrGameTransform         inHand;
    /// The centre of the lighter's meshes in the hand bone's frame, in
    /// units of the hand's scale, and the unit direction from it towards
    /// its flame
    VrVector3               centre;
    VrVector3               up;
    /// The rotations of the bones below the left hand on it
    std::vector<VrBonePose> fingers;
  };

  /**
   * \brief What the game's attack animation of a grenade or mine does
   *    with the left hand
   *
   * See findings/throw.md §5. The left hand acts on the part (the pin, the
   * lighter's flame at the fuse, the button) at one time of the animation,
   * before the release.
   */
  struct VrThrowAnimation {
    /// The left hand's sounds up to it, played as the hand gets there:
    /// the lighter opening and striking, the hand on the pin
    std::vector<VrThrowSound> prepare;
    /// The sounds from it to the release, played when the weapon is
    /// armed: the fuse catching, the pin coming out, the button's beep
    std::vector<VrThrowSound> arm;
    /// The rotations of the bones below the left hand then
    std::vector<VrBonePose>   fingers;
    /// The lighter in the left hand when the animation holds it nearest
    /// the hand bone; none if it never comes within reach of the hand
    bool                      hasLighter = false;
    VrLighterHold             lighter;
    /// The lighter's parts the animation moves, lit (its cap open), when
    /// the left hand acts
    std::vector<VrPartPose>   lighterLit;
    /// The button and the parts below it that the animation moves, as
    /// they are once it is armed (a switch thrown)
    std::vector<VrPartPose>   pressed;
    /// The most particles per second the animation has the lit fuse's
    /// emitter give, 0 without such a track
    float                     sparkRate = 0.0f;
  };

  /**
   * \brief How the attack animation of a thrown weapon holds it when it
   *    lets go of it
   */
  struct VrThrownGrip {
    /// The weapon node in the right hand bone's frame at the release
    VrSequencePose          weapon;
    /// The parts below the weapon node the animation moves, in their
    /// parents at the release, parents first
    std::vector<VrPartPose> parts;
    /// The largest turn, in radians, of the weapon node or of one of these
    /// parts in the right hand, from the pose the game holds it in now
    float                   turn = 0.0f;
  };

  /**
   * \brief The nodes of the first-person model a throw's animation is
   *    read on
   */
  struct VrThrowAnimationNodes {
    /// The first-person root and the left hand bone
    uint8_t* root     = nullptr;
    uint8_t* leftHand = nullptr;
    /// The part the left hand acts on: the pin, the button, the fuse's
    /// mesh
    uint8_t* target   = nullptr;
    /// The button or the mine's light, or \c nullptr
    uint8_t* button   = nullptr;
    /// The lighter and its flame, or \c nullptr
    uint8_t* lighter  = nullptr;
    uint8_t* flame    = nullptr;
    /// The particle system of the lit fuse's sparks, or \c nullptr
    uint8_t* sparks   = nullptr;
  };

  /**
   * \brief Reads the player's first-person attack animation of the
   *    equipped grenade or mine
   *
   * The time the left hand acts on the part is the time the flame is
   * nearest the fuse, or the hand bone nearest the part, before the
   * animation's Release key.
   *
   * \param [in] player The player character
   * \param [in] weapon The weapon form
   * \param [in] arming How the weapon is armed
   * \param [in] nodes The nodes of the model in the hand
   * \param [out] animation What the animation does
   * \returns \c false if the weapon has no such animation
   */
  bool vrReadThrowAnimation(
          uint8_t*              player,
    const uint8_t*              weapon,
          VrThrowArming         arming,
    const VrThrowAnimationNodes& nodes,
          VrThrowAnimation&     animation);

  /**
   * \brief Reads how the player's first-person attack animation of the
   *    equipped thrown weapon holds it when it lets go of it
   *
   * At the animation's Release key (else its Hit key, else its end). Read
   * while the game holds the weapon in its idle pose, to compare with it.
   *
   * \param [in] player The player character
   * \param [in] weapon The weapon form
   * \param [in] weaponNode The weapon node of the first-person model
   * \param [in] rightHand The right hand bone above it
   * \param [out] grip How the animation holds the weapon then
   * \returns \c false if the weapon has no such animation, or the weapon
   *    node is not below the hand bone
   */
  bool vrReadThrownGrip(
          uint8_t*              player,
    const uint8_t*              weapon,
          uint8_t*              weaponNode,
          uint8_t*              rightHand,
          VrThrownGrip&         grip);

}
