#pragma once

#include <cstddef>
#include <cstdint>

#include "vr_game_space.h"
#include "vr_weapon_aim.h"

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
    /// Held by the left hand in the weapon's well, sliding along it
    Seating,
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
    /// The magazine was taken out of the weapon: it slides out of its well
    Ejected,
    /// The magazine came out of the well, it is free in the hand
    Cleared,
    /// The magazine in the hand entered the well
    Engaged,
    /// A new magazine was taken behind the back
    Taken,
    /// The hand reached behind the back, but no rounds are left
    NoRounds,
    /// The magazine in the hand was let go of
    Dropped,
    /// The magazine was pushed home into the weapon
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
    /// Left hand: the middle of the grip
    VrVector3        hand;
    /// Where the centre of a magazine held by the left hand is
    VrVector3        held;
    /// Where the centre of the magazine sits in the weapon
    VrVector3        well;
    /// Direction the magazine comes out of the weapon along, unit
    VrVector3        axis;
    /// Head pose
    VrGameCameraPose head;
    /// The inventory holds rounds for the weapon
    bool             roundsHeld = false;
  };

  /**
   * \brief Distances of the magazine gestures, in metres
   */
  struct VrMagazineReach {
    /// How close the hand must be to the magazine, or to the way it comes
    /// out of the weapon, to take it out
    float eject   = 0.12f;
    /// How far that way goes from the magazine's centre
    float below   = 0.15f;
    /// How far the magazine slides along its well, from its place to the
    /// mouth of the well
    float depth   = 0.03f;
    /// How far past the mouth of the well the magazine is pulled before it
    /// comes out
    float slack   = 0.02f;
    /// How close to the way into the well a magazine in the hand must come
    /// to enter it
    float capture = 0.06f;
    /// How far that way goes beyond the mouth of the well
    float approach = 0.10f;
    /// How far the hand may move across the well before the magazine comes
    /// out of it sideways
    float leave   = 0.06f;
    /// How far a magazine just taken out must slide before pushing it back
    /// puts it in again
    float armed   = 0.01f;
    /// How far past that capture a magazine that came out of the well must
    /// move before it can enter again
    float rearm   = 0.02f;
    /// Where new magazines are taken
    VrMagazinePouch pouch;
  };

  /**
   * \brief Follows the magazine of the weapon in the right hand
   *
   * The left grip pressed near the magazine, or below it along the way it
   * comes out, takes it out: it slides along
   * its well with the hand, and comes free past the mouth of the well or
   * when the hand moves across it. It stays in the hand while the grip is
   * held. Let go of, it disappears. With no magazine in the weapon, the
   * left grip pressed behind the back takes a new one if rounds are left.
   * A magazine in the hand brought near the way into the well enters it
   * at its mouth,
   * and goes in once it is pushed home. Only a press of the grip takes a
   * magazine: holding it while the hand moves takes nothing.
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

    /**
     * \brief How far the magazine is out of its place along its well
     *
     * In metres, while it is VrMagazineState::Seating.
     */
    float seat() const {
      return m_seat;
    }

  private:

    VrMagazineReach m_reach;
    VrMagazineState m_state     = VrMagazineState::InWeapon;
    bool            m_gripDown  = false;
    bool            m_canInsert = false;
    bool            m_canEnter  = false;

    // While seating: how far out the magazine is, and where the held
    // magazine was along and across the well when it entered it, so that
    // the hand moves it from there
    float           m_seat       = 0.0f;
    float           m_alongStart = 0.0f;
    VrVector3       m_acrossStart;

    void seat(const VrMagazineInput& input, float start);

  };

  /**
   * \brief Where the magazine is at a time of the game's reload animation
   */
  struct VrMagazineSample {
    float     time = 0.0f;
    /// The magazine's centre relative to its place in the weapon, in
    /// metres, in the weapon's frame
    VrVector3 offset;
  };

  /**
   * \brief When the game's reload animation moves the magazine
   */
  struct VrMagazineTiming {
    /// The magazine leaves its place
    float     out  = 0.0f;
    /// The magazine is back in its place
    float     in   = 0.0f;
    /// The left hand holds the new magazine, before pushing it in
    float     grip = 0.0f;
    /// Direction the new magazine comes from, towards its place: the
    /// direction it goes out along, unit, in the weapon's frame
    VrVector3 axis;
  };

  /**
   * \brief Finds when the game's reload animation moves the magazine
   *
   * The magazine leaves its place when it first moves by more than a
   * centimetre, and is back when it last comes within a centimetre of it.
   * The hand holds the new magazine at the last time before that it was
   * about four centimetres out, or less if it never went that far.
   *
   * \param [in] samples The magazine through the animation, in time order
   * \param [in] count Number of samples
   * \param [out] timing When the magazine moves
   * \returns \c false if the magazine does not leave its place and come
   *    back to it
   */
  bool vrFindMagazineTiming(
    const VrMagazineSample*     samples,
          size_t                count,
          VrMagazineTiming&     timing);

  /**
   * \brief Finds when the game's reload animation holds the magazine best
   *
   * The left hand holds the magazine while it carries it: from one sample
   * to the next, the magazine moves by less than a few millimetres and
   * turns by less than a few degrees in the hand's frame. Of those times,
   * the hand holds it best where its centre is nearest the hand bone, in
   * the palm rather than at the fingertips or pushed in by the palm.
   *
   * \param [in] inHand The magazine's centre (in metres) and rotation in
   *    the left hand bone's frame, sampled 60 times per second
   * \param [in] count Number of samples
   * \returns The sample, or \c count if the hand never carries the
   *    magazine
   */
  size_t vrFindMagazineHold(
    const VrGameTransform*      inHand,
          size_t                count);

  /**
   * \brief Finds the sound played nearest a time
   *
   * \param [in] times Times of the sounds
   * \param [in] count Number of sounds
   * \param [in] time Time to look near
   * \param [in] window How far from the time a sound may be
   * \param [in] exclude A sound not to pick, or -1
   * \returns The sound's index, or -1 if none is that near
   */
  int32_t vrFindNearestSound(
    const float*                times,
          size_t                count,
          float                 time,
          float                 window,
          int32_t               exclude);

}
