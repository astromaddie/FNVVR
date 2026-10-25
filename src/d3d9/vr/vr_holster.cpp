#include "vr_holster.h"
#include "vr_math.h"

namespace dxvk {

  namespace {

    // Weapon animation types, as VrWeaponHand reads them
    constexpr int32_t TypeHandToHand   = 0;
    constexpr int32_t TypeOneHandMelee = 1;
    constexpr int32_t TypePistol       = 3;
    constexpr int32_t TypePistolEnergy = 4;
    constexpr int32_t TypeGrenade      = 10;
    constexpr int32_t TypeThrown       = 13;

  }


  VrHolsterKind vrHolsterForWeaponType(int32_t type) {
    if (type < 0 || type == TypeHandToHand || type > TypeThrown)
      return VrHolsterKind::None;

    if (type == TypeOneHandMelee || type == TypePistol || type == TypePistolEnergy || type >= TypeGrenade)
      return VrHolsterKind::Hip;

    return VrHolsterKind::Back;
  }


  bool vrIsAtHolster(
    const VrGameCameraPose&     head,
    const VrVector3&            hand,
          VrHolsterKind         kind,
    const VrHolsterZones&       zones) {
    if (kind == VrHolsterKind::None)
      return false;

    // The heading from the head's forward and up axes, defined when the
    // player looks straight up or down, as for the magazine pouch
    VrVector3 forward = {
      head.forward.x - head.forward.z * head.up.x,
      head.forward.y - head.forward.z * head.up.y,
      0.0f };

    float length = vrLength(forward);

    if (length < 1e-4f)
      return false;

    forward = forward * (1.0f / length);

    // The game's world is right-handed with Z up
    VrVector3 right  = { forward.y, -forward.x, 0.0f };
    VrVector3 offset = (hand - head.position) * (1.0f / VrGameUnitsPerMetre);

    float ahead = vrDot(offset, forward);
    float side  = vrDot(offset, right);

    if (kind == VrHolsterKind::Hip) {
      return side      >= zones.hipRightMin && side      <= zones.hipRightMax
          && -offset.z >= zones.hipBelowMin && -offset.z <= zones.hipBelowMax
          && ahead     <= zones.hipAhead    && -ahead    <= zones.hipBehind;
    }

    return -ahead    >= zones.backBehind
        && -side     <= zones.backLeft
        && -offset.z <= zones.backBelow
        && vrLength(offset) <= zones.backReach;
  }


  VrHolsterEvent VrHolsterHand::update(const VrHolsterInput& input) {
    bool pressed = input.grip && !m_gripDown;
    m_gripDown = input.grip;

    bool wasAtHolster = m_atHolster;
    m_atHolster = input.active && vrIsAtHolster(input.head, input.hand, input.kind, m_zones);

    if (!m_atHolster)
      return VrHolsterEvent::None;

    if (pressed)
      return input.drawn ? VrHolsterEvent::Holstered : VrHolsterEvent::Drawn;

    return wasAtHolster ? VrHolsterEvent::None : VrHolsterEvent::Entered;
  }

}
