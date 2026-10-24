#include <algorithm>
#include <cmath>

#include "vr_weapon_aim.h"

namespace dxvk {

  namespace {

    VrVector3 cross(const VrVector3& a, const VrVector3& b) {
      return VrVector3 {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x };
    }

  }


  VrGameRotation operator * (const VrGameRotation& a, const VrGameRotation& b) {
    VrGameRotation result;

    for (uint32_t i = 0; i < 3; i++) {
      for (uint32_t j = 0; j < 3; j++)
        result.m[i][j] = a.m[i][0] * b.m[0][j] + a.m[i][1] * b.m[1][j] + a.m[i][2] * b.m[2][j];
    }

    return result;
  }


  VrVector3 operator * (const VrGameRotation& r, const VrVector3& v) {
    return VrVector3 {
      r.m[0][0] * v.x + r.m[0][1] * v.y + r.m[0][2] * v.z,
      r.m[1][0] * v.x + r.m[1][1] * v.y + r.m[1][2] * v.z,
      r.m[2][0] * v.x + r.m[2][1] * v.y + r.m[2][2] * v.z };
  }


  VrGameRotation vrTranspose(const VrGameRotation& r) {
    VrGameRotation result;

    for (uint32_t i = 0; i < 3; i++) {
      for (uint32_t j = 0; j < 3; j++)
        result.m[i][j] = r.m[j][i];
    }

    return result;
  }


  VrGameRotation vrGameRotationFromQuaternion(const VrQuaternion& q) {
    float x2 = q.x * 2.0f;
    float y2 = q.y * 2.0f;
    float z2 = q.z * 2.0f;

    VrGameRotation r;
    r.m[0][0] = 1.0f - (q.y * y2 + q.z * z2);
    r.m[0][1] = q.x * y2 - q.w * z2;
    r.m[0][2] = q.x * z2 + q.w * y2;
    r.m[1][0] = q.x * y2 + q.w * z2;
    r.m[1][1] = 1.0f - (q.x * x2 + q.z * z2);
    r.m[1][2] = q.y * z2 - q.w * x2;
    r.m[2][0] = q.x * z2 - q.w * y2;
    r.m[2][1] = q.y * z2 + q.w * x2;
    r.m[2][2] = 1.0f - (q.x * x2 + q.y * y2);
    return r;
  }


  VrQuaternion vrQuaternionFromGameRotation(const VrGameRotation& r) {
    const auto& m = r.m;
    float trace = m[0][0] + m[1][1] + m[2][2];

    VrQuaternion q;

    if (trace > 0.0f) {
      float s = std::sqrt(trace + 1.0f) * 2.0f;
      q.w = 0.25f * s;
      q.x = (m[2][1] - m[1][2]) / s;
      q.y = (m[0][2] - m[2][0]) / s;
      q.z = (m[1][0] - m[0][1]) / s;
    } else if (m[0][0] > m[1][1] && m[0][0] > m[2][2]) {
      float s = std::sqrt(1.0f + m[0][0] - m[1][1] - m[2][2]) * 2.0f;
      q.w = (m[2][1] - m[1][2]) / s;
      q.x = 0.25f * s;
      q.y = (m[0][1] + m[1][0]) / s;
      q.z = (m[0][2] + m[2][0]) / s;
    } else if (m[1][1] > m[2][2]) {
      float s = std::sqrt(1.0f + m[1][1] - m[0][0] - m[2][2]) * 2.0f;
      q.w = (m[0][2] - m[2][0]) / s;
      q.x = (m[0][1] + m[1][0]) / s;
      q.y = 0.25f * s;
      q.z = (m[1][2] + m[2][1]) / s;
    } else {
      float s = std::sqrt(1.0f + m[2][2] - m[0][0] - m[1][1]) * 2.0f;
      q.w = (m[1][0] - m[0][1]) / s;
      q.x = (m[0][2] + m[2][0]) / s;
      q.y = (m[1][2] + m[2][1]) / s;
      q.z = 0.25f * s;
    }

    return vrNormalize(q);
  }


  VrGameRotation vrCameraRotation(const VrGameCameraPose& pose) {
    VrGameRotation result;
    const VrVector3* axes[3] = { &pose.forward, &pose.up, &pose.right };

    for (uint32_t j = 0; j < 3; j++) {
      result.m[0][j] = axes[j]->x;
      result.m[1][j] = axes[j]->y;
      result.m[2][j] = axes[j]->z;
    }

    return result;
  }


