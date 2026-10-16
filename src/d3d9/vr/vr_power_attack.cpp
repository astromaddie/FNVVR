#include <algorithm>
#include <cmath>

#include "vr_math.h"
#include "vr_pipboy_gaze.h"
#include "vr_power_attack.h"

namespace dxvk {

  namespace {

    // Ring radii as fractions of the image size
    constexpr float RingOuter = 0.46f;
    constexpr float RingInner = 0.33f;

    // Amber of the Pip-Boy's screen, as the lasers use
    constexpr uint32_t RingColor = 0xFFB642;

    // Opacity of the track, of the filled part, and of the charged ring
    constexpr float TrackAlpha   = 0.30f;
    constexpr float FillAlpha    = 0.80f;
    constexpr float ChargedAlpha = 1.00f;

    constexpr float Pi = 3.14159265f;

    // Steepest slope of the line from the head to the ring that the ring
    // still faces, in radians (80 degrees)
    constexpr float MaxRingPitch = 1.3962634f;

  }


  int32_t vrComputePowerAttackCost(const VrPowerAttackCost& cost, float weight) {
    // Truncated as the integer sum of the original is
    int32_t total = int32_t(float(cost.base) + std::max(weight, 0.0f) * cost.weightMult);
    return std::min(total, cost.max);
  }


  void VrPowerCharge::reset() {
    m_held    = false;
    m_allowed = false;
    m_time    = 0.0f;
  }


  void VrPowerCharge::update(bool held, bool allowed, float dt, float holdTime) {
    m_holdTime = holdTime;

    if (!held) {
      reset();
      return;
    }

    // The press starts the timer, the frames it stays held add to it; once
    // allowed again, it starts over as from a press
    if (m_held && m_allowed && allowed)
      m_time += std::max(dt, 0.0f);
    else
      m_time = 0.0f;

    m_held    = true;
    m_allowed = allowed;
  }


  void VrPowerCharge::spend() {
    m_time = 0.0f;
  }


  float VrPowerCharge::fill() const {
    if (!charging())
      return 0.0f;

    if (charged())
      return 1.0f;

    return m_holdTime > 0.0f ? std::min(m_time / m_holdTime, 1.0f) : 0.0f;
  }


  void vrDrawChargeRing(uint32_t* pixels, uint32_t size, float fill, bool charged) {
    float centre = 0.5f * float(size);
    float outer  = RingOuter * float(size);
    float inner  = RingInner * float(size);
    float middle = 0.5f * (outer + inner);
    float filled = (charged ? 1.0f : std::clamp(fill, 0.0f, 1.0f)) * 2.0f * Pi;

    for (uint32_t y = 0; y < size; y++) {
      for (uint32_t x = 0; x < size; x++) {
        float dx = float(x) + 0.5f - centre;
        float dy = float(y) + 0.5f - centre;
        float r  = std::sqrt(dx * dx + dy * dy);

        // Coverage of the pixel by the ring, smoothed over one pixel
        float ring = std::clamp(std::min(outer - r, r - inner) + 0.5f, 0.0f, 1.0f);
        float alpha = 0.0f;

        if (ring > 0.0f) {
          // Clockwise from the top, as seen in the image
          float angle = std::atan2(dx, -dy);

          if (angle < 0.0f)
            angle += 2.0f * Pi;

          // Coverage by the filled arc, smoothed over one pixel along the ring
          float arc = std::clamp((filled - angle) * middle + 0.5f, 0.0f, 1.0f);

          if (filled <= 0.0f)
            arc = 0.0f;

          float lit = charged ? ChargedAlpha : FillAlpha;
          alpha = ring * (TrackAlpha + (lit - TrackAlpha) * arc);
        }

        uint32_t a = uint32_t(std::lround(alpha * 255.0f));
        pixels[y * size + x] = (a << 24) | RingColor;
      }
    }
  }


  VrPose vrComputeChargeRingPose(const VrControllerState& hand, const VrPose& head) {
    VrVector3 centre = vrComputeControllerPoint(hand, VrChargeRingLayout::HandOffset);

    VrPose pose = vrComputePanelPoseTowards(head, centre,
      vrLength(centre - head.position), MaxRingPitch);
    pose.position = centre;
    return pose;
  }

}
