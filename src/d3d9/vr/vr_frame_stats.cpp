#include <algorithm>

#include "vr_frame_stats.h"

namespace dxvk {

  VrFrameStats::VrFrameStats(uint32_t windowFrames, double periodMs)
  : m_windowFrames(std::max(windowFrames, 1u)), m_lateMs(periodMs * 1.5) { }


  bool VrFrameStats::add(const VrFrameCycle& cycle) {
    if (m_frames >= m_windowFrames)
      return true;

    for (uint32_t i = 0; i < VrFrameStepCount; i++) {
      m_steps[i].sum += cycle[i];
      m_steps[i].max  = std::max(m_steps[i].max, cycle[i]);
    }

    if (cycle[uint32_t(VrFrameStep::Interval)] >= m_lateMs)
      m_late++;

    return ++m_frames >= m_windowFrames;
  }


  void VrFrameStats::reset() {
    m_frames = 0;
    m_late   = 0;
    m_steps  = { };
  }


  double VrFrameStats::average(VrFrameStep step) const {
    return m_frames ? m_steps[uint32_t(step)].sum / double(m_frames) : 0.0;
  }

}
