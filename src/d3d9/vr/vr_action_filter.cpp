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
    }

    m_blocked &= actions.pressed;

    VrActionState result = actions;
    result.pressed &= ~m_blocked;

    if (context != VrInputContext::Game)
      return result;

    updateHold(result.isPressed(VrAction::PauseHold), dt);

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

}
