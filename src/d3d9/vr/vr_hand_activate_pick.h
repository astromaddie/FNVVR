#pragma once

#include <array>
#include <memory>

#include "vr_hand_hud.h"
#include "vr_types.h"

namespace dxvk {

  /**
   * \brief Ray a hand points along, in the game world
   */
  struct VrHandRay {
    VrVector3 origin;     ///< In game units
    VrVector3 direction;  ///< Unit length
  };

  /**
   * \brief Picks what the player would activate along the hands
   *
   * Once per frame, after drawing, the game casts its activation pick along
   * the player's view, which follows the head, see findings/hud.md §10.
   * That call is redirected: the pick is cast along each tracked hand's ray
   * instead, and the hand whose ray finds something names the target
   * (vrChooseActivateHand). The chosen hand's ray is left in the game's
   * variables, so that the water pick and the pick location the game
   * computes after the call follow that hand too. Without tracked hands
   * the game picks along the view as before.
   *
   * Only used on the game's main thread.
   */
  class VrHandActivatePick {

  public:

    ~VrHandActivatePick();

    /**
     * \brief Redirects the game's call
     * \returns \c nullptr if the executable is not the supported version
     */
    static std::unique_ptr<VrHandActivatePick> install();

    /**
     * \brief Sets the hands' rays for the next pick
     *
     * \param [in] rays Ray of each hand in the game world, indexed by
     *    VrHand, or \c nullptr for an untracked hand
     */
    void setRays(const std::array<const VrHandRay*, VrHandCount>& rays);

    /**
     * \brief Whether the last pick was cast along the hands
     */
    bool pickedWithHands() const {
      return m_pickedWithHands;
    }

    /**
     * \brief Hand whose ray named the target on the last pick
     */
    VrHand hand() const {
      return m_hand;
    }

  private:

    VrHandActivatePick() = default;

    std::array<VrHandRay, VrHandCount> m_rays;
    std::array<bool, VrHandCount>      m_tracked = { };

    VrHand m_hand            = VrHand::Right;
    bool   m_pickedWithHands = false;
    bool   m_logged          = false;

    static void* __fastcall findHook(
            void*                 viewCaster,
            void*                 unused,
            VrVector3*            origin,
            VrVector3*            direction,
            float                 distance,
            float*                hitDistance,
            bool*                 fuzzy);

    void* find(
            void*                 viewCaster,
            VrVector3*            origin,
            VrVector3*            direction,
            float                 distance,
            float*                hitDistance,
            bool*                 fuzzy);

  };

}