  VrGameTransform vrComputeRigToHand(
    const VrGameCameraPose& gameCamera,
    const VrVector3&        weaponPosition,
    const VrGameCameraPose& hand) {
    VrGameTransform result;
    result.rotate    = vrCameraRotation(hand) * vrTranspose(vrCameraRotation(gameCamera));
    result.translate = hand.position - result.rotate * weaponPosition;
    return result;
  }


  VrGameTransform vrComputeRigHoldingWeapon(
    const VrGameRotation&   weaponRotation,
    const VrVector3&        weaponPosition,
    const VrGameRotation&   held,
    const VrGameCameraPose& hand) {
    VrGameTransform result;
    result.rotate    = vrCameraRotation(hand) * held * vrTranspose(weaponRotation);
    result.translate = hand.position - result.rotate * weaponPosition;
    return result;
  }


  VrGameRotation vrComputeHeldRotation(
    const VrGameCameraPose& gameCamera,
    const VrGameRotation&   weaponRotation) {
    return vrTranspose(vrCameraRotation(gameCamera)) * weaponRotation;
  }


  float vrRotationAngle(const VrGameRotation& a, const VrGameRotation& b) {
    VrGameRotation r = vrTranspose(a) * b;
    float cosine = 0.5f * (r.m[0][0] + r.m[1][1] + r.m[2][2] - 1.0f);
    return std::acos(std::clamp(cosine, -1.0f, 1.0f));
  }


  void VrHeldRotationSettle::reset() {
    *this = VrHeldRotationSettle();
  }


  bool VrHeldRotationSettle::update(const VrGameRotation& held, float dt) {
    if (m_settled)
      return false;

    m_waited += dt;

    if (!m_hasStart || vrRotationAngle(m_start, held) > Tolerance) {
      m_start      = held;
      m_hasStart   = true;
      m_steadyTime = 0.0f;
    } else {
      m_steadyTime += dt;
    }

    if (m_steadyTime < SettleTime && m_waited < MaxWait)
      return false;

    m_rotation = held;
    m_settled  = true;
    return true;
  }


  void VrDrawSettle::reset() {
    *this = VrDrawSettle();
  }


  bool VrDrawSettle::update(const VrVector3& offset, float dt) {
    if (m_settled)
      return false;

    m_distance = vrLength(offset);
    m_farthest = std::max(m_farthest, m_distance);

    if (m_traceCount < m_trace.size() && m_waited >= float(m_traceCount) * TraceStep)
      m_trace[m_traceCount++] = m_distance;

    m_waited += dt;

    // A hand held still far from the node is not where the weapon's idle
    // puts it: the game has not blended it in yet
    if (!m_hasStart || vrLength(offset - m_start) > Tolerance || m_distance > SettleReach) {
      m_start      = offset;
      m_hasStart   = true;
      m_steadyTime = 0.0f;
    } else {
      m_steadyTime += dt;
    }

    m_timedOut = m_steadyTime < SettleTime && m_waited >= MaxWait;
    m_settled  = m_steadyTime >= SettleTime || m_timedOut;

    if (away())
      m_awayTime += dt;

    return m_settled;
  }


  VrGameCameraPose vrComputeTwoHandedPose(
    const VrGameCameraPose& hand,
    const VrVector3&        support,
    const VrVector3&        target) {
    float supportLength = vrLength(support);
    float targetLength  = vrLength(target);

    if (supportLength <= 0.0f || targetLength <= 0.0f)
      return hand;

    VrVector3 from = support * (1.0f / supportLength);
    VrVector3 to   = target * (1.0f / targetLength);
    VrVector3 axis = cross(from, to);

    float s = vrLength(axis);
    float c = vrDot(from, to);

    if (s < 1e-4f)
      return hand;

    axis = axis * (1.0f / s);

    // Rodrigues' rotation formula
    auto rotate = [&] (const VrVector3& v) {
      return v * c + cross(axis, v) * s + axis * (vrDot(axis, v) * (1.0f - c));
    };

    VrGameCameraPose result = hand;
    result.forward = rotate(hand.forward);
    result.up      = rotate(hand.up);
    result.right   = rotate(hand.right);
    return result;
  }


