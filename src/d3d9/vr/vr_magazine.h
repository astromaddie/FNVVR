#pragma once

#include "vr_game_space.h"

namespace dxvk {

  /**
   * \brief What a named part of a weapon's model is for the manual reload
   *
   * The reload animations move parts of the weapon's model whose names
   * start with "##", and the names differ per weapon, see
   * findings/reload.md §6 and §7 (decision 10).
   */
  enum class VrWeaponPartKind {
    /// Not used by the manual reload
    None,
    /// A detachable magazine or energy cell
    Magazine,
  };

  /**
   * \brief Tells what a part of a weapon's model is from its name
   *
   * A magazine is a part whose name contains "clip", "magazine", "drum",
   * "cell" or "battery", in any case, unless it also names something next to the
   * magazine (its release, latch or bay) or a magazine that is not
   * detachable (a revolver's moon clip). A table of exceptions comes first.
   * The ":n" suffix of a part's geometry is ignored.
   *
   * \param [in] name Name of the object, with its "##" prefix
   */
  VrWeaponPartKind vrClassifyWeaponPart(const char* name);

  /**
   * \brief Where new magazines are taken: behind the back, over the left shoulder
   *
   * Distances in metres in the head's frame, the head's heading only:
   * behind the head, not far to its right, not much below it and within
   * reach of it.
   */
  struct VrMagazinePouch {
    /// How far behind the head the hand must be
    float behind   = 0.05f;
    /// How far right of the head's centre the hand may be
    float right    = 0.05f;
    /// How far below the head the hand may be
    float below    = 0.35f;
    /// How far from the head the hand may be
    float reach    = 0.6f;
  };

  /**
   * \brief Whether a hand is behind the back, over the left shoulder
   *
   * The heading is taken from the head's forward and up axes, so that it
   * stays defined when the player looks straight up or down.
   *
   * \param [in] head Head pose in the game world (Z up)
   * \param [in] hand Hand position in the game world
   * \param [in] pouch Extent of the place, in metres
   */
  bool vrIsAtMagazinePouch(
    const VrGameCameraPose&     head,
    const VrVector3&            hand,
    const VrMagazinePouch&      pouch);

  /**
   * \brief Where the weapon's magazine is
   */
  enum class VrMagazineState {
    /// In the weapon, as the game draws it
    InWeapon,
    /// Held by the left hand, out of the weapon
    InHand,
    /// Neither in the weapon nor in the hand
    Out,
  };

  /**
   * \brief What the left hand did with the magazine in a frame
   */
  enum class VrMagazineEvent {
    None,
    /// The magazine was taken out of the weapon
    Ejected,
    /// A new magazine was taken behind the back
    Taken,
    /// The hand reached behind the back, but no rounds are left
    NoRounds,
    /// The magazine in the hand was let go of
    Dropped,
    /// The magazine in the hand was pushed into the weapon
    Inserted,
  };

  /**
   * \brief What the magazine logic sees in a frame
   *
   * Positions in the game world, in game units.
   */
  struct VrMagazineInput {
    /// The weapon in the right hand has a magazine, and the left hand is
    /// tracked and free (not on the weapon)
    bool             active = false;
    /// The left grip is held
    bool             grip = false;
    /// Left hand: the middle of the grip, where it holds a magazine
    VrVector3        hand;
    /// Where the magazine sits in the weapon
    VrVector3        well;
    /// Head pose
    VrGameCameraPose head;
    /// The inventory holds rounds for the weapon
    bool             roundsHeld = false;
  };

  /**
   * \brief Distances of the magazine gestures, in metres
   */
  struct VrMagazineReach {
    /// How close the hand must be to the magazine to take it out
    float eject   = 0.10f;
    /// How close the magazine must come to its place to go in
    float insert  = 0.07f;
    /// How far a magazine just taken out must move away before it can go
    /// in again, so that it does not go back in at once
    float rearm   = 0.12f;
    /// Where new magazines are taken
    VrMagazinePouch pouch;
  };

  /**
   * \brief Follows the magazine of the weapon in the right hand
   *
   * The left grip pressed near the magazine takes it out, and the magazine
   * stays in the hand while the grip is held. Let go of, it disappears.
   * With no magazine in the weapon, the left grip pressed behind the back
   * takes a new one if rounds are left. A magazine in the hand goes in
   * when it comes near its place in the weapon. Only a press of the grip
   * takes a magazine: holding it while the hand moves takes nothing.
   *
   * The game decides the clip: it is emptied on the way out and filled on
   * the way in by the caller.
   */
  class VrMagazineHand {

  public:

    explicit VrMagazineHand(const VrMagazineReach& reach = VrMagazineReach())
    : m_reach(reach) { }

    /**
     * \brief Takes the frame's input
     * \returns What happened in this frame
     */
    VrMagazineEvent update(const VrMagazineInput& input);

    /**
     * \brief Puts the magazine back in the weapon
     *
     * When the weapon changes, or when the game fills the clip itself
     * (equipping, V.A.T.S., an ammunition swap...).
     */
    void reset();

    VrMagazineState state() const {
      return m_state;
    }

  private:

    VrMagazineReach m_reach;
    VrMagazineState m_state     = VrMagazineState::InWeapon;
    bool            m_gripDown  = false;
    bool            m_canInsert = false;

  };

}
