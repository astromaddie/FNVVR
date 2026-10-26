#include <algorithm>
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


  float vrGrabSpringLagSteps(const VrGrabSpringConstants& spring) {
    if (!(spring.elasticity > 0.0f))
      return 0.0f;

    return (1.0f - spring.objectDamping * (1.0f - spring.damping)) / spring.elasticity;
  }


  float vrGrabMaxAcceleration(
          float                 mass,
          uint32_t              hands,
          float                 fullAcceleration,
          float                 gravity) {
    // Enough to lift the body and still move it a little
    constexpr float GravityMargin = 1.5f;

    float minimum = std::min(GravityMargin * gravity, fullAcceleration);

    if (!(mass > 0.0f))
      return fullAcceleration;

    float acceleration = fullAcceleration * float(hands) * VrGrabLightMass / mass;
    return std::max(std::min(acceleration, fullAcceleration), minimum);
  }


  VrVector3 vrGrabLead(const VrVector3& playerStep, float frames) {
    // Faster than any run: the player was moved, not walking
    constexpr float MaxStep = 1.0f * VrGameUnitsPerMetre;
    constexpr float MaxLead = 0.6f * VrGameUnitsPerMetre;

    float step = vrLength(playerStep);

    if (step > MaxStep || !(frames > 0.0f))
      return VrVector3();

    VrVector3 lead = playerStep * frames;
    float length = step * frames;

    if (length > MaxLead)
      lead = lead * (MaxLead / length);

    return lead;
  }


  float vrGrabPullSpeed(float mass) {
    constexpr float MinScale = 0.35f;

    if (!(mass > VrGrabLightMass))
      return VrGrabHand::PullSpeed;

    return VrGrabHand::PullSpeed * std::max(std::sqrt(VrGrabLightMass / mass), MinScale);
  }


  VrGrabRequest VrGrabHand::update(const VrGrabHandInput& input) {
    bool pressed = input.grip && !m_gripDown;
    m_gripDown = input.grip;

    if (m_holding) {
      if (!input.hand || !input.grip || !input.free)
        return VrGrabRequest::Release;

      if (m_pulled)
        advancePull(input.dt);

      return VrGrabRequest::None;
    }

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
    m_pulled  = false;
    m_window  = 0.0f;
    m_offset  = vrPointToHand(hand, point);
  }


  void VrGrabHand::pull(const VrGameCameraPose& hand, const VrVector3& point, float speed) {
    hold(hand, point);
    m_pulled    = true;
    m_pullSpeed = speed;
  }


  void VrGrabHand::drop() {
    m_holding = false;
    m_pulled  = false;
    m_window  = 0.0f;
  }


  void VrGrabHand::advancePull(float dt) {
    float distance = vrLength(m_offset);
    float step     = m_pullSpeed * dt;

    if (distance <= step) {
      m_offset = VrVector3();
      m_pulled = false;
      return;
    }

    m_offset = m_offset * ((distance - step) / distance);
  }


  VrVector3 VrGrabHand::target(const VrGameCameraPose& hand) const {
    return vrPointFromHand(hand, m_offset);
  }

}