  VrGameRotation vrComputeRotationBetween(const VrVector3& from, const VrVector3& to) {
    float fromLength = vrLength(from);
    float toLength   = vrLength(to);

    if (fromLength <= 0.0f || toLength <= 0.0f)
      return VrGameRotation();

    VrVector3 a = from * (1.0f / fromLength);
    VrVector3 b = to * (1.0f / toLength);
    VrVector3 axis = cross(a, b);

    float s = vrLength(axis);
    float c = vrDot(a, b);

    if (s < 1e-6f) {
      if (c > 0.0f)
        return VrGameRotation();

      // Opposite: half a turn about any axis perpendicular to them
      axis = cross(a, std::abs(a.x) < 0.9f ? VrVector3 { 1.0f, 0.0f, 0.0f } : VrVector3 { 0.0f, 1.0f, 0.0f });
      s = 0.0f;
      c = -1.0f;
    }

    axis = axis * (1.0f / vrLength(axis));

    // Rodrigues' rotation formula, applied to each axis for the columns
    VrGameRotation result;

    for (uint32_t j = 0; j < 3; j++) {
      VrVector3 e = { j == 0 ? 1.0f : 0.0f, j == 1 ? 1.0f : 0.0f, j == 2 ? 1.0f : 0.0f };
      VrVector3 v = e * c + cross(axis, e) * s + axis * (vrDot(axis, e) * (1.0f - c));

      result.m[0][j] = v.x;
      result.m[1][j] = v.y;
      result.m[2][j] = v.z;
    }

    return result;
  }


  VrGameTransform vrTurnTransform(
    const VrGameTransform&      t,
    const VrVector3&            pivot,
    const VrVector3&            from,
    const VrVector3&            to) {
    VrGameRotation turn = vrComputeRotationBetween(from, to);

    VrGameTransform result;
    result.rotate    = turn * t.rotate;
    result.translate = pivot + turn * (t.translate - pivot);
    return result;
  }


  VrGameTransform vrInverse(const VrGameTransform& t) {
    VrGameTransform result;
    result.rotate    = vrTranspose(t.rotate);
    result.translate = (result.rotate * t.translate) * -1.0f;
    return result;
  }


  VrGameRotation vrMirrorHandRotation(const VrGameRotation& r) {
    VrGameRotation result = r;

    for (uint32_t i = 0; i < 2; i++) {
      result.m[i][2] = -r.m[i][2];
      result.m[2][i] = -r.m[2][i];
    }

    return result;
  }


  VrGameTransform vrMirrorHandPose(const VrGameTransform& pose) {
    VrGameTransform result;
    result.rotate    = vrMirrorHandRotation(pose.rotate);
    result.translate = { pose.translate.x, pose.translate.y, -pose.translate.z };
    return result;
  }


  VrGameTransform vrComputeHeldTransform(
    const VrGameCameraPose&     controller,
    const VrGameTransform&      local) {
    VrGameRotation frame = vrCameraRotation(controller);

    VrGameTransform result;
    result.rotate    = frame * local.rotate;
    result.translate = controller.position + frame * local.translate;
    return result;
  }


  VrVector3 vrComputeForearmPoint(
    const VrGameTransform&      hand,
    const VrGameRotation&       wrist,
    const VrGameRotation&       twist,
    const VrVector3&            wristOffset,
    const VrVector3&            twistOffset,
    const VrVector3&            point) {
    // The forearm is placed so that the hand, at its rotation in the
    // forearm, lands on its pose
    VrGameRotation forearm = hand.rotate * vrTranspose(wrist);
    return hand.translate + forearm * (twistOffset + twist * point - wristOffset);
  }


  VrHeadAngles vrComputeAimAngles(const VrVector3& direction) {
    VrHeadAngles angles;
    float length = vrLength(direction);

    if (length <= 0.0f)
      return angles;

    angles.pitch = -std::asin(std::clamp(direction.z / length, -1.0f, 1.0f));

    if (std::abs(direction.x) + std::abs(direction.y) > 1e-6f * length)
      angles.yaw = vrWrapHeading(std::atan2(direction.x, direction.y));

    return angles;
  }


  VrVector3 vrComputeBarrelDirection(const VrGameRotation& rotation) {
    return { rotation.m[0][1], rotation.m[1][1], rotation.m[2][1] };
  }

}
