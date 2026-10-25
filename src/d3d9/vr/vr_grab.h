#pragma once

#include <array>
#include <cstdint>

#include "vr_game_space.h"

namespace dxvk {

  /**
   * \brief A ray of the contact probe, in game units
   */
  struct VrGrabRay {
    VrVector3 from;
    VrVector3 to;
  };

  /**
   * \brief Number of rays of the contact probe
   */
  constexpr uint32_t VrGrabProbeRayCount = 14;

  /**
   * \brief Rays that find what a hand touches
   *
   * Each ray starts on a sphere around the hand and ends at its centre:
   * along the six axes and towards the eight corners of a cube. A ray
   * meets a surface that crosses the sphere on its way, and the outside of
   * a body that holds the centre, which a ray starting inside the body
   * would not see.
   *
   * \param [in] centre Centre of the hand, in the game world
   * \param [in] radius How far the hand reaches, in game units
   */
  std::array<VrGrabRay, VrGrabProbeRayCount> vrGrabProbeRays(
    const VrVector3&            centre,
          float                 radius);

  /**
   * \brief Whether a released body is far enough from the player to
   *    collide with the player again
   *
   * Only the horizontal distance counts: the player's capsule stands
   * upright on the player's position.
   *
   * \param [in] body Centre of the body, in the game world
   * \param [in] player The player's position
   * \param [in] distance Horizontal distance needed, in game units
   */
  bool vrGrabClearOfPlayer(
    const VrVector3&            body,
    const VrVector3&            player,
          float                 distance);

  /**
   * \brief What a hand does with its grip in a frame
   */
  struct VrGrabHandInput {
    /// The hand in the game world, or \c nullptr when it is not tracked or
    /// the player is not in game
    const VrGameCameraPose* hand = nullptr;
    /// The hand's grip is held
    bool  grip = false;
    /// The hand may hold an object: not holding the weapon, nor busy with
    /// something else
    bool  free = false;
    /// Time since the previous frame, in seconds
    float dt   = 0.0f;
  };

  enum class VrGrabRequest {
    None,
    /// Look for something to hold at the hand
    Grab,
    /// Let go of what the hand holds
    Release,
  };

  /**
   * \brief Grabbing and letting go with one hand's grip
   *
   * A press of the grip while the hand is free grabs what the hand touches.
   * The hand keeps looking for a short time after the press, so that a
   * touch a moment late still counts, but a grip closed for longer grabs
   * nothing until it is pressed again. What the hand holds is let go when
   * the grip is released, when the hand stops being free and when it is
   * lost.
   *
   * The held point keeps its place in the hand's frame, so that the object
   * follows the hand's moves and turns. A point pulled from a distance
   * comes to the hand's centre at a set speed, along the line it had from
   * the hand in the hand's frame, and is then held there.
   */
  class VrGrabHand {

  public:

    /// How long a press keeps looking for something to hold, in seconds
    constexpr static float GrabWindow = 0.25f;

    /// How fast a pulled point comes to the hand, in game units per second
    constexpr static float PullSpeed = 4.0f * VrGameUnitsPerMetre;

    /**
     * \brief Decides what the hand does in a frame
     *
     * After a Release, the caller lets go and calls \ref drop. After a
     * Grab, it calls \ref hold or \ref pull if it found something. While
     * the hand holds, moves a pulled point closer to the hand.
     */
    VrGrabRequest update(const VrGrabHandInput& input);

    /**
     * \brief Starts holding a point
     *
     * \param [in] hand The hand in the game world
     * \param [in] point Point held, in the game world
     */
    void hold(const VrGameCameraPose& hand, const VrVector3& point);

    /**
     * \brief Starts pulling a point to the hand's centre
     *
     * \param [in] hand The hand in the game world
     * \param [in] point Point pulled, in the game world
     */
    void pull(const VrGameCameraPose& hand, const VrVector3& point);

    /**
     * \brief Stops holding
     *
     * Also when the object was lost. The grip has to be pressed again to
     * grab.
     */
    void drop();

    bool holding() const {
      return m_holding;
    }

    /**
     * \brief Whether the held point is still on its way to the hand
     */
    bool pulling() const {
      return m_holding && m_pulled;
    }

    /**
     * \brief Where the held point belongs for a hand pose
     */
    VrVector3 target(const VrGameCameraPose& hand) const;

  private:

    bool      m_holding  = false;
    bool      m_gripDown = false;
    bool      m_pulled   = false;
    float     m_window   = 0.0f;
    VrVector3 m_offset;

    void advancePull(float dt);

  };

}
