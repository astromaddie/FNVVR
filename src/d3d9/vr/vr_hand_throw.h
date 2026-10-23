#pragma once

#include <memory>

#include "vr_game_space.h"
#include "vr_throw.h"
#include "vr_weapon_hand.h"

namespace dxvk {

  /**
   * \brief Hands and buttons that throw for a frame
   */
  struct VrHandThrowInput {
    /// Right hand in the game world, or \c nullptr if it is not tracked,
    /// or not in game
    const VrGameCameraPose* hand = nullptr;
    /// Left hand's position in the game world, or \c nullptr if it is not
    /// tracked
    const VrVector3*        otherHand = nullptr;
    /// Pose the eyes and hands are placed on in the game world, which moves
    /// and turns with the player
    VrGameCameraPose        body;
    /// The attack trigger, the left grip and the left trigger
    bool                    attack  = false;
    bool                    grip    = false;
    bool                    trigger = false;
    /// Time since the previous frame, in seconds
    float                   dt = 0.0f;
  };

  /**
   * \brief What a throw did in a frame, for the haptics
   */
  struct VrHandThrowEvents {
    VrArmEvent arm    = VrArmEvent::None;
    bool       thrown = false;
  };

  /**
   * \brief Throws grenades, mines and thrown weapons with the hand
   *
   * The game's attack control is held back while such a weapon is in the
   * right hand, as for melee weapons swung with the hand, and the game's
   * throw animation never plays. The left hand arms the weapon (VrThrowArm:
   * pulls a grenade's pin, holds the lighter to the dynamite's fuse,
   * presses a mine's button). While the right trigger is held the hand's
   * motion is followed; letting go of it while the hand moves fast enough
   * fires the weapon with TESObjectWEAP::Fire, which spends one and hides
   * the model in the hand, or unequips the weapon after the last one. The
   * projectile leaves from where the model was, along the hand's motion,
   * as fast as the hand threw it but no faster than the game's own throw
   * (VrWeaponHand::setThrowRelease). A moment later the next one is shown
   * in the hand, unarmed. See findings/throw.md.
   *
   * V.A.T.S. keeps the game's own throw.
   *
   * Only used on the game's main thread.
   */
  class VrHandThrow {

  public:

    /**
     * \brief Finds the game's functions
     *
     * \param [in] weaponHand Draws the weapon in the hand and launches its
     *    projectiles, kept by the caller for the object's lifetime
     * \returns \c nullptr if the executable is not the supported version
     */
    static std::unique_ptr<VrHandThrow> install(VrWeaponHand& weaponHand);

    /**
     * \brief Arms and throws for a frame
     */
    VrHandThrowEvents update(const VrHandThrowInput& input);

    /**
     * \brief Whether the game's attack control is held back
     */
    bool throwsWithHand() const {
      return m_throwsWithHand;
    }

    /**
     * \brief Whether the left hand is busy arming the weapon
     */
    bool leftHandBusy() const {
      return m_throwsWithHand && m_arm.busy();
    }

  private:

    explicit VrHandThrow(VrWeaponHand& weaponHand)
    : m_weaponHand(weaponHand) { }

    VrWeaponHand&  m_weaponHand;
    VrThrowConfig  m_config;
    VrThrowArm     m_arm;
    VrThrowTracker m_tracker;

    // The weapon armed, compared but not dereferenced
    const uint8_t* m_form = nullptr;

    bool           m_throwsWithHand = false;
    bool           m_holding = false;

    // Time left until the next weapon is shown in the hand after a throw
    float          m_refill = 0.0f;

    uint32_t       m_loggedThrows = 0;
    uint32_t       m_loggedSlow   = 0;
    uint32_t       m_loggedUnarmed = 0;
    uint32_t       m_loggedArms   = 0;

    void startWeapon(const VrHeldThrown& held);

    void fire(const VrHeldThrown& held, const VrVector3& origin, const VrVector3& velocity);

    void logArm(VrArmEvent event);

  };

}
