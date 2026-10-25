#include <cmath>

#include "vr_grab.h"
#include "vr_math.h"
#include "vr_melee.h"

namespace dxvk {

  std::array<VrGrabRay, VrGrabProbeRayCount> vrGrabProbeRays(
    const VrVector3&            centre,
          float                 radius) {
    constexpr float Corner = 0.57735027f;

    constexpr VrVector3 Directions[VrGrabProbeRayCount] = {
      {  1.0f,  0.0f,  0.0f }, { -1.0f,  0.0f,  0.0f },
      {  0.0f,  1.0f,  0.0f }, {  0.0f, -1.0f,  0.0f },
      {  0.0f,  0.0f,  1.0f }, {  0.0f,  0.0f, -1.0f },
      {  Corner,  Corner,  Corner }, { -Corner,  Corner,  Corner },
      {  Corner, -Corner,  Corner }, { -Corner, -Corner,  Corner },
      {  Corner,  Corner, -Corner }, { -Corner,  Corner, -Corner },
      {  Corner, -Corner, -Corner }, { -Corner, -Corner, -Corner },
    };

    std::array<VrGrabRay, VrGrabProbeRayCount> rays;

    for (uint32_t i = 0; i < VrGrabProbeRayCount; i++)
      rays[i] = { centre + Directions[i] * radius, centre };

    return rays;
  }


  bool vrGrabClearOfPlayer(
    const VrVector3&            body,
    const VrVector3&            player,
          float                 distance) {
    float dx = body.x - player.x;
    float dy = body.y - player.y;
    return dx * dx + dy * dy >= distance * distance;
  }


  VrGrabRequest VrGrabHand::update(const VrGrabHandInput& input) {
    bool pressed = input.grip && !m_gripDown;
    m_gripDown = input.grip;

    if (m_holding)
      return input.hand && input.grip && input.free ? VrGrabRequest::None : VrGrabRequest::Release;

    if (!input.hand || !input.grip || !input.free) {
      m_window = 0.0f;
      return VrGrabRequest::None;
    }

    if (pressed)
      m_window = GrabWindow;

    if (m_window <= 0.0f)
      return VrGrabRequest::None;

    m_window -= input.dt;
    return VrGrabRequest::Grab;
  }


  void VrGrabHand::hold(const VrGameCameraPose& hand, const VrVector3& point) {
    m_holding = true;
    m_window  = 0.0f;
    m_offset  = vrPointToHand(hand, point);
  }


  void VrGrabHand::drop() {
    m_holding = false;
    m_window  = 0.0f;
  }


  VrVector3 VrGrabHand::target(const VrGameCameraPose& hand) const {
    return vrPointFromHand(hand, m_offset);
  }

}
