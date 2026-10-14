#include "test.h"

#include "../vr_action_filter.h"

using namespace dxvk;

namespace {

  constexpr float Frame = 1.0f / 90.0f;

  VrActionState pressed(VrAction action) {
    VrActionState actions;
    actions.setPressed(action, true);
    return actions;
  }

  // Holds the PauseHold action for a duration, then releases it, and counts
  // the frames that report each output
  struct HoldPress {
    uint32_t holdFrames  = 0;
    uint32_t pauseFrames = 0;
    uint32_t waitFrames  = 0;

    HoldPress(VrActionFilter& filter, float duration) {
      for (float t = 0.0f; t < duration; t += Frame)
        count(filter.update(pressed(VrAction::PauseHold), VrInputContext::Game, Frame));

      for (uint32_t i = 0; i < 30; i++)
        count(filter.update(VrActionState(), VrInputContext::Game, Frame));
    }

    void count(const VrActionState& actions) {
      holdFrames  += actions.isPressed(VrAction::PauseHold) ? 1 : 0;
      pauseFrames += actions.isPressed(VrAction::Pause)     ? 1 : 0;
      waitFrames  += actions.isPressed(VrAction::Wait)      ? 1 : 0;
    }
  };

}


TEST_CASE(a_short_press_of_pause_hold_waits_once) {
  VrActionFilter filter;

  HoldPress tap(filter, 0.2f);
  CHECK(tap.holdFrames == 0);
  CHECK(tap.pauseFrames == 0);
  CHECK(tap.waitFrames > 0);
  CHECK(tap.waitFrames <= 10);
}


TEST_CASE(holding_pause_hold_pauses_once) {
  VrActionFilter filter;

  HoldPress hold(filter, 2.0f);
  CHECK(hold.holdFrames == 0);
  CHECK(hold.pauseFrames > 0);
  CHECK(hold.pauseFrames <= 10);
  CHECK(hold.waitFrames == 0);
}


TEST_CASE(a_pulse_lasts_at_least_one_frame) {
  VrActionFilter filter;

  VrActionState result = filter.update(pressed(VrAction::PauseHold), VrInputContext::Game, 1.0f);
  result = filter.update(pressed(VrAction::PauseHold), VrInputContext::Game, 1.0f);
  CHECK(result.isPressed(VrAction::Pause));

  result = filter.update(VrActionState(), VrInputContext::Game, 0.5f);
  CHECK(!result.isPressed(VrAction::Pause));
}


TEST_CASE(a_press_interrupted_by_a_menu_does_not_wait) {
  VrActionFilter filter;
  filter.update(pressed(VrAction::PauseHold), VrInputContext::Game, Frame);
  filter.update(pressed(VrAction::PauseHold), VrInputContext::Menu, Frame);
  filter.update(VrActionState(), VrInputContext::Menu, Frame);
  CHECK(!filter.update(VrActionState(), VrInputContext::Game, Frame).isPressed(VrAction::Wait));
}


TEST_CASE(bound_pause_action_still_works) {
  VrActionFilter filter;
  CHECK(filter.update(pressed(VrAction::Pause), VrInputContext::Game, Frame).isPressed(VrAction::Pause));
}


TEST_CASE(actions_held_across_a_context_change_are_ignored_until_released) {
  VrActionFilter filter;
  filter.update(pressed(VrAction::Jump), VrInputContext::Game, Frame);

  VrActionState back = pressed(VrAction::MenuBack);
  CHECK(!filter.update(back, VrInputContext::Menu, Frame).isPressed(VrAction::MenuBack));
  CHECK(!filter.update(back, VrInputContext::Menu, Frame).isPressed(VrAction::MenuBack));

  filter.update(VrActionState(), VrInputContext::Menu, Frame);
  CHECK(filter.update(back, VrInputContext::Menu, Frame).isPressed(VrAction::MenuBack));

  // Leaving the menu with the button still down does not jump
  VrActionState jump = pressed(VrAction::Jump);
  CHECK(!filter.update(jump, VrInputContext::Game, Frame).isPressed(VrAction::Jump));
}


TEST_CASE(other_actions_and_sticks_pass_through) {
  VrActionFilter filter;
  VrActionState actions = pressed(VrAction::Attack);
  actions.move = { 0.5f, -0.25f };

  VrActionState result = filter.update(actions, VrInputContext::Game, Frame);
  CHECK(result.isPressed(VrAction::Attack));
  CHECK(result.move.x == 0.5f && result.move.y == -0.25f);
}


TEST_CASE(reload_activates_while_the_prompt_is_at_the_left_hand) {
  VrActionFilter filter;
  filter.setReloadActivates(true);

  VrActionState result = filter.update(pressed(VrAction::Reload), VrInputContext::Game, Frame);
  CHECK(result.isPressed(VrAction::Activate));
  CHECK(!result.isPressed(VrAction::Reload));

  filter.update(VrActionState(), VrInputContext::Game, Frame);
  filter.setReloadActivates(false);

  result = filter.update(pressed(VrAction::Reload), VrInputContext::Game, Frame);
  CHECK(result.isPressed(VrAction::Reload));
  CHECK(!result.isPressed(VrAction::Activate));
}


TEST_CASE(a_held_reload_keeps_what_it_did_when_the_prompt_moves) {
  VrActionFilter filter;

  VrActionState result = filter.update(pressed(VrAction::Reload), VrInputContext::Game, Frame);
  CHECK(result.isPressed(VrAction::Reload));

  filter.setReloadActivates(true);
  result = filter.update(pressed(VrAction::Reload), VrInputContext::Game, Frame);
  CHECK(result.isPressed(VrAction::Reload));
  CHECK(!result.isPressed(VrAction::Activate));

  filter.update(VrActionState(), VrInputContext::Game, Frame);
  result = filter.update(pressed(VrAction::Reload), VrInputContext::Game, Frame);
  CHECK(result.isPressed(VrAction::Activate));

  filter.setReloadActivates(false);
  result = filter.update(pressed(VrAction::Reload), VrInputContext::Game, Frame);
  CHECK(result.isPressed(VrAction::Activate));
  CHECK(!result.isPressed(VrAction::Reload));
}
