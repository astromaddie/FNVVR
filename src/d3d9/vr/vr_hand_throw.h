#pragma once

#include <chrono>
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
    /// Left hand in the game world, or \c nullptr if it is not tracked
    const VrGameCameraPose* otherHand = nullptr;
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
   * pulls a grenade's pin, holds the lighter's flame to the dynamite's
   * fuse, presses a mine's button with the index finger), at the part of
   * the model it acts on (VrWeaponHand measures them). A grenade or thrown
   * weapon is thrown by letting go of the right trigger while the hand
   * moves fast enough; a mine leaves the hand at a press of the trigger,
   * as fast as the hand moves, however slowly.
   *
   * The weapon is fired with TESObjectWEAP::Fire, which spends one and
   * hides the model in the hand, or unequips the weapon after the last
   * one. The game fires its own throws while it updates the player, before
   * the AI threads step Havok alongside the rendering, so a throw by hand
   * is fired there too, on the next update of the player. The projectile
   * leaves from where the model was, along the hand's motion, with a share
   * of the game's own throw that grows with the hand's speed
   * (VrWeaponHand::setThrowRelease). A moment later the next one is shown
   * in the hand, unarmed. Where the projectile is a moment after it left
   * is logged. See findings/throw.md.
   *
   * V.A.T.S. keeps the game's own throw.
   *
   * Only used on the game's main thread.
   */
  class VrHandThrow {

  public:

    ~VrHandThrow();

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
     * \brief Whether the left hand is busy with the weapon
     *
     * Holding its pin, or holding the lighter lit.
     */
    bool leftHandBusy() const {
      return m_throwsWithHand && (m_arm.busy() || m_lit);
    }

  private:

    /**
     * \brief A throw waiting for the game's update of the player
     */
    struct PendingThrow {
      const uint8_t* form = nullptr;
      VrThrowRelease release;
      /// Where the player was when the hand let go, in the game world
      VrVector3      player;
      float          handSpeed = 0.0f;
      float          age = 0.0f;
    };

    /**
     * \brief A projectile thrown by hand, followed for the log
     */
    struct WatchedThrow {
      uint32_t       id = 0;
      VrVector3      origin;
      std::chrono::steady_clock::time_point start;
      uint32_t       logged = 0;
    };

    VrHandThrow(VrWeaponHand& weaponHand, bool firesInUpdate)
    : m_weaponHand(weaponHand), m_firesInUpdate(firesInUpdate) { }

    VrWeaponHand&  m_weaponHand;
    VrThrowConfig  m_config;
    VrThrowArm     m_arm;
    VrThrowTracker m_tracker;

    // Whether throws are fired in the game's update of the player
    bool           m_firesInUpdate = false;

    // The weapon armed, compared but not dereferenced
    const uint8_t* m_form = nullptr;

    bool           m_throwsWithHand = false;
    bool           m_holding = false;
    bool           m_lit     = false;

    // Time left until the next weapon is shown in the hand after a throw
    float          m_refill = 0.0f;

    bool           m_hasPending = false;
    PendingThrow   m_pending;
    WatchedThrow   m_watched;

    uint32_t       m_loggedThrows  = 0;
    uint32_t       m_loggedSlow    = 0;
    uint32_t       m_loggedUnarmed = 0;
    uint32_t       m_loggedArms    = 0;

    void startWeapon(const VrHeldThrown& held, VrThrowArming arming);

    VrThrowArmInput armInput(
      const VrHandThrowInput&     input,
      const VrHeldThrown&         held) const;

    void fire(const PendingThrow& pending);

    void firePending();

    void watchThrown();

    void logArm(VrArmEvent event);

    static void __fastcall postAnimationHook(
            uint8_t*              actor,
            void*                 unused);

  };

}
