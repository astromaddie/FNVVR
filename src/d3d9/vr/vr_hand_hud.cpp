#include <algorithm>
#include <cmath>

#include "vr_hand_hud.h"
#include "vr_math.h"
#include "vr_pipboy_gaze.h"

namespace dxvk {

  namespace {

    // Shorter side of the interface, and the longer one of a 4:3 screen,
    // in interface units
    constexpr float InterfaceShortSide = 960.0f;
    constexpr float InterfaceLongSide  = 1280.0f;

    // Margin around the health bar's group, which the compass and the
    // bracket images slightly overflow, in interface units
    constexpr float WristMargin = 12.0f;

    // The prompt's first line (the action, such as "Take") is drawn above
    // its group and the item details below it, in interface units
    constexpr float PromptMarginAbove = 80.0f;
    constexpr float PromptMarginSide  = 40.0f;

    // A quarter turn, in radians
    constexpr float QuarterTurn = 1.5707963f;

    // Steepest slope of the line from the head to the prompt that the
    // prompt still faces, in radians (80 degrees)
    constexpr float MaxPromptPitch = 1.3962634f;

    VrInterfaceRect clampRect(const VrInterfaceRect& rect, const VrVector2& size) {
      return {
        std::clamp(rect.left,   0.0f, size.x),
        std::clamp(rect.top,    0.0f, size.y),
        std::clamp(rect.right,  0.0f, size.x),
        std::clamp(rect.bottom, 0.0f, size.y) };
    }

  }


  VrVector2 vrComputeInterfaceSize(uint32_t width, uint32_t height) {
    if (!width || !height)
      return { InterfaceLongSide, InterfaceShortSide };

    float aspect = float(width) / float(height);

    if (width > height)
      return { InterfaceShortSide * aspect, InterfaceShortSide };

    return { InterfaceLongSide, InterfaceLongSide / aspect };
  }


  VrHandHudCrops vrComputeHandHudCrops(
    const VrInterfaceRect&      hitPoints,
    const VrInterfaceRect&      info,
    const VrVector2&            size) {
    VrHandHudCrops crops;

    crops.wrist = clampRect({
      hitPoints.left   - WristMargin,
      hitPoints.top    - WristMargin,
      hitPoints.right  + WristMargin,
      hitPoints.bottom + WristMargin }, size);

    float centre     = 0.5f * (info.left + info.right);
    float halfWidth  = 0.5f * info.width() + PromptMarginSide;
    float top        = std::max(info.top - PromptMarginAbove, 0.0f);
    float bottom     = size.y;

    // Narrowed on both sides, so that the prompt stays centred
    if (top < crops.wrist.bottom && bottom > crops.wrist.top && crops.wrist.right <= centre)
      halfWidth = std::min(halfWidth, centre - crops.wrist.right);

    if (top < crops.wrist.bottom && bottom > crops.wrist.top && crops.wrist.left >= centre)
      halfWidth = std::min(halfWidth, crops.wrist.left - centre);

    halfWidth = std::min({ halfWidth, centre, size.x - centre });

    crops.prompt = clampRect({ centre - halfWidth, top, centre + halfWidth, bottom }, size);
    return crops;
  }


  VrPose vrComputeWristPanelPose(const VrControllerState& hand) {
    // A panel faces along +Z with its up axis on +Y. Turned by -90 degrees
    // around X, its up axis runs forward along the hand (-Z) and it faces
    // up (+Y). Turned around the hand's forward axis, it faces the hand's
    // right axis (+X) raised by the face-up angle.
    VrQuaternion alongHand = vrQuaternionFromAxisAngle({ 1.0f, 0.0f, 0.0f }, -QuarterTurn);
    VrQuaternion outwards  = vrQuaternionFromAxisAngle({ 0.0f, 0.0f, 1.0f },
      VrHandHudLayout::WristFaceUpAngle - QuarterTurn);

    VrPose pose;
    pose.position    = vrComputeControllerPoint(hand, VrHandHudLayout::WristOffset);
    pose.orientation = vrNormalize(hand.aimPose.orientation * outwards * alongHand);
    return pose;
  }


  VrPose vrComputePromptPanelPose(const VrControllerState& hand, const VrPose& head) {
    VrVector3 centre = vrComputeControllerPoint(hand, VrHandHudLayout::FingertipOffset)
                     + VrVector3 { 0.0f, VrHandHudLayout::PromptLift, 0.0f };

    // Faces the head without rolling, as a panel placed on the line from
    // the head to the prompt does
    VrPose pose = vrComputePanelPoseTowards(head, centre,
      vrLength(centre - head.position), MaxPromptPitch);
    pose.position = centre;
    return pose;
  }


  bool VrWristGaze::update(const VrPose* head, const VrPose* panel, float dt) {
    bool looking = false;

    if (head && panel && vrIsLookingAt(*head, panel->position, MaxAngle, MaxDistance)) {
      VrVector3 normal = vrRotate(panel->orientation, { 0.0f, 0.0f, 1.0f });
      VrVector3 toHead = head->position - panel->position;
      looking = vrDot(normal, toHead) >= std::cos(MaxFacingAngle) * vrLength(toHead);
    }

    if (looking) {
      m_lookTime += dt;
      m_awayTime  = 0.0f;
    } else {
      m_lookTime  = 0.0f;
      m_awayTime += dt;
    }

    if (!m_visible && looking && m_lookTime >= ShowDelay)
      m_visible = true;

    if (m_visible && !looking && m_awayTime >= HideDelay)
      m_visible = false;

    return m_visible;
  }


  void VrWristGaze::reset() {
    m_visible  = false;
    m_lookTime = 0.0f;
    m_awayTime = 0.0f;
  }


  bool VrPromptHand::update(
    const std::array<VrControllerState, VrHandCount>& controllers,
    const VrVector3&            target,
          VrHand&               hand) {
    const VrControllerState& current = controllers[uint32_t(m_hand)];
    VrHand other = m_hand == VrHand::Right ? VrHand::Left : VrHand::Right;
    const VrControllerState& alternative = controllers[uint32_t(other)];

    if (!current.isActive && !alternative.isActive)
      return false;

    if (!current.isActive) {
      m_hand = other;
    } else if (alternative.isActive) {
      float currentDistance     = vrLength(vrComputeControllerPoint(current,     VrHandHudLayout::FingertipOffset) - target);
      float alternativeDistance = vrLength(vrComputeControllerPoint(alternative, VrHandHudLayout::FingertipOffset) - target);

      if (alternativeDistance + SwitchMargin < currentDistance)
        m_hand = other;
    }

    hand = m_hand;
    return true;
  }

}
