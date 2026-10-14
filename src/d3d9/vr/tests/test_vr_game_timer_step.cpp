#include "test.h"

#include "../vr_game_timer_step.h"

using namespace dxvk;

TEST_CASE(gameTimerStepFollowsTheRealInterval) {
  CHECK_NEAR(vrGameTimerStepMs(13.89), 13.89, 1e-9);
  CHECK_NEAR(vrGameTimerStepMs(11.11), 11.11, 1e-9);
}

TEST_CASE(gameTimerStepKeepsTheGamesLimits) {
  CHECK_NEAR(vrGameTimerStepMs(0.0), VrGameTimerMinStepMs, 1e-9);
  CHECK_NEAR(vrGameTimerStepMs(-5.0), VrGameTimerMinStepMs, 1e-9);
  CHECK_NEAR(vrGameTimerStepMs(2500.0), VrGameTimerMaxStepMs, 1e-9);
}

TEST_CASE(havokStepIsTheFrameWhenFasterThanTheSetting) {
  CHECK_NEAR(vrHavokMaxStep(11.11, 1.0f / 60.0f), 0.01111f, 1e-6);
  CHECK_NEAR(vrHavokMaxStep(13.89, 1.0f / 60.0f), 0.01389f, 1e-6);
}

TEST_CASE(havokStepKeepsTheSettingWhenSlower) {
  CHECK_NEAR(vrHavokMaxStep(25.0, 1.0f / 60.0f), 1.0f / 60.0f, 1e-7);
  CHECK_NEAR(vrHavokMaxStep(16.0, 0.01f), 0.01f, 1e-7);
}
