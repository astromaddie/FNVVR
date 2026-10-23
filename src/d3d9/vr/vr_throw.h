#pragma once

#include <array>
#include <cstdint>

#include "vr_types.h"

namespace dxvk {

  /**
   * \brief What the left hand does to a thrown weapon before it can be thrown
   *
   * See findings/throw.md §3. The weapon is told by its type and its
   * model's file name.
   */
  enum class VrThrowArming : uint32_t {
    /// Thrown as it is: thrown weapons, and grenades whose model shows
    /// nothing to act on
    None,
    /// The left hand grips the pin and pulls it out
    Pin,
    /// The left hand holds the lighter to the fuse
    Fuse,
    /// The left hand presses the device's button
    Button,
  };

  /**
   * \brief Distances and speeds of a throw
   *
   * Named constants until they are tuned in the headset.
   */
  struct VrThrowConfig {
    /// How far the left hand reaches beyond the device's bound, in metres
    float reach = 0.08f;
    /// How far beyond the reach the pin is pulled out, in metres
    float pinPull = 0.10f;
    /// How long the lighter is held to the fuse, in seconds
    float fuseTime = 0.3f;
    /// Slowest hand that throws when the trigger is released, in m/s
    float minSpeed = 2.0f;
    /// Speed of the throw for each metre per second of the hand
    float speedScale = 1.5f;
    /// Span of the hand's motion that gives the speed at the release, in
    /// seconds
    float velocityWindow = 0.05f;
  };

  /**
   * \brief How a weapon is armed by hand
   *
   * \param [in] type The weapon's type (WeaponAnimationType, 10 to 13 are
   *    grenades, mines, lunchbox mines and thrown weapons)
   * \param [in] model The path of the weapon's model, or \c nullptr
   * \returns How the weapon is armed; weapons that are not thrown are
   *    never armed
   */
  VrThrowArming vrThrowArming(int32_t type, const char* model);

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
    /// The lighter reached the fuse
    Lighting,
    /// The weapon is ready to be thrown
    Armed,
  };

  /**
   * \brief Arms the thrown weapon in the right hand with the left hand
   *
   * The device's pin, fuse or button is anywhere on its model: the left
   * hand acts on it when it comes within the reach of the model's bound.
   * A pin is taken with the left grip and pulled away from the model;
   * the lighter, the left trigger, is held near the fuse for a moment; a
   * button is pressed with a click of the left trigger. A weapon with
   * nothing to act on is armed from the start.
   */
  class VrThrowArm {

  public:

    /**
     * \brief Starts over with a new weapon in the hand
     */
    void reset(VrThrowArming arming);

    /**
     * \brief Follows the left hand for a frame
     *
     * \param [in] hand Left hand's position, in metres
     * \param [in] centre Centre of the weapon's bound, in metres
     * \param [in] radius Radius of the weapon's bound, in metres
     * \param [in] grip Whether the left grip is held
     * \param [in] trigger Whether the left trigger is held
     * \param [in] dt Time since the previous frame, in seconds
     * \param [in] config Distances
     */
    VrArmEvent update(
      const VrVector3&            hand,
      const VrVector3&            centre,
            float                 radius,
            bool                  grip,
            bool                  trigger,
            float                 dt,
      const VrThrowConfig&        config);

    VrThrowArming arming() const {
      return m_arming;
    }

    bool armed() const {
      return m_armed;
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
    // The lighter is at the fuse, for this long
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
   * \brief Speed of a throw from the hand's
   *
   * \param [in] handSpeed Speed of the hand when it lets go, in m/s
   * \param [in] config Speeds
   * \returns The speed to throw at in m/s, zero if the hand is too slow
   *    to throw
   */
  float vrComputeThrowSpeed(float handSpeed, const VrThrowConfig& config);

}
