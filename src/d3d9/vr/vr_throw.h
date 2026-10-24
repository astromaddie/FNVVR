#pragma once

#include <array>
#include <cstdint>

#include "vr_types.h"

namespace dxvk {

  /**
   * \brief What the left hand does to a thrown weapon before it can be thrown
   *
   * See findings/throw.md §2. The weapon is told by its type, the parts of
   * its model and its model's file name.
   */
  enum class VrThrowArming : uint32_t {
    /// Thrown as it is: thrown weapons, and grenades whose model shows
    /// nothing to act on
    None,
    /// The left hand grips the pin and pulls it out
    Pin,
    /// The left hand holds the lighter's flame to the fuse
    Fuse,
    /// The left index finger presses the device's button
    Button,
  };

  /**
   * \brief What a part of a thrown weapon's model is, from its name
   */
  enum class VrThrowPart : uint32_t {
    None,
    /// The pin a grenade is armed by pulling
    Pin,
    /// The lighter of the dynamite and the fire bomb, held in the left hand
    Lighter,
    /// The lighter's flame, below the lighter
    Flame,
    /// A button, switch, dial or plunger pressed to arm the device
    Button,
    /// A mine's light, shown lit once it is armed
    LightOn,
    /// A mine's light, shown unlit until it is armed
    LightOff,
  };

  /**
   * \brief Which parts a thrown weapon's model has
   */
  struct VrThrowParts {
    bool pin     = false;
    bool lighter = false;
    bool button  = false;
  };

  /**
   * \brief Distances and speeds of a throw
   *
   * Named constants until they are tuned in the headset.
   */
  struct VrThrowConfig {
    /// How far from the pin's bound the left hand takes it, in metres
    float pinReach = 0.06f;
    /// How much further the hand pulls the pin out, in metres
    float pinPull = 0.08f;
    /// How far from the fuse's bound the lighter's flame lights it, in
    /// metres
    float fuseReach = 0.03f;
    /// How long the flame is held to the fuse, in seconds
    float fuseTime = 0.3f;
    /// How far from the button's bound the left index fingertip presses
    /// it, in metres
    float buttonTouch = 0.02f;
    /// How far from the button's bound a click of the left trigger
    /// presses it, from the left hand, in metres
    float buttonReach = 0.06f;
    /// Slowest hand that throws when the trigger is released, in m/s
    float minSpeed = 2.0f;
    /// Hand speed that gives the game's full throw, in m/s
    float fullThrowHandSpeed = 6.0f;
    /// Slowest hand whose motion gives the direction a device is placed
    /// along, in m/s
    float placeDirectionSpeed = 0.3f;
    /// Span of the hand's motion that gives the speed at the release, in
    /// seconds
    float velocityWindow = 0.05f;
  };

  /**
   * \brief What a part of a thrown weapon's model is
   *
   * \param [in] name The object's name, or \c nullptr. A geometry's name
   *    ends with ':' and its index, which is left out.
   */
  VrThrowPart vrThrowPartKind(const char* name);

  /**
   * \brief How a weapon is armed by hand
   *
   * A pin, then a lighter, then a button found in the model decide;
   * without them mines are armed by a button anywhere on them, and
   * grenades by their model's file name (dynamite fuse, frag pin, plasma
   * and pulse button).
   *
   * \param [in] type The weapon's type (WeaponAnimationType, 10 to 13 are
   *    grenades, mines, lunchbox mines and thrown weapons)
   * \param [in] model The path of the weapon's model, or \c nullptr
   * \param [in] parts The parts found in the model
   * \returns How the weapon is armed; weapons that are not grenades or
   *    mines are never armed
   */
  VrThrowArming vrThrowArming(int32_t type, const char* model, const VrThrowParts& parts);

  /**
   * \brief Whether a weapon type is placed rather than thrown
   *
   * Mines and lunchbox mines leave the hand at a press of the trigger,
   * as fast as the hand moves, however slow.
   */
  bool vrThrowIsPlaced(int32_t type);

  /**
   * \brief Returns a name for the log
   */
  const char* vrThrowArmingName(VrThrowArming arming);

