#include <vector>

#include "test.h"
#include "paint_probe.h"

#include "../vr_power_attack.h"

using namespace dxvk;

namespace {

  constexpr float HoldTime = 0.3f;

  float alphaAt(const VrPaintList& list, float x, float y) {
    return test::probePaint(list, x, y).a;
  }

}


TEST_CASE(power_cost_grows_with_weight_up_to_the_cap) {
  VrPowerAttackCost cost;

  CHECK(vrComputePowerAttackCost(cost, 0.0f) == 25);
  // A machete of 2 units: 25 + 20
  CHECK(vrComputePowerAttackCost(cost, 2.0f) == 45);
  // Truncated, not rounded
  CHECK(vrComputePowerAttackCost(cost, 2.95f) == 54);
  // A sledgehammer of 20 units hits the cap
  CHECK(vrComputePowerAttackCost(cost, 20.0f) == 150);
}


TEST_CASE(power_charge_starts_at_the_press) {
  VrPowerCharge charge;

  // The press frame adds nothing, however long it was
  charge.update(true, true, 1.0f, HoldTime);
  CHECK(charge.charging());
  CHECK(!charge.charged());
  CHECK_NEAR(charge.fill(), 0.0f, 1e-6);

  charge.update(true, true, 0.1f, HoldTime);
  CHECK_NEAR(charge.fill(), 1.0f / 3.0f, 1e-4);

  charge.update(true, true, 0.1f, HoldTime);
  CHECK(!charge.charged());

  charge.update(true, true, 0.11f, HoldTime);
  CHECK(charge.charged());
  CHECK_NEAR(charge.fill(), 1.0f, 1e-6);
}


TEST_CASE(power_charge_is_lost_on_release) {
  VrPowerCharge charge;

  charge.update(true, true, 0.0f, HoldTime);
  charge.update(true, true, 0.5f, HoldTime);
  CHECK(charge.charged());

  charge.update(false, true, 0.01f, HoldTime);
  CHECK(!charge.charging());
  CHECK(!charge.charged());

  // A new press starts again from zero
  charge.update(true, true, 0.5f, HoldTime);
  CHECK(!charge.charged());
}


TEST_CASE(power_charge_is_spent_by_a_hit) {
  VrPowerCharge charge;

  charge.update(true, true, 0.0f, HoldTime);
  charge.update(true, true, 0.5f, HoldTime);
  CHECK(charge.charged());

  charge.spend();
  CHECK(!charge.charged());
  CHECK(charge.charging());

  // Still held: the next charge builds up from zero
  charge.update(true, true, 0.2f, HoldTime);
  CHECK(!charge.charged());
  charge.update(true, true, 0.2f, HoldTime);
  CHECK(charge.charged());
}


TEST_CASE(power_charge_does_not_build_while_not_allowed) {
  VrPowerCharge charge;

  charge.update(true, false, 0.0f, HoldTime);
  charge.update(true, false, 1.0f, HoldTime);
  CHECK(!charge.charging());
  CHECK(!charge.charged());
  CHECK_NEAR(charge.fill(), 0.0f, 1e-6);

  // Allowed again: starts from zero, like a new press
  charge.update(true, true, 1.0f, HoldTime);
  CHECK(charge.charging());
  CHECK(!charge.charged());

  charge.update(true, true, 0.4f, HoldTime);
  CHECK(charge.charged());

  // Losing the right to it drops the charge
  charge.update(true, false, 0.01f, HoldTime);
  CHECK(!charge.charged());
}


TEST_CASE(power_refusal_tells_once_per_hold) {
  VrPowerRefusal refusal;

  // The press frame adds nothing, then the time of a charge must pass
  CHECK(!refusal.update(true, true, 1.0f, HoldTime));
  CHECK(!refusal.update(true, true, 0.2f, HoldTime));
  CHECK(refusal.update(true, true, 0.11f, HoldTime));

  // Told once while it stays held
  CHECK(!refusal.update(true, true, 1.0f, HoldTime));

  // Let go and held again: told again after a charge's time
  CHECK(!refusal.update(false, true, 0.1f, HoldTime));
  CHECK(!refusal.update(true, true, 0.0f, HoldTime));
  CHECK(!refusal.update(true, true, 0.2f, HoldTime));
  CHECK(refusal.update(true, true, 0.2f, HoldTime));
}


TEST_CASE(power_refusal_needs_a_whole_charge_short_of_action_points) {
  VrPowerRefusal refusal;

  // Enough action points: nothing to tell, however long
  CHECK(!refusal.update(true, false, 0.0f, HoldTime));
  CHECK(!refusal.update(true, false, 1.0f, HoldTime));

  // Short of them from now on: starts from zero
  CHECK(!refusal.update(true, true, 1.0f, HoldTime));
  CHECK(!refusal.update(true, true, 0.2f, HoldTime));

  // Enough again for a frame: the time starts over
  CHECK(!refusal.update(true, false, 0.2f, HoldTime));
  CHECK(!refusal.update(true, true, 0.2f, HoldTime));
  CHECK(!refusal.update(true, true, 0.2f, HoldTime));
  CHECK(refusal.update(true, true, 0.2f, HoldTime));
}


TEST_CASE(power_ring_fills_clockwise_from_the_top) {
  constexpr float Size = 64.0f;
  VrPaintList list(64, 64);

  // Points in the middle of the ring: right, left, then below
  const float centre = 0.5f * Size + 0.13f;
  const float ring   = 0.395f * Size;

  vrPaintChargeRing(list, 0.5f, false);

  // Half full: the right half is lit, the left half is the track
  CHECK(alphaAt(list, centre, centre) < 0.01f);
  CHECK(alphaAt(list, 0.3f, 0.2f) < 0.01f);
  CHECK_NEAR(alphaAt(list, centre + ring, centre), 0.8f, 0.02f);
  CHECK_NEAR(alphaAt(list, centre - ring, centre), 0.3f, 0.02f);
  CHECK_NEAR(alphaAt(list, centre + 4.0f, centre + ring - 0.4f), 0.8f, 0.02f);
  CHECK_NEAR(alphaAt(list, centre - 5.0f, centre + ring - 0.4f), 0.3f, 0.02f);

  // Charged: the whole ring is opaque
  vrPaintChargeRing(list, 0.0f, true);
  CHECK(alphaAt(list, centre - ring, centre) > 0.99f);
  CHECK(alphaAt(list, centre + ring, centre) > 0.99f);
  CHECK(alphaAt(list, centre, centre) < 0.01f);

  // Empty: only the track, in the amber of the Pip-Boy
  vrPaintChargeRing(list, 0.0f, false);
  test::PaintProbe track = test::probePaint(list, centre + ring, centre);
  CHECK_NEAR(track.a, 0.3f, 0.02f);
  CHECK_NEAR(track.r / track.a, 1.0f, 0.02f);
  CHECK_NEAR(track.g / track.a, float(0xB6) / 255.0f, 0.02f);
  CHECK_NEAR(track.b / track.a, float(0x42) / 255.0f, 0.02f);
}
