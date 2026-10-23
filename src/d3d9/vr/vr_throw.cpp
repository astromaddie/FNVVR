#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>

#include "vr_math.h"
#include "vr_throw.h"

namespace dxvk {

  namespace {

    // Weapon types (WeaponAnimationType) of the thrown and placed weapons
    constexpr int32_t TypeGrenade      = 10;
    constexpr int32_t TypeMine         = 11;
    constexpr int32_t TypeLunchboxMine = 12;
    constexpr int32_t TypeThrown       = 13;

    /**
     * \brief Words of a grenade model's file name and how it is armed
     *
     * The first match wins. Grenades matching none are thrown as they are.
     */
    struct GrenadeModel {
      const char*   word;
      VrThrowArming arming;
    };

    constexpr GrenadeModel GrenadeModels[] = {
      { "dynamite", VrThrowArming::Fuse   },
      { "frag",     VrThrowArming::Pin    },
      { "plasma",   VrThrowArming::Button },
      { "pulse",    VrThrowArming::Button },
    };

    std::string fileName(const char* path) {
      std::string name(path);
      size_t slash = name.find_last_of("\\/");

      if (slash != std::string::npos)
        name.erase(0, slash + 1);

      std::transform(name.begin(), name.end(), name.begin(),
        [] (unsigned char c) { return char(std::tolower(c)); });
      return name;
    }

  }


  VrThrowArming vrThrowArming(int32_t type, const char* model) {
    if (type == TypeMine || type == TypeLunchboxMine)
      return VrThrowArming::Button;

    if (type != TypeGrenade || !model)
      return VrThrowArming::None;

    std::string name = fileName(model);

    for (const GrenadeModel& entry : GrenadeModels) {
      if (name.find(entry.word) != std::string::npos)
        return entry.arming;
    }

    return VrThrowArming::None;
  }


  const char* vrThrowArmingName(VrThrowArming arming) {
    switch (arming) {
      case VrThrowArming::None:   return "none";
      case VrThrowArming::Pin:    return "pin";
      case VrThrowArming::Fuse:   return "fuse";
      case VrThrowArming::Button: return "button";
    }

    return "unknown";
  }


  void VrThrowArm::reset(VrThrowArming arming) {
    m_arming   = arming;
    m_armed    = arming == VrThrowArming::None;
    m_holdsPin = false;
    m_lighting = false;
    m_lit      = 0.0f;
    m_grip     = true;
    m_trigger  = true;
  }


  VrArmEvent VrThrowArm::update(
    const VrVector3&            hand,
    const VrVector3&            centre,
          float                 radius,
          bool                  grip,
          bool                  trigger,
          float                 dt,
    const VrThrowConfig&        config) {
    bool gripPressed    = grip && !m_grip;
    bool triggerPressed = trigger && !m_trigger;

    m_grip    = grip;
    m_trigger = trigger;

    if (m_armed)
      return VrArmEvent::None;

    float distance = vrLength(hand - centre);
    float reach    = radius + config.reach;
    bool  near     = distance <= reach;

    switch (m_arming) {
      case VrThrowArming::Pin:
        if (m_holdsPin && !grip) {
          m_holdsPin = false;
          return VrArmEvent::PinDropped;
        }

        if (m_holdsPin && distance > reach + config.pinPull) {
          m_holdsPin = false;
          m_armed    = true;
          return VrArmEvent::Armed;
        }

        if (!m_holdsPin && gripPressed && near) {
          m_holdsPin = true;
          return VrArmEvent::PinTaken;
        }

        return VrArmEvent::None;

      case VrThrowArming::Fuse: {
        if (!trigger || !near) {
          m_lighting = false;
          m_lit      = 0.0f;
          return VrArmEvent::None;
        }

        // The frame the lighter reaches the fuse starts the count
        bool started = !m_lighting;
        m_lit      = started ? 0.0f : m_lit + dt;
        m_lighting = true;

        if (m_lit >= config.fuseTime) {
          m_lighting = false;
          m_lit      = 0.0f;
          m_armed    = true;
          return VrArmEvent::Armed;
        }

        return started ? VrArmEvent::Lighting : VrArmEvent::None;
      }

      case VrThrowArming::Button:
        if (triggerPressed && near) {
          m_armed = true;
          return VrArmEvent::Armed;
        }

        return VrArmEvent::None;

      case VrThrowArming::None:
        break;
    }

    return VrArmEvent::None;
  }


  void VrThrowTracker::reset() {
    m_count = 0;
    m_next  = 0;
  }


  void VrThrowTracker::add(const VrVector3& position, float dt) {
    m_samples[m_next] = { position, dt };
    m_next  = (m_next + 1) % Capacity;
    m_count = std::min(m_count + 1, Capacity);
  }


  VrVector3 VrThrowTracker::velocity(float window) const {
    if (m_count < 2)
      return VrVector3();

    // Newest first: the latest position, then back in time until the
    // window is covered
    const Sample& last = m_samples[(m_next + Capacity - 1) % Capacity];
    float time = 0.0f;
    uint32_t used = 0;

    for (uint32_t i = 1; i < m_count; i++) {
      time += m_samples[(m_next + Capacity - i) % Capacity].dt;
      used  = i;

      if (time >= window)
        break;
    }

    const Sample& first = m_samples[(m_next + Capacity - 1 - used) % Capacity];

    if (time <= 0.0f)
      return VrVector3();

    return (last.position - first.position) * (1.0f / time);
  }


  float vrComputeThrowSpeed(float handSpeed, const VrThrowConfig& config) {
    if (!(handSpeed >= config.minSpeed))
      return 0.0f;

    return handSpeed * config.speedScale;
  }

}
