#pragma once

#include <memory>

#include "vr_power_attack.h"

namespace dxvk {

  /**
   * \brief What the game allows and costs for a power attack
   *
   * The player charges a power attack by holding the trigger with a melee
   * weapon swung with the hand, see findings/melee.md §12. This reads the
   * game's own rules for it: the time the game's attack is held to make a
   * power attack, the game time of the frame, and when the game refuses a
   * power attack (overencumbered, swimming). On top of them, a power attack
   * costs action points that grow with the weapon's weight, as in
   * lStewieAl's Tweaks, and is refused without enough of them outside of
   * god mode.
   *
   * Only used on the game's main thread.
   */
  class VrPowerAttackRules {

  public:

    /**
     * \brief Finds the game's functions and data
     * \returns \c nullptr if the executable is not the supported version
     */
    static std::unique_ptr<VrPowerAttackRules> install(const VrPowerAttackCost& cost);

    /**
     * \brief Time the game's attack control is held to make a power attack
     *
     * The game's fPowerAttackDelay, in seconds.
     */
    float holdTime() const;

    /**
     * \brief Game time of this frame, in seconds
     *
     * The step the game's own power timer adds, after the time multiplier.
     */
    float frameTime() const;

    /**
     * \brief Whether the player may make a power attack now
     *
     * \param [out] cost Action points the power attack costs
     */
    bool allowed(int32_t& cost) const;

    /**
     * \brief Takes a power attack's action points from the player
     */
    void spend(int32_t cost) const;

  private:

    explicit VrPowerAttackRules(const VrPowerAttackCost& cost)
    : m_cost(cost) { }

    VrPowerAttackCost m_cost;

    float weaponWeight(const uint8_t* player) const;

  };

}
