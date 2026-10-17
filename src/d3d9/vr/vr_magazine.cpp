#include <algorithm>
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

    constexpr float M = VrGameUnitsPerMetre;

    VrVector3 fromWell = (input.held - input.well) * (1.0f / M);
    float along = vrDot(fromWell, input.axis);

    // The mouth of the well, where a magazine enters it
    VrVector3 mouth = input.well + input.axis * (m_reach.depth * M);
    float toMouth   = vrLength(input.held - mouth) / M;

    switch (m_state) {
      case VrMagazineState::InWeapon:
        if (input.active && pressed && vrLength(input.hand - input.well) / M <= m_reach.eject) {
          seat(input, along);
          m_canInsert = false;
          return VrMagazineEvent::Ejected;
        }
        break;

      case VrMagazineState::Seating: {
        if (!input.grip || !input.active) {
          m_state = VrMagazineState::Out;
          return VrMagazineEvent::Dropped;
        }

        VrVector3 across = fromWell - input.axis * along;
        float slide = along - m_alongStart;
        m_seat = std::max(slide, 0.0f);

        if (m_seat >= m_reach.armed)
          m_canInsert = true;

        if (m_seat > m_reach.depth + m_reach.slack || vrLength(across - m_acrossStart) > m_reach.leave) {
          m_state    = VrMagazineState::InHand;
          m_canEnter = false;
          return VrMagazineEvent::Cleared;
        }

        if (m_canInsert && slide <= 0.0f) {
          m_state = VrMagazineState::InWeapon;
          return VrMagazineEvent::Inserted;
        }
      } break;

      case VrMagazineState::InHand:
        if (!input.grip || !input.active) {
          m_state = VrMagazineState::Out;
          return VrMagazineEvent::Dropped;
        }

        if (toMouth > m_reach.rearm)
          m_canEnter = true;

        // A magazine coming from beyond the mouth starts at the mouth
        if (m_canEnter && toMouth <= m_reach.capture) {
          seat(input, std::max(along - m_reach.depth, 0.0f));
          m_canInsert = true;
          return VrMagazineEvent::Engaged;
        }
        break;

      case VrMagazineState::Out:
        if (input.active && pressed && vrIsAtMagazinePouch(input.head, input.hand, m_reach.pouch)) {
          if (!input.roundsHeld)
            return VrMagazineEvent::NoRounds;

          m_state    = VrMagazineState::InHand;
          m_canEnter = toMouth > m_reach.rearm;
          return VrMagazineEvent::Taken;
        }
        break;
    }

    return VrMagazineEvent::None;
  }


  void VrMagazineHand::seat(const VrMagazineInput& input, float start) {
    VrVector3 fromWell = (input.held - input.well) * (1.0f / VrGameUnitsPerMetre);
    float along = vrDot(fromWell, input.axis);

    m_state       = VrMagazineState::Seating;
    m_alongStart  = start;
    m_acrossStart = fromWell - input.axis * along;
    m_seat        = std::max(along - start, 0.0f);
  }


  void VrMagazineHand::reset() {
    m_state     = VrMagazineState::InWeapon;
    m_canInsert = false;
    m_canEnter  = false;
    m_seat      = 0.0f;
  }


  bool vrFindMagazineTiming(
    const VrMagazineSample*     samples,
          size_t                count,
          VrMagazineTiming&     timing) {
    // Out of its place by more than this, the magazine has moved
    constexpr float Moved = 0.01f;
    // The new magazine is held this far out of its place before it goes in
    constexpr float Held  = 0.04f;

    size_t first = count;
    size_t last  = count;
    float  far   = 0.0f;

    for (size_t i = 0; i < count; i++) {
      float distance = vrLength(samples[i].offset);

      if (distance > Moved) {
        if (first == count)
          first = i;

        last = i;
        far  = std::max(far, distance);
      }
    }

    // The magazine must leave and come back before the animation ends
    if (first == count || last + 1 >= count)
      return false;

    float reach = std::min(Held, far * 0.75f);
    size_t grip = last;

    while (grip > first && vrLength(samples[grip].offset) < reach)
      grip -= 1;

    float length = vrLength(samples[grip].offset);

    timing.out  = samples[first].time;
    timing.in   = samples[last + 1].time;
    timing.grip = samples[grip].time;
    timing.axis = samples[grip].offset * (1.0f / length);
    return true;
  }


  int32_t vrFindNearestSound(
    const float*                times,
          size_t                count,
          float                 time,
          float                 window,
          int32_t               exclude) {
    int32_t nearest = -1;
    float   best    = window;

    for (size_t i = 0; i < count; i++) {
      float distance = std::abs(times[i] - time);

      if (int32_t(i) != exclude && distance <= best) {
        nearest = int32_t(i);
        best    = distance;
      }
    }

    return nearest;
  }

}
