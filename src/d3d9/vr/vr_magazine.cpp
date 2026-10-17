#include <cctype>
#include <cmath>
#include <cstring>
#include <string>

#include "vr_magazine.h"
#include "vr_math.h"

namespace dxvk {

  namespace {

    struct PartException {
      const char*      name;
      VrWeaponPartKind kind;
    };

    // Parts the keywords would get wrong, compared without case and
    // without the "##" prefix
    const PartException PartExceptions[] = {
      // The sniper rifle's second magazine model, for its other clip size
      { "srcliphigh",  VrWeaponPartKind::None },
      // The Tesla cannon's cell, not its magazine (##TCClip)
      { "tceccell",    VrWeaponPartKind::None },
    };

    const char* const MagazineWords[] = { "clip", "magazine", "drum", "cell", "battery" };

    // Parts next to the magazine, or magazines that do not come out
    const char* const NotMagazineWords[] = { "release", "latch", "spring", "peg", "bay", "assembly", "moon" };

    template<size_t N>
    bool containsAny(const std::string& name, const char* const (&words)[N]) {
      for (const char* word : words) {
        if (name.find(word) != std::string::npos)
          return true;
      }

      return false;
    }

  }


  VrWeaponPartKind vrClassifyWeaponPart(const char* name) {
    if (!name || std::strncmp(name, "##", 2))
      return VrWeaponPartKind::None;

    std::string part;

    for (const char* c = name + 2; *c && *c != ':'; c++)
      part += char(std::tolower(static_cast<unsigned char>(*c)));

    for (const PartException& exception : PartExceptions) {
      if (part == exception.name)
        return exception.kind;
    }

    if (containsAny(part, MagazineWords) && !containsAny(part, NotMagazineWords))
      return VrWeaponPartKind::Magazine;

    return VrWeaponPartKind::None;
  }


  bool vrIsAtMagazinePouch(
    const VrGameCameraPose&     head,
    const VrVector3&            hand,
    const VrMagazinePouch&      pouch) {
    // Level, forward follows the head's forward axis, and its up axis
    // as the head pitches towards straight up or down
    VrVector3 forward = {
      head.forward.x - head.forward.z * head.up.x,
      head.forward.y - head.forward.z * head.up.y,
      0.0f };

    float length = vrLength(forward);

    if (length < 1e-4f)
      return false;

    forward = forward * (1.0f / length);

    // The game's world is right-handed with Z up: facing north (+Y), east
    // (+X) is to the right
    VrVector3 right  = { forward.y, -forward.x, 0.0f };
    VrVector3 offset = (hand - head.position) * (1.0f / VrGameUnitsPerMetre);

    return -vrDot(offset, forward) >= pouch.behind
        &&  vrDot(offset, right)   <= pouch.right
        &&  offset.z               >= -pouch.below
        &&  vrLength(offset)       <= pouch.reach;
  }


  VrMagazineEvent VrMagazineHand::update(const VrMagazineInput& input) {
    bool pressed = input.grip && !m_gripDown;
    m_gripDown = input.grip;

    float distance = vrLength(input.hand - input.well) / VrGameUnitsPerMetre;

    switch (m_state) {
      case VrMagazineState::InWeapon:
        if (input.active && pressed && distance <= m_reach.eject) {
          m_state     = VrMagazineState::InHand;
          m_canInsert = false;
          return VrMagazineEvent::Ejected;
        }
        break;

      case VrMagazineState::InHand:
        if (!input.grip || !input.active) {
          m_state = VrMagazineState::Out;
          return VrMagazineEvent::Dropped;
        }

        if (distance > m_reach.rearm)
          m_canInsert = true;

        if (m_canInsert && distance <= m_reach.insert) {
          m_state = VrMagazineState::InWeapon;
          return VrMagazineEvent::Inserted;
        }
        break;

      case VrMagazineState::Out:
        if (input.active && pressed && vrIsAtMagazinePouch(input.head, input.hand, m_reach.pouch)) {
          if (!input.roundsHeld)
            return VrMagazineEvent::NoRounds;

          m_state     = VrMagazineState::InHand;
          m_canInsert = distance > m_reach.rearm;
          return VrMagazineEvent::Taken;
        }
        break;
    }

    return VrMagazineEvent::None;
  }


  void VrMagazineHand::reset() {
    m_state     = VrMagazineState::InWeapon;
    m_canInsert = false;
  }

}
