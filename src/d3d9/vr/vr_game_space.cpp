#include <algorithm>
#include <cmath>

#include "vr_game_space.h"

namespace dxvk {

  VrPose vrMakeRecenterReference(const VrPose& head) {
    VrPose reference;
    reference.orientation = vrHeading(head.orientation);
    reference.position    = head.position;
    return reference;
  }


  VrPose vrComputeEyeInReference(const VrPose& reference, const VrPose& eyeInTracking) {
    return vrCompose(vrInverse(reference), eyeInTracking);
  }


  VrGameCameraPose vrComputeEyeCameraPose(
    const VrGameCameraPose& base,
    const VrPose&           eyeInReference,
          float             unitsPerMetre) {
    // OpenXR axes (+X right, +Y up, -Z forward) expressed in the base camera frame
    auto toGame = [&base] (const VrVector3& v) {
      return base.right * v.x + base.up * v.y + base.forward * -v.z;
    };

    VrGameCameraPose result;
    result.right    = toGame(vrRotate(eyeInReference.orientation, { 1.0f, 0.0f, 0.0f }));
    result.up       = toGame(vrRotate(eyeInReference.orientation, { 0.0f, 1.0f, 0.0f }));
    result.forward  = toGame(vrRotate(eyeInReference.orientation, { 0.0f, 0.0f, -1.0f }));
    result.position = base.position + toGame(eyeInReference.position) * unitsPerMetre;
    return result;
  }


  VrVector3 vrComputeGamePointInReference(
    const VrGameCameraPose& base,
    const VrVector3&        point,
          float             unitsPerMetre) {
    // The base camera's axes are the reference's +X, +Y and -Z
    VrVector3 offset = (point - base.position) * (1.0f / unitsPerMetre);
    return { vrDot(offset, base.right), vrDot(offset, base.up), -vrDot(offset, base.forward) };
  }


  VrHeadAngles vrComputeHeadAngles(const VrPose& headInReference) {
    // Forward is -Z, right is +X and up is +Y in the reference frame
    VrVector3 forward = vrRotate(headInReference.orientation, { 0.0f, 0.0f, -1.0f });

    VrHeadAngles angles;
    angles.pitch = -std::asin(std::clamp(forward.y, -1.0f, 1.0f));

    if (std::abs(forward.x) + std::abs(forward.z) > 1e-6f)
      angles.yaw = std::atan2(forward.x, -forward.z);

    return angles;
  }


  VrGameCameraPose vrComputeBodyCameraPose(float heading, const VrVector3& position) {
    // Heading 0 faces north (+Y) and grows clockwise, towards east (+X)
    float s = std::sin(heading);
    float c = std::cos(heading);

    VrGameCameraPose pose;
    pose.forward  = { s, c, 0.0f };
    pose.up       = { 0.0f, 0.0f, 1.0f };
    pose.right    = { c, -s, 0.0f };
    pose.position = position;
    return pose;
  }


  VrVector3 vrTurnAroundPivot(const VrVector3& point, const VrVector3& pivot, float headingChange) {
    float s = std::sin(headingChange);
    float c = std::cos(headingChange);

    float x = point.x - pivot.x;
    float y = point.y - pivot.y;

    // Clockwise seen from above: north (+Y) turns towards east (+X)
    return { pivot.x + x * c + y * s, pivot.y - x * s + y * c, point.z };
  }


  float vrWrapHeading(float angle) {
    constexpr float FullTurn = 6.2831853f;

    float wrapped = std::fmod(angle, FullTurn);

    if (wrapped < 0.0f)
      wrapped += FullTurn;

    // Rounding can bring a small negative angle up to exactly a full turn
    return wrapped < FullTurn ? wrapped : 0.0f;
  }


  VrGameFrustum vrComputeGameFrustum(const VrFov& fov) {
    VrGameFrustum frustum;
    frustum.left   = std::tan(fov.angleLeft);
    frustum.right  = std::tan(fov.angleRight);
    frustum.top    = std::tan(fov.angleUp);
    frustum.bottom = std::tan(fov.angleDown);
    return frustum;
  }


  VrFov vrComputeCullingFov(const VrFov& left, const VrFov& right, float margin) {
    // Left and down are negative angles
    VrFov fov;
    fov.angleLeft  = std::max(std::min(left.angleLeft,  right.angleLeft)  - margin, -VrMaxCullingAngle);
    fov.angleRight = std::min(std::max(left.angleRight, right.angleRight) + margin,  VrMaxCullingAngle);
    fov.angleUp    = std::min(std::max(left.angleUp,    right.angleUp)    + margin,  VrMaxCullingAngle);
    fov.angleDown  = std::max(std::min(left.angleDown,  right.angleDown)  - margin, -VrMaxCullingAngle);
    return fov;
  }


  VrExtent vrComputeGameResolution(const VrExtent& eye, float minAspect) {
    // The tolerance keeps an exact ratio such as 1920 * 4/3 from being
    // rounded up by the float error of minAspect
    uint32_t minWidth = uint32_t(std::ceil(double(eye.height) * double(minAspect) - 0.01));

    VrExtent result;
    result.width  = std::max(eye.width, minWidth);
    result.height = eye.height;
    result.width += result.width & 1;
    return result;
  }

}