  /**
   * \brief What arming did in a frame
   */
  enum class VrArmEvent : uint32_t {
    None,
    /// The left hand took the pin
    PinTaken,
    /// The left hand let go of the pin before it was out
    PinDropped,
    /// The lighter's flame reached the fuse
    Lighting,
    /// The weapon is ready to be thrown
    Armed,
  };

  /**
   * \brief The left hand and what it acts on, for a frame
   *
   * Positions in metres, in any frame shared by all of them.
   */
  struct VrThrowArmInput {
    /// Left hand, where it grips
    VrVector3 hand;
    /// Tip of the left index finger
    VrVector3 fingertip;
    /// The lighter's flame
    VrVector3 flame;
    /// Centre and radius of the part acted on: the pin where it sits in
    /// the grenade, the fuse's stick or bottle, the button
    VrVector3 target;
    float     radius = 0.0f;
    /// Whether the left grip and the left trigger are held
    bool      grip    = false;
    bool      trigger = false;
    /// Time since the previous frame, in seconds
    float     dt = 0.0f;
  };

  /**
   * \brief Arms the thrown weapon in the right hand with the left hand
   *
   * A pin is taken with the left grip at the pin and pulled away from
   * where it sits; the lighter's flame, lit by the left trigger, is held
   * at the fuse for a moment; a button is pressed by the left index
   * fingertip, or by a click of the left trigger with the hand at it. A
   * weapon with nothing to act on is armed from the start.
   */
  class VrThrowArm {

  public:

    /**
     * \brief Starts over with a new weapon in the hand
     */
    void reset(VrThrowArming arming);

    /**
     * \brief Follows the left hand for a frame
     */
    VrArmEvent update(const VrThrowArmInput& input, const VrThrowConfig& config);

    VrThrowArming arming() const {
      return m_arming;
    }

    bool armed() const {
      return m_armed;
    }

    /**
     * \brief Whether the left hand holds the pin
     *
     * Once the pin is out, until the grip lets go of it.
     */
    bool holdsPin() const {
      return m_holdsPin;
    }

    /**
     * \brief Whether the left hand is busy with the device
     */
    bool busy() const {
      return m_holdsPin || m_lighting;
    }

  private:

    VrThrowArming m_arming   = VrThrowArming::None;
    bool          m_armed    = true;
    bool          m_holdsPin = false;
    // How far the hand was from the pin when it took it
    float         m_pinTaken = 0.0f;
    // The flame is at the fuse, for this long
    bool          m_lighting = false;
    float         m_lit      = 0.0f;
    // The grip and the trigger as they were on the previous frame, so that
    // only a press made at the device acts on it
    bool          m_grip     = true;
    bool          m_trigger  = true;

  };

  /**
   * \brief Follows the hand's motion to give its speed when it lets go
   *
   * Positions are taken in the player's body frame, which moves and turns
   * with the player, so that only the arm's own motion counts, as the
   * melee swing does.
   */
  class VrThrowTracker {

  public:

    /**
     * \brief Forgets the motion
     */
    void reset();

    /**
     * \brief Adds the hand's position in a frame
     *
     * \param [in] position Hand position in the body frame, in metres
     * \param [in] dt Time since the previous frame, in seconds
     */
    void add(const VrVector3& position, float dt);

    /**
     * \brief Velocity over the last part of the motion
     *
     * \param [in] window Span of the motion, in seconds; at least one
     *    frame is used, however short the window
     * \returns The velocity in metres per second, zero until two positions
     *    are known
     */
    VrVector3 velocity(float window) const;

  private:

    static constexpr uint32_t Capacity = 16;

    struct Sample {
      VrVector3 position;
      float     dt = 0.0f;
    };

    std::array<Sample, Capacity> m_samples = { };
    uint32_t m_count = 0;
    uint32_t m_next  = 0;

  };

  /**
   * \brief Share of the game's full throw a throw by hand gets
   *
   * \param [in] handSpeed Speed of the hand when it lets go, in m/s
   * \param [in] config Speeds
   * \returns From 0 to 1 in proportion to the hand's speed, 1 from
   *    config.fullThrowHandSpeed on, and 0 if the hand is too slow to
   *    throw
   */
  float vrComputeThrowShare(float handSpeed, const VrThrowConfig& config);

}
