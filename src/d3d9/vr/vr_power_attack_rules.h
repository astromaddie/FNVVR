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
   * god mode, which the player is told with V.A.T.S.'s words.
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
     * \param [out] lacksActionPoints Whether it is refused for its action
     *    points alone
     */
    bool allowed(int32_t& cost, bool& lacksActionPoints) const;

    /**
     * \brief Tells the player that a power attack lacks action points
     *
     * As V.A.T.S. says it by its action point meter, in a HUD message, with
     * the sound the inventory plays when action points are short.
     */
    void tellLowActionPoints() const;

    /**
     * \brief Takes a power attack's action points from the player
     */
    void spend(int32_t cost) const;

    /**
     * \brief The player's current action points
     */
    float actionPoints() const;

  private:

    explicit VrPowerAttackRules(const VrPowerAttackCost& cost)
    : m_cost(cost) { }

    VrPowerAttackCost m_cost;
    bool              m_tellsLowActionPoints = false;

    float weaponWeight(const uint8_t* player) const;

  };

}
