#include <algorithm>
#include <cmath>

#include "vr_game_timer_stats.h"

namespace dxvk {

  VrGameTimerStats::VrGameTimerStats(double windowMs)
  : m_windowMs(windowMs) { }


  bool VrGameTimerStats::add(const VrGameTimerSample& sample) {
    if (m_realSum >= m_windowMs)
      return true;

    if (sample.realMs >= LongIntervalMs) {
      m_long++;
      return false;
    }

    double error = std::abs(sample.gameMs - sample.realMs);

    m_frames++;
    m_realSum  += sample.realMs;
    m_gameSum  += sample.gameMs;
    m_errorSum += error;
    m_errorMax  = std::max(m_errorMax, error);
    m_updateMax = std::max(m_updateMax, sample.updateMs);

    if (sample.noTick)
      m_noTick++;

    if (sample.updateMs >= SleptMs) {
      m_slept++;
      m_sleptSum += sample.updateMs;
    }

    double step = std::max(std::round(sample.gameMs), 0.0);
    m_steps[std::min(uint32_t(step), StepCounts - 1)]++;

    return m_realSum >= m_windowMs;
  }


  void VrGameTimerStats::reset() {
    *this = VrGameTimerStats(m_windowMs);
  }


  double VrGameTimerStats::averageRealMs() const {
    return m_frames ? m_realSum / double(m_frames) : 0.0;
  }


  double VrGameTimerStats::averageGameMs() const {
    return m_frames ? m_gameSum / double(m_frames) : 0.0;
  }


  double VrGameTimerStats::averageErrorMs() const {
    return m_frames ? m_errorSum / double(m_frames) : 0.0;
  }


  double VrGameTimerStats::averageSleptMs() const {
    return m_slept ? m_sleptSum / double(m_slept) : 0.0;
  }


  std::array<VrGameTimerStep, VrGameTimerStats::CommonSteps> VrGameTimerStats::commonSteps() const {
    std::array<VrGameTimerStep, CommonSteps> result = { };

    for (uint32_t ms = 0; ms < StepCounts; ms++) {
      VrGameTimerStep step = { ms, m_steps[ms] };

      for (uint32_t i = 0; i < CommonSteps && step.frames; i++) {
        if (step.frames > result[i].frames)
          std::swap(step, result[i]);
      }
    }

    return result;
  }

}
