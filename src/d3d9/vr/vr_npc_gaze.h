#pragma once

#include <memory>

#include "../../util/thread.h"

#include "vr_types.h"

namespace dxvk {

  /**
   * \brief Makes the other actors look at the headset
   *
   * Actors turn their head and eyes towards their head-tracking target
   * (Actor::ProcessTracking aims the ragdoll's look-at IK at the target's
   * GetLookingAtLocation). For the player in first person the game gives
   * the position of its first-person camera node, at the character's eye
   * height and blind to the headset, see findings/npc-gaze.md. The player's
   * entry of that virtual function is replaced, so that it gives the
   * position of the head in the game world instead. In third person, or
   * while no head pose is known, the game's own function answers.
   *
   * The head is set once per frame on the game's main thread. The game may
   * ask for it from its other threads, so it is read under a lock.
   */
  class VrNpcGaze {

  public:

    ~VrNpcGaze();

    /**
     * \brief Replaces the player's look-at location
     * \returns \c nullptr if the executable is not the supported version
     */
    static std::unique_ptr<VrNpcGaze> install();

    /**
     * \brief Sets the head for the frame
     *
     * \param [in] head Position of the head in the game world, or
     *    \c nullptr to leave the location to the game
     */
    void setHead(const VrVector3* head);

  private:

    VrNpcGaze() = default;

    dxvk::mutex     m_mutex;
    VrVector3       m_head;
    bool            m_hasHead = false;
    bool            m_loggedHead = false;

    bool head(VrVector3& head);

    static float* __fastcall lookingAtHook(
            uint8_t*              player,
            void*                 unused,
            float*                result);

  };

}
