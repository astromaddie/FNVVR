#include <algorithm>

#include "vr_frame_stats.h"

namespace dxvk {

  VrFrameStats::VrFrameStats(uint32_t windowFrames, double periodMs)
  : m_windowFrames(std::max(windowFrames, 1u)), m_periodMs(periodMs) { }


  bool VrFrameStats::add(const VrFrameCycle& cycle) {
    if (m_frames >= m_windowFrames)
      return true;

    for (uint32_t i = 0; i < VrFrameStepCount; i++) {
      m_steps[i].sum += cycle[i];
      m_steps[i].max  = std::max(m_steps[i].max, cycle[i]);
    }

    double interval = cycle[uint32_t(VrFrameStep::Interval)];

    if (interval >= m_periodMs * 1.5)
      m_late++;

    if (interval - cycle[uint32_t(VrFrameStep::Wait)] > m_periodMs)
      m_busy++;

    return ++m_frames >= m_windowFrames;
  }


  void VrFrameStats::reset() {
    m_frames = 0;
    m_late   = 0;
    m_busy   = 0;
    m_steps  = { };
  }


  double VrFrameStats::average(VrFrameStep step) const {
    return m_frames ? m_steps[uint32_t(step)].sum / double(m_frames) : 0.0;
  }

}
