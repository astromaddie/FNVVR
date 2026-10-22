#include <cmath>

#include "vr_math.h"
#include "vr_pipboy_gaze.h"

namespace dxvk {

  bool vrIsLookingAt(
    const VrPose&               head,
    const VrVector3&            point,
          float                 maxAngle,
          float                 maxDistance) {
    VrVector3 direction = point - head.position;
    float distance = vrLength(direction);

    if (distance > maxDistance || distance < 1e-4f)
      return false;

    VrVector3 forward = vrRotate(head.orientation, VrVector3 { 0.0f, 0.0f, -1.0f });
    return vrDot(forward, direction) >= std::cos(maxAngle) * distance;
  }


  bool vrIsFacing(
    const VrPose&               head,
    const VrVector3&            point,
    const VrVector3&            normal,
          float                 maxAngle) {
    VrVector3 toHead = head.position - point;
    return vrDot(normal, toHead) >= std::cos(maxAngle) * vrLength(toHead);
  }


  VrVector3 vrComputeControllerPoint(
    const VrControllerState&    controller,
    const VrVector3&            offset) {
    return controller.gripPose.position + vrComputeControllerDirection(controller, offset);
  }


  VrVector3 vrComputeControllerDirection(
    const VrControllerState&    controller,
    const VrVector3&            direction) {
    // Forward is -Z, up +Y and right +X
    VrVector3 local = { direction.z, direction.y, -direction.x };
    return vrRotate(controller.aimPose.orientation, local);
  }


  bool vrIsHandAtPipBoy(
    const VrControllerState&    hand,
    const VrVector3&            screen) {
    return vrLength(hand.gripPose.position - screen) <= VrPipBoyHandReach;
  }


  VrPipBoyRequest VrPipBoyGaze::update(const VrPipBoyGazeInput& input) {
    m_looking = input.head && input.screen
      && vrIsLookingAt(*input.head, *input.screen, MaxAngle, MaxDistance)
      && (!input.normal || vrIsFacing(*input.head, *input.screen, *input.normal, MaxFacingAngle));

    if (m_looking) {
      m_lookTime += input.dt;
      m_awayTime  = 0.0f;
    } else {
      m_lookTime  = 0.0f;
      m_awayTime += input.dt;
    }

    switch (input.state) {
      case VrPipBoyState::Closed:
        if (!m_looking)
          m_armed = true;

        if (m_armed && m_looking && input.canOpen && m_lookTime >= OpenDelay)
          return VrPipBoyRequest::Open;

        return VrPipBoyRequest::None;

      case VrPipBoyState::Open:
        m_armed = false;

        if (!m_looking && input.canClose && m_awayTime >= CloseDelay)
          return VrPipBoyRequest::Close;

        return VrPipBoyRequest::None;

      case VrPipBoyState::Changing:
        m_armed = false;
        return VrPipBoyRequest::None;
    }

    return VrPipBoyRequest::None;
  }

}
