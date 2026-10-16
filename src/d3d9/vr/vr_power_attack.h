#pragma once

#include <cstdint>

#include "vr_types.h"

namespace dxvk {

  /**
   * \brief Action points a power attack costs
   *
   * The cost grows with the weapon's weight, as in lStewieAl's Tweaks
   * (charged attacks cost action points), five times its values: the game
   * restores action points quickly outside of V.A.T.S., see
   * findings/melee.md §12.2 and §12.4.
   */
  struct VrPowerAttackCost {
    /// Cost of a weightless weapon
    int32_t base = 25;
    /// Highest cost, whatever the weight
    int32_t max = 150;
    /// Action points added per unit of the weapon's weight
    float   weightMult = 10.0f;
  };

  /**
   * \brief Action points a power attack with a weapon costs
   *
   * \param [in] cost Cost settings
   * \param [in] weight Weight of the weapon, as the inventory counts it
   */
  int32_t vrComputePowerAttackCost(const VrPowerAttackCost& cost, float weight);

  /**
   * \brief Charge of a power attack, built up while the attack is held
   *
   * Follows the game's power timer (PlayerCharacter::CheckUserInputAttacks,
   * findings/melee.md §12.1): the frame the attack is pressed starts it at
   * zero, each later frame it stays held adds that frame's game time, and
   * the attack is charged once the time exceeds the game's hold time. The
   * charge is kept while the attack stays held, until a hit spends it; the
   * next charge then starts from zero, as the game's timer does after a
   * power attack. While the game would not allow a power attack, nothing
   * builds up.
   */
  class VrPowerCharge {

  public:

    /**
     * \brief Forgets the charge
     */
    void reset();

    /**
     * \brief Follows the attack for a frame
     *
     * \param [in] held Whether the attack is held in this frame
     * \param [in] allowed Whether the game would allow a power attack now
     * \param [in] dt Game time since the previous frame, in seconds
     * \param [in] holdTime Time the game's attack is held to make a power
     *    attack, in seconds
     */
    void update(bool held, bool allowed, float dt, float holdTime);

    /**
     * \brief Spends the charge on a hit
     */
    void spend();

    /**
     * \brief Whether a charge is building up or ready
     */
    bool charging() const {
      return m_held && m_allowed;
    }

    /**
     * \brief Whether the next hit is a power attack
     */
    bool charged() const {
      return charging() && m_time > m_holdTime;
    }

    /**
     * \brief How full the charge is, from 0 to 1
     */
    float fill() const;

  private:

    bool  m_held     = false;
    bool  m_allowed  = false;
    float m_time     = 0.0f;
    float m_holdTime = 0.0f;

  };

  /**
   * \brief Draws the charge as a ring
   *
   * The ring fills clockwise from the top, in the amber of the Pip-Boy's
   * screen, over a faint track; a charged attack shows it whole and fully
   * opaque. Outside the ring the image is transparent.
   *
   * \param [out] pixels Square image, rows from the top, each pixel
   *    0xAARRGGBB with straight alpha
   * \param [in] size Width and height of the image, in pixels
   * \param [in] fill How full the ring is, from 0 to 1
   * \param [in] charged Whether the attack is charged
   */
  void vrDrawChargeRing(uint32_t* pixels, uint32_t size, float fill, bool charged);

  /**
   * \brief Where the charge ring is shown and how large it is
   */
  struct VrChargeRingLayout {
    /// Beside the outer side of the right hand, in metres along the hand's
    /// forward, up and right axes (see vrComputeControllerPoint), clear of
    /// the weapon's grip
    constexpr static VrVector3 HandOffset = { 0.0f, 0.03f, 0.08f };
    /// Width of the ring's panel, in metres
    constexpr static float Width = 0.06f;
    /// Size of the ring's image, in pixels
    constexpr static uint32_t ImageSize = 128;
  };

  /**
   * \brief Pose of the charge ring's panel beside a hand
   *
   * The panel is upright and faces the head.
   *
   * \param [in] hand Controller of the hand holding the weapon
   * \param [in] head Head pose in tracking space
   */
  VrPose vrComputeChargeRingPose(const VrControllerState& hand, const VrPose& head);

}
