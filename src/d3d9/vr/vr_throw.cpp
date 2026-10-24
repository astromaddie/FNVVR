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

    /**
     * \brief Words of a grenade model's file name and how it is armed
     *
     * Used when the model shows no part to act on. The first match wins.
     * Grenades matching none are thrown as they are.
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

    /**
     * \brief Words of a part's name and what the part is
     *
     * Matched against the name in lower case without its leading '#'
     * marks and geometry index, as the whole name or anywhere in it; the
     * first match wins: the lighter's switch is part of the lighter, not a
     * button.
     */
    struct PartName {
      const char* word;
      bool        contains;
      VrThrowPart part;
    };

    constexpr PartName PartNames[] = {
      { "lighterflame", true,  VrThrowPart::Flame    },
      { "lighter",      false, VrThrowPart::Lighter  },
      { "lighter",      true,  VrThrowPart::None     },
      { "lighton",      true,  VrThrowPart::LightOn  },
      { "lightoff",     true,  VrThrowPart::LightOff },
      { "button",       true,  VrThrowPart::Button   },
      { "switch",       true,  VrThrowPart::Button   },
      { "dial",         true,  VrThrowPart::Button   },
      { "plunger",      true,  VrThrowPart::Button   },
    };

    std::string lowerCase(std::string text) {
      std::transform(text.begin(), text.end(), text.begin(),
        [] (unsigned char c) { return char(std::tolower(c)); });
      return text;
    }

    std::string fileName(const char* path) {
      std::string name(path);
      size_t slash = name.find_last_of("\\/");

      if (slash != std::string::npos)
        name.erase(0, slash + 1);

      return lowerCase(std::move(name));
    }

    /**
     * \brief A part's name without its marks and geometry index
     */
    std::string partName(const char* name) {
      std::string text = lowerCase(name);
      size_t start = text.find_first_not_of('#');
      size_t colon = text.find(':');

      if (start == std::string::npos)
        return std::string();

      if (colon != std::string::npos)
        text.erase(colon);

      return text.substr(std::min(start, text.size()));
    }

    /**
     * \brief Whether a part's name holds the pin
     *
     * "pin" as the end of a name or followed by digits (the Nuka grenade's
     * "NGPin"), but not inside a word such as "spine" or "spinner".
     */
    bool isPinName(const std::string& name) {
      size_t at = name.find("pin");

      while (at != std::string::npos) {
        bool afterS = at > 0 && name[at - 1] == 's';
        size_t end = at + 3;
        bool wordEnd = end == name.size() || !std::isalpha(static_cast<unsigned char>(name[end]));

        if (!afterS && wordEnd)
          return true;

        at = name.find("pin", at + 1);
      }

      return false;
    }

  }


  VrThrowPart vrThrowPartKind(const char* name) {
    if (!name)
      return VrThrowPart::None;

    std::string part = partName(name);

    for (const PartName& entry : PartNames) {
      bool match = entry.contains
        ? part.find(entry.word) != std::string::npos
        : part == entry.word;

      if (match)
        return entry.part;
    }

    return isPinName(part) ? VrThrowPart::Pin : VrThrowPart::None;
  }


  VrThrowArming vrThrowArming(int32_t type, const char* model, const VrThrowParts& parts) {
    bool mine = vrThrowIsPlaced(type);

    if (type != TypeGrenade && !mine)
      return VrThrowArming::None;

    if (parts.pin)
      return VrThrowArming::Pin;

    if (parts.lighter)
      return VrThrowArming::Fuse;

    if (parts.button || mine)
      return VrThrowArming::Button;

    if (!model)
      return VrThrowArming::None;

    std::string name = fileName(model);

    for (const GrenadeModel& entry : GrenadeModels) {
      if (name.find(entry.word) != std::string::npos)
        return entry.arming;
    }

    return VrThrowArming::None;
  }


  bool vrThrowIsPlaced(int32_t type) {
    return type == TypeMine || type == TypeLunchboxMine;
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
    m_pinTaken = 0.0f;
    m_lighting = false;
    m_lit      = 0.0f;
    m_grip     = true;
    m_trigger  = true;
  }


  VrArmEvent VrThrowArm::update(const VrThrowArmInput& input, const VrThrowConfig& config) {
    bool gripPressed    = input.grip && !m_grip;
    bool triggerPressed = input.trigger && !m_trigger;

    m_grip    = input.grip;
    m_trigger = input.trigger;

    // A pin pulled out stays in the hand until the grip lets it go
    if (m_holdsPin && m_armed && !input.grip)
      m_holdsPin = false;

    if (m_armed)
      return VrArmEvent::None;

    float handDistance = vrLength(input.hand - input.target);

    switch (m_arming) {
      case VrThrowArming::Pin:
        if (m_holdsPin && !input.grip) {
          m_holdsPin = false;
          return VrArmEvent::PinDropped;
        }

        if (m_holdsPin && handDistance >= m_pinTaken + config.pinPull) {
          m_armed = true;
          return VrArmEvent::Armed;
        }

        if (!m_holdsPin && gripPressed && handDistance <= input.radius + config.pinReach) {
          m_holdsPin = true;
          m_pinTaken = handDistance;
          return VrArmEvent::PinTaken;
        }

        return VrArmEvent::None;

      case VrThrowArming::Fuse: {
        bool atFuse = vrLength(input.flame - input.target) <= input.radius + config.fuseReach;

        if (!input.trigger || !atFuse) {
          m_lighting = false;
          m_lit      = 0.0f;
          return VrArmEvent::None;
        }

        // The frame the flame reaches the fuse starts the count
        bool started = !m_lighting;
        m_lit      = started ? 0.0f : m_lit + input.dt;
        m_lighting = true;

        if (m_lit >= config.fuseTime) {
          m_lighting = false;
          m_lit      = 0.0f;
          m_armed    = true;
          return VrArmEvent::Armed;
        }

        return started ? VrArmEvent::Lighting : VrArmEvent::None;
      }

      case VrThrowArming::Button: {
        bool touched = vrLength(input.fingertip - input.target) <= input.radius + config.buttonTouch;
        bool clicked = triggerPressed && handDistance <= input.radius + config.buttonReach;

        if (touched || clicked) {
          m_armed = true;
          return VrArmEvent::Armed;
        }

        return VrArmEvent::None;
      }

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


  float vrComputeThrowShare(float handSpeed, const VrThrowConfig& config) {
    if (!(handSpeed >= config.minSpeed))
      return 0.0f;

    return std::min(handSpeed / config.fullThrowHandSpeed, 1.0f);
  }

}
