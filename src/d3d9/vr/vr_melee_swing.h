#pragma once

#include <memory>

#include "vr_melee.h"

namespace dxvk {

  /**
   * \brief What a swing did in a frame
   */
  struct VrMeleeHit {
    /// Number of actors hit
    uint32_t count = 0;
    /// Whether the hits were power attacks
    bool     power = false;
  };

  /**
   * \brief Hits what the blade of the melee weapon in the hand touches
   *
   * The player swings the weapon held in the hand instead of pressing the
   * attack control, see findings/melee.md §9. Each frame, the blade is
   * swept from where it was on the previous frame to where the hand holds
   * it now, against capsules around the skeleton bones of the loaded
   * actors. Both are taken relative to the player's body, so that only the
   * hand's own motion sweeps and makes speed: running into an actor or
   * turning with the weapon held still does not hit. When it touches one while its tip moves at least at the hit
   * speed, the game's Actor::CombatHit deals the hit as if the player's
   * swing animation had reached it: the game computes the damage,
   * criticals, sounds, reaction and crime. The blade held still across the
   * front of the head is a guard, which holds the game's block control.
   *
   * Only used on the game's main thread.
   */
  class VrMeleeSwing {

  public:

    ~VrMeleeSwing();

    /**
     * \brief Finds the game's functions and data
     * \returns \c nullptr if the executable is not the supported version
     */
    static std::unique_ptr<VrMeleeSwing> install(const VrSwingConfig& config);

    /**
     * \brief Follows the blade for a frame and hits what it touches
     *
     * \param [in] hand Pose of the hand holding the weapon in the game
     *    world, or \c nullptr if it is not tracked or not in game
     * \param [in] blade Blade of the weapon in the hand, or \c nullptr if
     *    the hand holds no melee weapon
     * \param [in] head Head pose in the game world
     * \param [in] body Pose the eyes and hands are placed on in the game
     *    world, which moves and turns with the player
     * \param [in] dt Time since the previous frame, in seconds
     */
    VrMeleeHit update(
      const VrGameCameraPose*     hand,
      const VrBlade*              blade,
      const VrGameCameraPose&     head,
      const VrGameCameraPose&     body,
            float                 dt);

    /**
     * \brief Whether a melee weapon is swung with the hand
     *
     * The game's attack control is then left alone, so that the game does
     * not swing the weapon with its own animation as well.
     */
    bool swingsWithHand() const {
      return m_hasBlade;
    }

    /**
     * \brief Whether the game's block control should be held
     */
    bool wantsBlock() const {
      return m_guard.guarding();
    }

  private:

    explicit VrMeleeSwing(const VrSwingConfig& config)
    : m_swing(config) { }

    VrSwingTracker m_swing;
    VrGuardTracker m_guard;

    // Blade on the previous frame, in the body's frame
    VrSegment      m_blade;
    bool           m_hasBlade = false;

    uint32_t       m_loggedHits = 0;
    uint32_t       m_loggedGuards = 0;

    uint32_t hitActors(uint8_t* player, const VrSegment& from, const VrSegment& to, bool power);

    bool touches(uintptr_t actor, const VrSegment& from, const VrSegment& to) const;

  };

}
