#include "test.h"

#include "../vr_frame_stats.h"

using namespace dxvk;

namespace {

  constexpr double PeriodMs = 1000.0 / 90.0;

  VrFrameCycle cycle(double intervalMs, double renderMs) {
    VrFrameCycle result = { };
    result[uint32_t(VrFrameStep::Interval)] = intervalMs;
    result[uint32_t(VrFrameStep::Render)]   = renderMs;
    return result;
  }

}

TEST_CASE(frameStatsFillsTheWindow) {
  VrFrameStats stats(3, PeriodMs);

  CHECK(!stats.add(cycle(PeriodMs, 4.0)));
  CHECK(!stats.add(cycle(PeriodMs, 6.0)));
  CHECK(stats.add(cycle(PeriodMs, 8.0)));
  CHECK(stats.frames() == 3);

  // A full window ignores further frames until it is reset
  CHECK(stats.add(cycle(PeriodMs, 100.0)));
  CHECK(stats.frames() == 3);
  CHECK_NEAR(stats.average(VrFrameStep::Render), 6.0, 1e-9);
  CHECK_NEAR(stats.step(VrFrameStep::Render).max, 8.0, 1e-9);
  CHECK_NEAR(stats.seconds(), 3.0 * PeriodMs / 1000.0, 1e-9);

  stats.reset();
  CHECK(stats.frames() == 0);
  CHECK(stats.lateFrames() == 0);
  CHECK_NEAR(stats.average(VrFrameStep::Render), 0.0, 1e-9);
}

TEST_CASE(frameStatsCountsLateFrames) {
  VrFrameStats stats(4, PeriodMs);

  // Two vsyncs, slightly early, and one vsync with some jitter
  stats.add(cycle(2.0 * PeriodMs - 0.5, 0.0));
  stats.add(cycle(PeriodMs + 1.0, 0.0));
  stats.add(cycle(2.0 * PeriodMs + 0.5, 0.0));
  stats.add(cycle(PeriodMs - 1.0, 0.0));

  CHECK(stats.lateFrames() == 2);
}

TEST_CASE(frameStatsCountsBusyFrames) {
  VrFrameStats stats(3, PeriodMs);

  // Busy for less than a period then waiting, busy for longer than a
  // period with no wait, and late only because of the wait
  VrFrameCycle cycles[3] = { cycle(PeriodMs, 8.0), cycle(2.0 * PeriodMs, 12.0), cycle(2.0 * PeriodMs, 4.0) };
  cycles[0][uint32_t(VrFrameStep::Wait)] = PeriodMs - 8.0;
  cycles[2][uint32_t(VrFrameStep::Wait)] = 2.0 * PeriodMs - 4.0;

  for (const VrFrameCycle& c : cycles)
    stats.add(c);

  CHECK(stats.lateFrames() == 2);
  CHECK(stats.busyFrames() == 1);

  stats.reset();
  CHECK(stats.busyFrames() == 0);
}

TEST_CASE(frameStatsNeedsOneFrame) {
  VrFrameStats stats(0, PeriodMs);
  CHECK(stats.add(cycle(PeriodMs, 0.0)));
}
