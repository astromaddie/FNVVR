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
    /// The lighter in the left hand bone's frame then, in units of the
    /// hand's scale
    bool                      hasLighter = false;
    VrGameTransform           lighterInHand;
    /// The lighter's parts the animation moves, lit (its cap open)
    std::vector<VrPartPose>   lighterLit;
    /// The button and the parts below it that the animation moves, as
    /// they are once it is armed (a switch thrown)
    std::vector<VrPartPose>   pressed;
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

}
