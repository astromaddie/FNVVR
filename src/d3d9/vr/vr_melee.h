#pragma once

#include <cstdint>
#include <vector>

#include "vr_game_space.h"

namespace dxvk {

  /**
   * \brief Segment between two points in the game world
   */
  struct VrSegment {
    VrVector3 a;
    VrVector3 b;
  };

  /**
   * \brief Blade of a melee weapon in the frame of the hand holding it
   *
   * Each point is given by its distances along the hand's forward, up and
   * right axes from the hand's position, in game units.
   */
  struct VrBlade {
    VrVector3 base;
    VrVector3 tip;
  };

  /**
   * \brief Speeds that make a swing hit, see findings/melee.md §9
   */
  struct VrSwingConfig {
    /// Blade tip speed above which a contact hits, in metres per second
    float hitSpeed   = 2.0f;
    /// Blade tip speed that makes the hit a power attack, in metres per second
    float powerSpeed = 5.0f;
  };

  /**
   * \brief Turns a point from a hand's frame to the game world
   *
   * \param [in] hand Hand pose in the game world
   * \param [in] local Distances along the hand's forward, up and right axes
   */
  VrVector3 vrPointFromHand(const VrGameCameraPose& hand, const VrVector3& local);

  /**
   * \brief Turns a point from the game world to a hand's frame
   */
  VrVector3 vrPointToHand(const VrGameCameraPose& hand, const VrVector3& world);

  /**
   * \brief Measures the blade of the weapon held in a hand
   *
   * The blade goes from the hand towards the centre of the weapon's bounding
   * sphere, up to the far side of the sphere. This needs no table of weapons:
   * the bound of a sword is centred on its blade, the bound of an axe or a
   * hammer leans towards its head.
   *
   * \param [in] hand Pose of the hand holding the weapon
   * \param [in] centre Centre of the weapon's bounding sphere
   * \param [in] radius Radius of the weapon's bounding sphere
   * \param [out] blade The blade in the hand's frame
   * \returns \c false if the bound is too small or too large for a weapon
   */
  bool vrMeasureBlade(
    const VrGameCameraPose&     hand,
    const VrVector3&            centre,
          float                 radius,
          VrBlade&              blade);

  /**
   * \brief Places a blade held in a hand in the game world
   */
  VrSegment vrBladeInWorld(const VrGameCameraPose& hand, const VrBlade& blade);

  /**
   * \brief Shortest distance between two segments
   */
  float vrSegmentDistance(const VrSegment& s, const VrSegment& t);

  /**
   * \brief Shortest distance between a moving segment and a fixed one
   *
   * The moving segment's ends go in straight lines from \c from to \c to, and
   * it is compared with \c fixed at \c steps evenly spaced positions after
   * \c from, \c to included, so that a fast swing cannot pass through a
   * thin bone between two frames.
   */
  float vrSweptSegmentDistance(
    const VrSegment&            from,
    const VrSegment&            to,
    const VrSegment&            fixed,
          uint32_t              steps);

  /**
   * \brief Follows the swings of a blade, frame after frame
   *
   * A swing lasts while the blade's tip moves at least as fast as the hit
   * speed. Each target is hit once per swing, and a swing is a power attack
   * once its tip has reached the power speed.
   */
  class VrSwingTracker {

  public:

    explicit VrSwingTracker(const VrSwingConfig& config)
    : m_config(config) { }

    /**
     * \brief Forgets the blade, when it is not held any more
     */
    void reset();

    /**
     * \brief Moves the tip to where it is in this frame
     *
     * \param [in] tip Tip position in the game world, in game units
     * \param [in] dt Time since the previous frame, in seconds
     */
    void update(const VrVector3& tip, float dt);

    /**
     * \brief Speed of the tip in this frame, in metres per second
     */
    float speed() const {
      return m_speed;
    }

    /**
     * \brief Whether the blade moves fast enough to hit
     */
    bool swinging() const {
      return m_hasTip && m_speed >= m_config.hitSpeed;
    }

    /**
     * \brief Whether the current swing makes power attacks
     */
    bool powerful() const {
      return swinging() && m_peakSpeed >= m_config.powerSpeed;
    }

    /**
     * \brief Whether a target was hit during the current swing
     *
     * \param [in] target Compared, never dereferenced
     */
    bool hasHit(const void* target) const;

    /**
     * \brief Records that a target was hit during the current swing
     */
    void addHit(const void* target);

  private:

    VrSwingConfig m_config;

    VrVector3 m_tip;
    bool      m_hasTip    = false;
    float     m_speed     = 0.0f;
    float     m_peakSpeed = 0.0f;

    std::vector<const void*> m_hits;

  };

  /**
   * \brief Whether a blade is held as a guard in front of the head
   *
   * The blade lies close to horizontal, across the front of the head
   * rather than pointing away from it, its middle in front of the face and
   * within reach, between the chest and slightly above the head.
   *
   * \param [in] head Head pose in the game world
   * \param [in] blade Blade in the game world
   */
  bool vrIsGuardPose(const VrGameCameraPose& head, const VrSegment& blade);

  /**
   * \brief Holds the guard while the blade stays in the guard pose
   *
   * The guard starts as soon as the pose is taken and the blade is slow,
   * and ends once the pose has been left for \ref ReleaseTime, so that a
   * shaking hand at the edge of the pose does not drop the guard.
   */
  class VrGuardTracker {

  public:

    /// How long the pose must be left to end the guard, in seconds
    constexpr static float ReleaseTime = 0.15f;

    /**
     * \brief Updates the guard for a frame
     *
     * \param [in] pose Whether the blade is in the guard pose
     * \param [in] swinging Whether the blade moves fast enough to hit
     * \param [in] dt Time since the previous frame, in seconds
     */
    void update(bool pose, bool swinging, float dt);

    /**
     * \brief Ends the guard at once
     */
    void reset();

    /**
     * \brief Whether the guard is held
     */
    bool guarding() const {
      return m_guarding;
    }

  private:

    bool  m_guarding = false;
    float m_outTime  = 0.0f;

  };

}
