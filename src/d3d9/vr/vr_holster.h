#pragma once

#include <cstdint>

#include "vr_game_space.h"

namespace dxvk {

  /**
   * \brief Where a weapon is holstered, see findings/quick-menu.md §6
   */
  enum class VrHolsterKind {
    /// Not holstered by hand: no weapon, or one worn on the fist
    None,
    /// At the right hip: one-handed weapons, grenades, mines, thrown weapons
    Hip,
    /// Behind the back, over the right shoulder: rifles, heavy weapons and
    /// two-handed melee weapons
    Back,
  };

  /**
   * \brief Where a weapon of a type is holstered
   *
   * \param [in] type The weapon's animation type (0 HandToHandMelee to
   *    13 OneHandThrown), or -1 without a weapon
   */
  VrHolsterKind vrHolsterForWeaponType(int32_t type);

  /**
   * \brief Extent of the holsters, in metres in the head's frame, the
   *    head's heading only
   */
  struct VrHolsterZones {
    /// The hip: right of the head's centre, well below it, not far in
    /// front of it nor behind it
    float hipRightMin   = 0.08f;
    float hipRightMax   = 0.50f;
    float hipBelowMin   = 0.40f;
    float hipBelowMax   = 1.00f;
    float hipAhead      = 0.30f;
    float hipBehind     = 0.30f;
    /// The back, over the right shoulder: behind the head, not far to its
    /// left, not much below it and within reach of it (the mirror of the
    /// magazine pouch, VrMagazinePouch)
    float backBehind    = 0.05f;
    float backLeft      = 0.05f;
    float backBelow     = 0.35f;
    float backReach     = 0.60f;
  };

  /**
   * \brief Whether a hand is at a holster
   *
   * \param [in] head Head pose in the game world (Z up)
   * \param [in] hand Hand position in the game world
   * \param [in] kind The holster
   * \param [in] zones Extent of the holsters
   * \returns \c false for VrHolsterKind::None
   */
  bool vrIsAtHolster(
    const VrGameCameraPose&     head,
    const VrVector3&            hand,
          VrHolsterKind         kind,
    const VrHolsterZones&       zones);

  /**
   * \brief What the right hand does with the holster in a frame
   */
  struct VrHolsterInput {
    /// The head and the right hand are tracked in game, with the weapon
    /// handled by the hands
    bool             active = false;
    VrGameCameraPose head;
    VrVector3        hand;
    /// The right grip is held
    bool             grip   = false;
    /// The holster of the equipped weapon
    VrHolsterKind    kind   = VrHolsterKind::None;
    /// The weapon is in the hand
    bool             drawn  = false;
  };

  enum class VrHolsterEvent {
    None,
    /// The hand came to the holster, where a press of the grip holsters or
    /// draws
    Entered,
    /// The weapon in the hand was put in its holster
    Holstered,
    /// The weapon was taken out of its holster
    Drawn,
  };

  /**
   * \brief Holstering and drawing the weapon with the right hand
   *
   * A press of the right grip with the hand at the equipped weapon's
   * holster puts the weapon in it if it is in the hand, or takes it out.
   * A grip already held when the hand comes to the holster does nothing
   * until it is pressed again.
   */
  class VrHolsterHand {

  public:

    VrHolsterHand() = default;

    explicit VrHolsterHand(const VrHolsterZones& zones)
    : m_zones(zones) { }

    VrHolsterEvent update(const VrHolsterInput& input);

    /**
     * \brief Whether the hand is at the holster
     *
     * Set by the last update. A grip pressed there is taken by the
     * holster, not by the game.
     */
    bool atHolster() const {
      return m_atHolster;
    }

  private:

    VrHolsterZones m_zones;
    bool           m_atHolster = false;
    bool           m_gripDown  = false;

  };

}
