#pragma once

#include "vr_types.h"

namespace dxvk {

  /**
   * \brief Turns the raw actions into the actions the game acts on
   *
   * Two rules apply:
   *
   * - The PauseHold action opens the pause menu once it is held for
   *   \ref HoldTime, and a shorter press opens the wait menu when it is
   *   released (the Wait action). Both menu buttons
   *   of the Quest controllers are taken by SteamVR and the headset's
   *   system, so the pause menu has no button of its own. The separate
   *   Pause action still opens it at once if the user binds it. The
   *   Pip-Boy has no button: it opens when the player looks at it (see
   *   VrPipBoyGaze), which freed the short press for the wait menu. The
   *   separate Wait action also opens it at once if the user binds it.
   *
   * - When the context changes, actions that are already held are ignored
   *   until they are released. The same button means something else in a
   *   menu, so holding it across the change would trigger the menu action
   *   (for example the pause menu's Y right after the long press opened it).
   */
  class VrActionFilter {

  public:

    /// How long the PauseHold action is held to open the pause menu, in seconds
    constexpr static float HoldTime  = 0.6f;
    /// How long a hold or a tap is reported, in seconds, at least one frame
    constexpr static float PulseTime = 0.1f;

    /**
     * \brief Filters the actions of a frame
     *
     * \param [in] actions Actions read by the backend
     * \param [in] context Context the actions were read in
     * \param [in] dt Time since the previous frame, in seconds
     * \returns Actions to pass to the game
     */
    VrActionState update(
      const VrActionState&        actions,
            VrInputContext        context,
            float                 dt);

  private:

    VrInputContext m_context = VrInputContext::Game;
    uint32_t       m_blocked = 0;

    bool  m_holdDown    = false;
    bool  m_holdFired   = false;
    float m_heldTime    = 0.0f;
    float m_holdPulse   = 0.0f;
    float m_tapPulse    = 0.0f;

    void updateHold(bool down, float dt);

  };

}
