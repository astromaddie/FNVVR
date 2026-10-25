#pragma once

#include <array>
#include <memory>
#include <vector>

#include "vr_grab.h"
#include "vr_types.h"

namespace dxvk {

  /**
   * \brief What the hands grabbed and let go of since the last call to
   *    VrPhysicsGrab::takeEvents, for the haptics
   */
  struct VrPhysicsGrabEvents {
    std::array<bool, VrHandCount> grabbed  = { };
    std::array<bool, VrHandCount> released = { };
  };

  /**
   * \brief Holds world objects with the hands
   *
   * Each hand holds its own object on a Havok mouse spring of its own, made
   * as the game makes the one of its grab key (findings/grab.md §7, §8):
   * a press of a free hand's grip holds the object the hand touches (the
   * nearest hit of short rays around the hand, see vrGrabProbeRays) by the
   * point touched, and the spring pulls that point to where it belongs in
   * the hand. Objects too heavy for the game's grab, fixed and keyframed
   * bodies, and actors (living or dead) are not held.
   *
   * Havok steps on another thread while the frame is drawn, so the springs
   * are worked in the game's update of the player, after the game's own
   * grab, with the hands the VR frame gave last. While a hand holds an
   * object, the object's collision group is the player's: it neither
   * pushes nor hurts the player and keeps colliding with everything else.
   * The object's own group comes back once it was let go and is clear of
   * the player.
   *
   * As with the game's grab, the object's OnGrab and OnRelease script
   * blocks run, guards see it taken and its new position is saved. The
   * game's IsPlayerGrabbedRef does not know about it.
   *
   * Only used on the game's main thread.
   */
  class VrPhysicsGrab {

  public:

    ~VrPhysicsGrab();

    /**
     * \brief Finds the game's functions and hooks the player's update
     *
     * \returns \c nullptr if the executable is not the supported version
     */
    static std::unique_ptr<VrPhysicsGrab> install();

    /**
     * \brief Gives a hand for the next update of the player
     *
     * \param [in] hand The hand
     * \param [in] pose The hand in the game world, or \c nullptr when it is
     *    not tracked or the player is not in game; copied
     * \param [in] grip The hand's grip is held
     * \param [in] free The hand may hold an object
     */
    void setHand(
            VrHand                hand,
      const VrGameCameraPose*     pose,
            bool                  grip,
            bool                  free);

    /**
     * \brief Whether a hand holds an object
     */
    bool holds(VrHand hand) const {
      return m_hands[uint32_t(hand)].grab.holding();
    }

    /**
     * \brief What the hands grabbed and let go of since the previous call
     */
    VrPhysicsGrabEvents takeEvents();

  private:

    struct HandInput {
      VrGameCameraPose pose;
      bool             tracked = false;
      bool             grip    = false;
      bool             free    = false;
    };

    /**
     * \brief An object held by a hand
     */
    struct Held {
      VrGrabHand grab;
      /// NiPointer<bhkMouseSpringAction>, holds a reference
      void*      spring = nullptr;
      /// hkpRigidBody, kept alive by the spring
      uint8_t*   body   = nullptr;
      /// TESObjectREFR, compared with the form found by its id before use
      uint8_t*   ref    = nullptr;
      uint32_t   refId  = 0;
      /// Point held, in the body's frame, Havok units
      VrVector3  local;
      /// The body's own collision filter, and whether it was changed
      uint32_t   filter = 0;
      bool       grouped = false;
    };

    /**
     * \brief A body let go of whose collision filter is still the player's
     */
    struct Released {
      void*    spring = nullptr;
      uint8_t* body   = nullptr;
      uint32_t filter = 0;
    };

    /**
     * \brief Something a hand may hold, found by the probe
     */
    struct Candidate {
      uint8_t*  body = nullptr;
      uint8_t*  ref  = nullptr;
      VrVector3 point;
      float     distance = 0.0f;
      float     mass     = 0.0f;
    };

    VrPhysicsGrab() = default;

    std::array<HandInput, VrHandCount> m_input;
    std::array<Held, VrHandCount>      m_hands;
    std::vector<Released>              m_released;
    VrPhysicsGrabEvents                m_events;

    uint32_t m_loggedGrabs   = 0;
    uint32_t m_loggedMisses  = 0;
    uint32_t m_loggedLosses  = 0;
    bool     m_loggedSpring  = false;
    bool     m_loggedNoGroup = false;

    void update(uint8_t* player, float dt);

    void updateHand(uint8_t* player, VrHand hand, float dt);

    bool grab(uint8_t* player, VrHand hand, const VrGameCameraPose& pose);

    bool probe(uint8_t* player, VrHand hand, const VrVector3& centre, Candidate& best);

    bool holdable(uint8_t* body, float maxMass, Candidate& candidate) const;

    bool attach(uint8_t* player, Held& held, const Candidate& candidate);

    bool stillHeld(const Held& held) const;

    bool moveTarget(Held& held, const VrGameCameraPose& pose);

    void release(Held& held);

    void restoreFilters(uint8_t* player);

    static void __fastcall physicsGrabHook(
            uint8_t*              player,
            void*                 unused,
            float                 dt);

  };

}
