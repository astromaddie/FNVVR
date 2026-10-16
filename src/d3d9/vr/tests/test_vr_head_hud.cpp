#include "test.h"

#include "../vr_head_hud.h"

using namespace dxvk;

namespace {

  // Headset resolution, 2 pixels per interface unit
  const VrVector2 Image = { 2560.0f, 1920.0f };
  const VrVector2 Interface = { 1280.0f, 960.0f };

  bool overlaps(const VrInterfaceRect& a, const VrInterfaceRect& b) {
    return a.left < b.right && b.left < a.right && a.top < b.bottom && b.top < a.bottom;
  }

}


TEST_CASE(xp_meter_crop_reaches_the_right_side) {
  // The game's position without safe zone, moved to the messages' row
  VrInterfaceRect crop = vrComputeXpMeterCrop({ 890.0f, 25.0f, 890.0f, 25.0f }, Interface);

  CHECK_NEAR(crop.left, 878.0f, 1e-3f);
  CHECK_NEAR(crop.top, 25.0f, 1e-3f);
  CHECK_NEAR(crop.right, 1280.0f, 1e-3f);
  CHECK_NEAR(crop.bottom, 281.0f, 1e-3f);
}


TEST_CASE(xp_meter_crop_stays_on_screen) {
  VrInterfaceRect crop = vrComputeXpMeterCrop({ 5.0f, 800.0f, 5.0f, 800.0f }, Interface);

  CHECK_NEAR(crop.left, 0.0f, 1e-3f);
  CHECK_NEAR(crop.bottom, 960.0f, 1e-3f);
}


TEST_CASE(head_hud_without_xp_meter_shifts_the_upper_half) {
  VrHeadHudLayout layout = vrComputeHeadHudLayout(Image, 400.0f, nullptr);
  CHECK(layout.count == 2);

  // Lower half unchanged
  CHECK_NEAR(layout.copies[0].source.top, 960.0f, 1e-3f);
  CHECK_NEAR(layout.copies[0].target.left, 0.0f, 1e-3f);
  CHECK_NEAR(layout.copies[0].target.right, 2560.0f, 1e-3f);

  // Upper half moved right, cut at the image's side
  CHECK_NEAR(layout.copies[1].source.right, 2160.0f, 1e-3f);
  CHECK_NEAR(layout.copies[1].target.left, 400.0f, 1e-3f);
  CHECK_NEAR(layout.copies[1].target.right, 2560.0f, 1e-3f);
}


TEST_CASE(head_hud_moves_the_xp_meter_towards_the_centre) {
  const VrInterfaceRect xp = { 1756.0f, 50.0f, 2560.0f, 562.0f };
  VrHeadHudLayout layout = vrComputeHeadHudLayout(Image, 400.0f, &xp);
  CHECK(layout.count == 3);

  // The upper half stops before the meter, so the meter is copied once
  CHECK_NEAR(layout.copies[1].source.right, 1756.0f, 1e-3f);
  CHECK_NEAR(layout.copies[1].target.right, 2156.0f, 1e-3f);

  const VrHudCopy& meter = layout.copies[2];
  CHECK_NEAR(meter.source.left, 1756.0f, 1e-3f);
  CHECK_NEAR(meter.target.left, 1356.0f, 1e-3f);
  CHECK_NEAR(meter.target.right, 2160.0f, 1e-3f);
  CHECK_NEAR(meter.target.top, 50.0f, 1e-3f);
  CHECK_NEAR(meter.target.bottom, 562.0f, 1e-3f);

  // The meter is copied last, over the empty right part of the upper half,
  // and leaves the lower half's subtitles alone
  CHECK(!overlaps(meter.target, layout.copies[0].target));
}


TEST_CASE(head_hud_keeps_the_xp_meter_in_the_upper_half) {
  const VrInterfaceRect xp = { 1756.0f, 800.0f, 2560.0f, 1312.0f };
  VrHeadHudLayout layout = vrComputeHeadHudLayout(Image, 400.0f, &xp);
  CHECK(layout.count == 3);
  CHECK_NEAR(layout.copies[2].source.bottom, 960.0f, 1e-3f);
}
