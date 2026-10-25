#pragma once

#include "vr_types.h"

namespace dxvk {

  /**
   * \brief Turns the raw actions into the actions the game acts on
   *
   * Three rules apply:
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
   *
   * - While the interaction prompt is at the left hand, the Reload action
   *   (X, a left hand button) activates instead, so that the hand the
   *   prompt is at activates. What a press does is decided when it starts
   *   and kept until it is released, so a press never turns into the other
   *   control when the prompt moves to the other hand.
   *
   * - While the right hand is at the Pip-Boy on the left forearm, a
   *   Grab press (the right grip) switches the Pip-Boy light instead of
   *   grabbing: the game's Pip-Boy control is held for the time the
   *   game needs to switch the light (see VrPipBoyControl::lightHoldTime)
   *   plus \ref LightMargin, whether the grip stays down or not. As for
   *   Reload, the press is decided when it starts, and Grab stays off
   *   until the grip is released.
   *
   * - While the left hand holds ammunition taken behind the back, the
   *   Reload action (X) changes its type (see VrManualReload::cycleAmmo)
   *   and reaches the game neither as Reload nor as Activate. As above,
   *   the press is decided when it starts.
   *
   * - While the right hand is at the holster of the equipped weapon, a
   *   Grab press holsters or draws it (see VrHolsterHand) and never
   *   reaches the game, until the grip is released.
   */
  class VrActionFilter {

  public:

    /// How long the PauseHold action is held to open the pause menu, in seconds
    constexpr static float HoldTime  = 0.6f;
    /// How long a hold or a tap is reported, in seconds, at least one frame
    constexpr static float PulseTime = 0.1f;
    /// How much longer than the game's light timer the Pip-Boy control is held, in seconds
    constexpr static float LightMargin = 0.25f;

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

    /**
     * \brief Makes the Reload action activate from the next press on
     *
     * \param [in] activates Whether the interaction prompt is at the left hand
     */
    void setReloadActivates(bool activates) {
      m_reloadActivates = activates;
    }

    /**
     * \brief Keeps the Reload action from the game from the next press on
     *
     * \param [in] taken Whether the left hand holds ammunition whose type
     *    the press changes
     */
    void setReloadTaken(bool taken) {
      m_reloadTaken = taken;
    }

    /**
     * \brief Makes the Grab action switch the Pip-Boy light from the next press on
     *
     * \param [in] holdTime How long the game's Pip-Boy control is held to
     *    switch the light, in seconds, or 0 while the right hand is not at
     *    the Pip-Boy (or the light cannot be switched)
     */
    void setGrabLightHoldTime(float holdTime) {
      m_lightHoldTime = holdTime;
    }

    /**
     * \brief Makes the Grab action holster or draw from the next press on
     *
     * \param [in] atHolster Whether the right hand is at the holster
     */
    void setGrabAtHolster(bool atHolster) {
      m_grabAtHolster = atHolster;
    }

    /**
     * \brief Whether the game's Pip-Boy control is held to switch the light
     *
     * Set by the last call to \ref update, in the Game context only.
     */
    bool holdsPipBoyControl() const {
      return m_lightTime > 0.0f;
    }

  private:

    VrInputContext m_context = VrInputContext::Game;
    uint32_t       m_blocked = 0;

    bool  m_holdDown    = false;
    bool  m_holdFired   = false;
    float m_heldTime    = 0.0f;
    float m_holdPulse   = 0.0f;
    float m_tapPulse    = 0.0f;

    bool  m_reloadActivates = false;
    bool  m_reloadDown      = false;
    bool  m_pressActivates  = false;
    bool  m_reloadTaken     = false;
    bool  m_pressTaken      = false;

    float m_lightHoldTime   = 0.0f;
    float m_lightTime       = 0.0f;
    bool  m_grabDown      = false;
    bool  m_pressLights     = false;
    bool  m_grabAtHolster   = false;
    bool  m_pressHolsters   = false;

    void updateHold(bool down, float dt);

    void updateReload(VrActionState& actions);

    void updateLight(VrActionState& actions, float dt);

  };

}
