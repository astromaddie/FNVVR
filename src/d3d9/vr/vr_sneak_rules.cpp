#include <algorithm>

#include "vr_sneak_rules.h"

namespace dxvk {

  bool VrPhysicalCrouch::update(float headHeight, float dt) {
    if (!m_hasStanding) {
      m_standing    = headHeight;
      m_hasStanding = true;
    } else if (headHeight > m_standing) {
      m_standing = std::min(headHeight, m_standing + StandingRiseSpeed * dt);
    }

    float drop = m_standing - headHeight;

    m_crouched = m_crouched
      ? drop > m_config.depth - m_config.hysteresis
      : drop >= m_config.depth;
    return m_crouched;
  }


  void VrPhysicalCrouch::reset() {
    m_hasStanding = false;
    m_crouched    = false;
  }


  bool VrSneakToggle::update(bool wanted, bool sneaking, bool allowed, float dt) {
    if (!allowed)
      m_pressTime = 0.0f;

    if (m_pressTime > 0.0f) {
      m_pressTime -= dt;

      if (m_pressTime <= 0.0f)
        m_waitTime = SettleTime;

      return true;
    }

    if (m_waitTime > 0.0f) {
      m_waitTime -= dt;
      return false;
    }

    if (!allowed || wanted == sneaking)
      return false;

    m_pressTime = PressTime - dt;

    if (m_pressTime <= 0.0f)
      m_waitTime = SettleTime;

    return true;
  }


  void VrSneakToggle::reset() {
    m_pressTime = 0.0f;
    m_waitTime  = 0.0f;
  }


  float VrSneakCamera::update(const float* height, bool sneaking, float dt) {
    if (!height)
      return m_raise;

    float drop = m_hasStanding ? m_standing - *height : 0.0f;

    // The camera comes back up over a few frames once the game stops
    // sneaking, and is still raised meanwhile
    if (sneaking) {
      m_rising   = true;
      m_riseTime = 0.0f;
    } else if (m_rising) {
      m_riseTime += dt;
      m_rising = drop >= BackUpRaise && m_riseTime < MaxRiseTime;
    }

    if (m_rising) {
      m_raise = std::clamp(drop, 0.0f, MaxRaise);
      return m_raise;
    }

    if (!m_hasStanding) {
      m_standing    = *height;
      m_hasStanding = true;
    } else {
      m_standing += (*height - m_standing) * std::min(1.0f, dt / StandingSmoothing);
    }

    m_raise = 0.0f;
    return m_raise;
  }


  void VrSneakCamera::reset() {
    m_hasStanding = false;
    m_raise       = 0.0f;
    m_rising      = false;
    m_riseTime    = 0.0f;
  }

}
