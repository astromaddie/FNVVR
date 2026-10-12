#include "test.h"

#include "../vr_turn_input.h"

using namespace dxvk;

namespace {

  constexpr float StepRadians = 30.0f * 0.017453293f;

}


TEST_CASE(snap_turn_fires_once_per_push) {
  VrTurnInput turn = VrTurnInput(VrTurnConfig());

  CHECK_NEAR(turn.update(0.0f, 0.01f), 0.0f, 1e-6);
  CHECK_NEAR(turn.update(0.9f, 0.01f), StepRadians, 1e-6);

  // Held: no second step
  CHECK_NEAR(turn.update(1.0f, 0.01f), 0.0f, 1e-6);
  CHECK_NEAR(turn.update(0.5f, 0.01f), 0.0f, 1e-6);

  // Released, then pushed left
  CHECK_NEAR(turn.update(0.1f, 0.01f), 0.0f, 1e-6);
  CHECK_NEAR(turn.update(-0.8f, 0.01f), -StepRadians, 1e-6);
}


TEST_CASE(snap_turn_waits_for_release_after_reset) {
  VrTurnInput turn = VrTurnInput(VrTurnConfig());
  turn.update(0.0f, 0.01f);
  turn.reset();

  CHECK_NEAR(turn.update(0.9f, 0.01f), 0.0f, 1e-6);
  CHECK_NEAR(turn.update(0.0f, 0.01f), 0.0f, 1e-6);
  CHECK_NEAR(turn.update(0.9f, 0.01f), StepRadians, 1e-6);
}


TEST_CASE(smooth_turn_scales_with_deflection_and_time) {
  VrTurnConfig config;
  config.smooth      = true;
  config.smoothSpeed = 90.0f;
  VrTurnInput turn(config);

  CHECK_NEAR(turn.update(0.1f, 1.0f), 0.0f, 1e-6);
  CHECK_NEAR(turn.update(1.0f, 0.5f), 45.0f * 0.017453293f, 1e-5);
  CHECK_NEAR(turn.update(-1.0f, 1.0f), -90.0f * 0.017453293f, 1e-5);
  CHECK(turn.update(0.5f, 1.0f) > 0.0f);
  CHECK(turn.update(0.5f, 1.0f) < 90.0f * 0.017453293f);
}


TEST_CASE(switching_to_smooth_turn_takes_effect_at_once) {
  VrTurnInput turn = VrTurnInput(VrTurnConfig());
  turn.update(0.0f, 0.01f);

  VrTurnConfig config;
  config.smooth = true;
  turn.setConfig(config);
  CHECK(turn.update(1.0f, 1.0f) > StepRadians);
  CHECK(turn.update(1.0f, 0.01f) > 0.0f);

  config.smooth = false;
  turn.setConfig(config);
  CHECK_NEAR(turn.update(1.0f, 0.01f), 0.0f, 1e-6);
  CHECK_NEAR(turn.update(0.0f, 0.01f), 0.0f, 1e-6);
  CHECK_NEAR(turn.update(1.0f, 0.01f), StepRadians, 1e-6);
}


TEST_CASE(smooth_turn_speed_can_change_while_turning) {
  VrTurnConfig config;
  config.smooth      = true;
  config.smoothSpeed = 60.0f;
  VrTurnInput turn(config);

  CHECK_NEAR(turn.update(1.0f, 1.0f), 60.0f * 0.017453293f, 1e-5);

  config.smoothSpeed = 240.0f;
  turn.setConfig(config);
  CHECK_NEAR(turn.update(1.0f, 1.0f), 240.0f * 0.017453293f, 1e-5);
}
