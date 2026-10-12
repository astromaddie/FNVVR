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


  VrVector3 vrComputeControllerPoint(
    const VrControllerState&    controller,
    const VrVector3&            offset) {
    // Forward is -Z, up +Y and right +X
    VrVector3 local = { offset.z, offset.y, -offset.x };
    return controller.gripPose.position + vrRotate(controller.aimPose.orientation, local);
  }


  VrPipBoyRequest VrPipBoyGaze::update(const VrPipBoyGazeInput& input) {
    bool open = input.state == VrPipBoyState::Open;

    m_looking = input.head && input.screen
      && vrIsLookingAt(*input.head, *input.screen,
        open ? KeepAngle    : MaxAngle,
        open ? KeepDistance : MaxDistance);

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
