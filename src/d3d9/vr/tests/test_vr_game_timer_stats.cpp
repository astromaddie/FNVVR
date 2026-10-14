#include "test.h"

#include "../vr_game_timer_stats.h"

using namespace dxvk;

namespace {

  VrGameTimerSample sample(double realMs, double gameMs, double updateMs = 0.0, bool noTick = false) {
    VrGameTimerSample result;
    result.realMs   = realMs;
    result.gameMs   = gameMs;
    result.updateMs = updateMs;
    result.noTick   = noTick;
    return result;
  }

}

TEST_CASE(gameTimerStatsFillsTheWindow) {
  VrGameTimerStats stats(30.0);

  CHECK(!stats.add(sample(10.0, 10.0)));
  CHECK(!stats.add(sample(10.0, 12.0)));
  CHECK(stats.add(sample(10.0, 14.0)));
  CHECK(stats.frames() == 3);

  // A full window ignores further updates until it is reset
  CHECK(stats.add(sample(10.0, 100.0)));
  CHECK(stats.frames() == 3);
  CHECK_NEAR(stats.seconds(), 0.03, 1e-9);
  CHECK_NEAR(stats.averageRealMs(), 10.0, 1e-9);
  CHECK_NEAR(stats.averageGameMs(), 12.0, 1e-9);
  CHECK_NEAR(stats.driftMs(), 6.0, 1e-9);
  CHECK_NEAR(stats.averageErrorMs(), 2.0, 1e-9);
  CHECK_NEAR(stats.maxErrorMs(), 4.0, 1e-9);

  stats.reset();
  CHECK(stats.frames() == 0);
  CHECK_NEAR(stats.driftMs(), 0.0, 1e-9);
  CHECK(stats.commonSteps()[0].frames == 0);
}

TEST_CASE(gameTimerStatsLeavesOutLongIntervals) {
  VrGameTimerStats stats(1000.0);

  stats.add(sample(11.0, 11.0));
  stats.add(sample(VrGameTimerStats::LongIntervalMs, 166.0));
  stats.add(sample(11.0, 11.0));

  CHECK(stats.frames() == 2);
  CHECK(stats.longIntervals() == 1);
  CHECK_NEAR(stats.seconds(), 0.022, 1e-9);
}

TEST_CASE(gameTimerStatsCountsSleeps) {
  VrGameTimerStats stats(1000.0);

  // At 90 Hz with a tick count that advances every 15.6 ms, some updates
  // see no tick: the game takes 10 ms and sleeps 9 ms in the update
  stats.add(sample(11.1, 16.0));
  stats.add(sample(11.1, 10.0, 9.4, true));
  stats.add(sample(11.1, 15.0, 0.01));

  CHECK(stats.noTickFrames() == 1);
  CHECK(stats.sleptFrames() == 1);
  CHECK_NEAR(stats.averageSleptMs(), 9.4, 1e-9);
  CHECK_NEAR(stats.maxUpdateMs(), 9.4, 1e-9);
}

TEST_CASE(gameTimerStatsListsCommonSteps) {
  VrGameTimerStats stats(1000.0);

  for (uint32_t i = 0; i < 3; i++)
    stats.add(sample(11.0, 16.0));

  for (uint32_t i = 0; i < 5; i++)
    stats.add(sample(11.0, 10.0));

  stats.add(sample(11.0, 15.2));
  stats.add(sample(11.0, 400.0));

  auto steps = stats.commonSteps();
  CHECK(steps[0].ms == 10 && steps[0].frames == 5);
  CHECK(steps[1].ms == 16 && steps[1].frames == 3);
  CHECK(steps[2].frames == 1);
  CHECK(steps[3].frames == 1);
  CHECK(steps[3].ms > steps[2].ms);
  CHECK(steps[3].ms == VrGameTimerStats::StepCounts - 1);
}
