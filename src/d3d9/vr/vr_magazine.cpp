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

    const char* const RackWords[] = { "slide", "bolt", "charginghandle", "lever", "bar" };

    // Barrels, and parts next to the rack part
    const char* const NotRackWords[] = { "barrel", "lock", "pin", "release", "trigger" };

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

    if (containsAny(part, RackWords) && !containsAny(part, NotRackWords))
      return VrWeaponPartKind::Rack;

    return VrWeaponPartKind::None;
  }


  float vrDistanceToSegment(
    const VrVector3&            point,
    const VrVector3&            start,
    const VrVector3&            axis,
          float                 length) {
    float along = std::clamp(vrDot(point - start, axis), 0.0f, length);
    return vrLength(point - (start + axis * along));
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

    // The mouth of the well, where a magazine enters it, and the way into
    // it from there
    VrVector3 mouth = input.well + input.axis * (m_reach.depth * M);
    float toWay     = vrDistanceToSegment(input.held, mouth, input.axis, m_reach.approach * M) / M;

    switch (m_state) {
      case VrMagazineState::InWeapon:
        if (input.active && pressed && !input.blocked && ejectDistance(input) <= m_reach.eject) {
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

        if (toWay > m_reach.capture + m_reach.rearm)
          m_canEnter = true;

        // A magazine coming from beyond the mouth starts at the mouth
        if (m_canEnter && toWay <= m_reach.capture) {
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
          m_canEnter = toWay > m_reach.capture + m_reach.rearm;
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


  float VrMagazineHand::ejectDistance(const VrMagazineInput& input) const {
    constexpr float M = VrGameUnitsPerMetre;
    return vrDistanceToSegment(input.hand, input.well, input.axis, m_reach.below * M) / M;
  }


  VrRackEvent VrRackHand::update(const VrRackInput& input) {
    bool pressed = input.grip && !m_gripDown;
    m_gripDown = input.grip;

    VrVector3 offset = (input.hand - input.rack) * (1.0f / VrGameUnitsPerMetre);
    float along      = vrDot(offset, input.axis);
    VrVector3 across = offset - input.axis * along;

    if (!m_held) {
      if (!input.active || !pressed || input.blocked || grabDistance(input) > m_reach.grab)
        return VrRackEvent::None;

      // A part locked back is taken where it is, all the way back
      m_held        = true;
      m_pullStart   = input.lockedBack ? input.travel : 0.0f;
      m_full        = input.lockedBack;
      m_pull        = m_pullStart;
      m_alongStart  = along;
      m_acrossStart = across;
      return VrRackEvent::Grabbed;
    }

    if (!input.grip || !input.active || vrLength(across - m_acrossStart) > m_reach.leave) {
      bool racked = m_full;
      reset();
      return racked ? VrRackEvent::Racked : VrRackEvent::Released;
    }

    m_pull = std::clamp(m_pullStart + along - m_alongStart, 0.0f, input.travel);

    if (m_pull >= m_reach.full * input.travel)
      m_full = true;

    return VrRackEvent::None;
  }


  void VrRackHand::reset() {
    m_held = false;
    m_full = false;
    m_pull = 0.0f;
  }


  float VrRackHand::grabDistance(const VrRackInput& input) const {
    constexpr float M = VrGameUnitsPerMetre;
    return vrDistanceToSegment(input.hand, input.rack, input.axis, input.travel * M) / M;
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


  size_t vrFindMagazineHold(
    const VrGameTransform*      inHand,
          size_t                count) {
    // Moving with the hand, between two samples: metres and radians (2 degrees)
    constexpr float MaxStep = 0.003f;
    constexpr float MaxTurn = 0.035f;

    auto carried = [inHand] (size_t a, size_t b) {
      return vrLength(inHand[b].translate - inHand[a].translate) <= MaxStep
          && vrRotationAngle(inHand[a].rotate, inHand[b].rotate) <= MaxTurn;
    };

    size_t best = count;

    for (size_t i = 1; i + 1 < count; i++) {
      if (!carried(i - 1, i) || !carried(i, i + 1))
        continue;

      if (best == count || vrLength(inHand[i].translate) < vrLength(inHand[best].translate))
        best = i;
    }

    return best;
  }


  bool vrFindRackMotion(
    const VrMagazineSample*     samples,
          size_t                count,
          VrRackMotion&         motion) {
    // Pulled back by more than this, the part racks the weapon
    constexpr float Moved = 0.01f;
    // Within this of its place, the part is home
    constexpr float Home  = 0.003f;

    size_t back = count;
    float  far  = Moved;

    for (size_t i = 0; i < count; i++) {
      float distance = vrLength(samples[i].offset);

      if (distance > far) {
        back = i;
        far  = distance;
      }
    }

    if (back == count)
      return false;

    size_t start = back;

    while (start > 0 && vrLength(samples[start - 1].offset) > Home)
      start -= 1;

    size_t home = back;

    while (home + 1 < count && vrLength(samples[home].offset) > Home)
      home += 1;

    motion.start  = start;
    motion.back   = back;
    motion.home   = home;
    motion.axis   = samples[back].offset * (1.0f / far);
    motion.travel = far;
    return true;
  }


  int32_t vrFindNearestSound(
    const float*                times,
          size_t                count,
          float                 time,
          float                 window,
          int32_t               exclude) {
    return vrFindSoundDuring(times, count, time, time, window, exclude);
  }


  int32_t vrFindSoundDuring(
    const float*                times,
          size_t                count,
          float                 from,
          float                 to,
          float                 window,
          int32_t               exclude) {
    int32_t nearest = -1;
    float   best    = window;

    for (size_t i = 0; i < count; i++) {
      float distance = std::max({ from - times[i], times[i] - to, 0.0f });

      if (int32_t(i) != exclude && distance <= best) {
        nearest = int32_t(i);
        best    = distance;
      }
    }

    return nearest;
  }

}
