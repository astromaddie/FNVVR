#include <vector>

#include "test.h"

#include "../vr_power_attack.h"

using namespace dxvk;

namespace {

  constexpr float HoldTime = 0.3f;

  uint32_t alphaAt(const std::vector<uint32_t>& pixels, uint32_t size, uint32_t x, uint32_t y) {
    return pixels[y * size + x] >> 24;
  }

}


TEST_CASE(power_cost_grows_with_weight_up_to_the_cap) {
  VrPowerAttackCost cost;

  CHECK(vrComputePowerAttackCost(cost, 0.0f) == 5);
  // A machete of 2 units: 5 + 4
  CHECK(vrComputePowerAttackCost(cost, 2.0f) == 9);
  // Truncated, not rounded
  CHECK(vrComputePowerAttackCost(cost, 2.9f) == 10);
  // A sledgehammer of 20 units hits the cap
  CHECK(vrComputePowerAttackCost(cost, 20.0f) == 30);
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


TEST_CASE(power_ring_fills_clockwise_from_the_top) {
  constexpr uint32_t Size = 64;
  std::vector<uint32_t> pixels(Size * Size);

  // Points in the middle of the ring: top, right, bottom, left
  const uint32_t ring = uint32_t(0.5f * Size - 0.395f * Size);

  vrDrawChargeRing(pixels.data(), Size, 0.5f, false);

  uint32_t centre = alphaAt(pixels, Size, Size / 2, Size / 2);
  uint32_t corner = alphaAt(pixels, Size, 0, 0);
  uint32_t right  = alphaAt(pixels, Size, Size - 1 - ring, Size / 2);
  uint32_t left   = alphaAt(pixels, Size, ring, Size / 2);
  uint32_t bottomRight = alphaAt(pixels, Size, Size / 2 + 4, Size - 1 - ring);
  uint32_t bottomLeft  = alphaAt(pixels, Size, Size / 2 - 5, Size - 1 - ring);

  CHECK(centre == 0);
  CHECK(corner == 0);
  // Half full: the right half is lit, the left half is the track
  CHECK(right > 150);
  CHECK(bottomRight > 150);
  CHECK(left > 40 && left < 120);
  CHECK(bottomLeft > 40 && bottomLeft < 120);

  // Charged: the whole ring is opaque
  vrDrawChargeRing(pixels.data(), Size, 0.0f, true);
  CHECK(alphaAt(pixels, Size, ring, Size / 2) == 255);
  CHECK(alphaAt(pixels, Size, Size - 1 - ring, Size / 2) == 255);
  CHECK(alphaAt(pixels, Size, Size / 2, Size / 2) == 0);

  // Empty: only the track
  vrDrawChargeRing(pixels.data(), Size, 0.0f, false);
  CHECK(alphaAt(pixels, Size, Size - 1 - ring, Size / 2) < 120);
  CHECK((pixels[ring * Size + Size / 2] & 0xFFFFFF) == 0xFFB642);
}
