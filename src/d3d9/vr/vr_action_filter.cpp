#include <algorithm>

#include "vr_action_filter.h"

namespace dxvk {

  VrActionState VrActionFilter::update(
    const VrActionState&        actions,
          VrInputContext        context,
          float                 dt) {
    if (context != m_context) {
      m_context    = context;
      m_blocked    = actions.pressed;
      m_holdDown   = false;
      m_holdPulse  = 0.0f;
      m_tapPulse   = 0.0f;
      m_reloadDown = false;
      m_grabDown = false;
      m_lightTime  = 0.0f;
    }

    m_blocked &= actions.pressed;

    VrActionState result = actions;
    result.pressed &= ~m_blocked;

    if (context != VrInputContext::Game)
      return result;

    updateHold(result.isPressed(VrAction::PauseHold), dt);
    updateReload(result);
    updateLight(result, dt);

    result.setPressed(VrAction::PauseHold, false);
    result.setPressed(VrAction::Pause, m_holdPulse > 0.0f || result.isPressed(VrAction::Pause));
    result.setPressed(VrAction::Wait,  m_tapPulse  > 0.0f || result.isPressed(VrAction::Wait));

    m_holdPulse -= dt;
    m_tapPulse  -= dt;
    return result;
  }


  void VrActionFilter::updateHold(bool down, float dt) {
    if (down) {
      if (m_holdDown) {
        m_heldTime += dt;
      } else {
        m_heldTime  = 0.0f;
        m_holdFired = false;
      }

      if (!m_holdFired && m_heldTime >= HoldTime) {
        m_holdFired = true;
        m_holdPulse = PulseTime;
      }
    } else if (m_holdDown && !m_holdFired) {
      m_tapPulse = PulseTime;
    }

    m_holdDown = down;
  }


  void VrActionFilter::updateReload(VrActionState& actions) {
    bool down = actions.isPressed(VrAction::Reload);

    if (down && !m_reloadDown) {
      m_pressTaken     = m_reloadTaken;
      m_pressActivates = m_reloadActivates && !m_pressTaken;
    }

    m_reloadDown = down;

    if (down && m_pressTaken)
      actions.setPressed(VrAction::Reload, false);

    if (down && m_pressActivates) {
      actions.setPressed(VrAction::Reload,   false);
      actions.setPressed(VrAction::Activate, true);
    }
  }


  void VrActionFilter::updateLight(VrActionState& actions, float dt) {
    m_lightTime = std::max(m_lightTime - dt, 0.0f);

    bool down = actions.isPressed(VrAction::Grab);

    if (down && !m_grabDown) {
      // A press while the light is still being switched switches nothing
      m_pressLights = m_lightHoldTime > 0.0f || m_lightTime > 0.0f;

      if (m_lightHoldTime > 0.0f && m_lightTime == 0.0f)
        m_lightTime = m_lightHoldTime + LightMargin;

      m_pressHolsters = m_grabAtHolster && !m_pressLights;
    }

    m_grabDown = down;

    if (down && (m_pressLights || m_pressHolsters))
      actions.setPressed(VrAction::Grab, false);
  }

}
