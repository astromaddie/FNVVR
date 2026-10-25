#include "test.h"

#include "../vr_quick_wheel_image.h"

using namespace dxvk;

namespace {

  float alphaAt(const VrCanvas& canvas, float x, float y) {
    return float(canvas.at(uint32_t(x), uint32_t(y)) >> 24) / 255.0f;
  }

}


TEST_CASE(quick_wheel_cells_follow_the_gamepad_layout) {
  // Up is the ammunition swap, then clockwise as the D-pad's diagonals
  CHECK(vrQuickWheelSlot(0) == 1);
  CHECK(vrQuickWheelSlot(1) == 2);
  CHECK(vrQuickWheelSlot(2) == 4);
  CHECK(vrQuickWheelSlot(3) == 7);
  CHECK(vrQuickWheelSlot(4) == 6);
  CHECK(vrQuickWheelSlot(5) == 5);
  CHECK(vrQuickWheelSlot(6) == 3);
  CHECK(vrQuickWheelSlot(7) == 0);

  for (uint32_t cell = 0; cell < VrQuickWheelCellCount; cell++)
    CHECK(vrQuickWheelCell(vrQuickWheelSlot(cell)) == cell);
}


TEST_CASE(quick_wheel_picks_the_cell_towards_the_hand) {
  VrQuickWheelPicking picking;

  CHECK(vrPickQuickWheelCell({ 0.0f, 0.0f }, -1, picking) == -1);
  CHECK(vrPickQuickWheelCell({ 0.02f, 0.0f }, -1, picking) == -1);
  CHECK(vrPickQuickWheelCell({ 0.0f, 0.06f }, -1, picking) == 0);
  CHECK(vrPickQuickWheelCell({ 0.05f, 0.05f }, -1, picking) == 1);
  CHECK(vrPickQuickWheelCell({ 0.06f, 0.0f }, -1, picking) == 2);
  CHECK(vrPickQuickWheelCell({ 0.0f, -0.06f }, -1, picking) == 4);
  CHECK(vrPickQuickWheelCell({ -0.06f, 0.0f }, -1, picking) == 6);
  CHECK(vrPickQuickWheelCell({ -0.05f, 0.05f }, -1, picking) == 7);
  // Just left of the top still picks the top
  CHECK(vrPickQuickWheelCell({ -0.01f, 0.06f }, -1, picking) == 0);
}


TEST_CASE(quick_wheel_keeps_a_cell_past_its_edge) {
  VrQuickWheelPicking picking;

  // 25 degrees right of the top: past the edge at 22.5, within the margin
  VrVector2 past = { 0.06f * std::sin(0.436f), 0.06f * std::cos(0.436f) };
  CHECK(vrPickQuickWheelCell(past, -1, picking) == 1);
  CHECK(vrPickQuickWheelCell(past, 0, picking) == 0);

  // Back towards the middle, the cell is kept a little nearer
  CHECK(vrPickQuickWheelCell({ 0.0f, 0.03f }, 0, picking) == 0);
  CHECK(vrPickQuickWheelCell({ 0.0f, 0.03f }, -1, picking) == -1);
  CHECK(vrPickQuickWheelCell({ 0.0f, 0.02f }, 0, picking) == -1);
}


TEST_CASE(canvas_sector_covers_its_angles_only) {
  VrCanvas canvas(100, 100);
  VrColour white = { 1.0f, 1.0f, 1.0f, 1.0f };

  // The right half of a ring, clockwise from the top
  canvas.fillSector(50.0f, 50.0f, 20.0f, 40.0f, 0.0f, 3.14159f, white);

  CHECK(alphaAt(canvas, 80.0f, 50.0f) > 0.99f);
  CHECK(alphaAt(canvas, 20.0f, 50.0f) < 0.01f);
  // The hole and outside the ring
  CHECK(alphaAt(canvas, 55.0f, 50.0f) < 0.01f);
  CHECK(alphaAt(canvas, 95.0f, 50.0f) < 0.01f);
}


TEST_CASE(canvas_blends_over_what_is_there) {
  VrCanvas canvas(10, 10);
  canvas.fillSector(5.0f, 5.0f, 0.0f, 20.0f, 0.0f, 7.0f, { 0.0f, 0.0f, 0.0f, 0.5f });
  canvas.fillSector(5.0f, 5.0f, 0.0f, 20.0f, 0.0f, 7.0f, { 1.0f, 0.0f, 0.0f, 0.5f });

  uint32_t pixel = canvas.at(5, 5);
  CHECK_NEAR(float(pixel >> 24) / 255.0f, 0.75f, 0.01f);
  // Red over black: two thirds red once the alpha is taken out
  CHECK_NEAR(float((pixel >> 16) & 0xFF) / 255.0f, 2.0f / 3.0f, 0.01f);
  CHECK((pixel & 0xFFFF) == 0);
}


TEST_CASE(canvas_shape_takes_the_image_alpha_and_brightness) {
  VrImage image;
  image.width  = 2;
  image.height = 1;
  image.pixels = { 0xFFFFFFFFu, 0x00FFFFFFu };

  VrCanvas canvas(20, 20);
  canvas.drawShape(image, 10.0f, 10.0f, 20.0f, { 0.0f, 1.0f, 0.0f, 1.0f });

  // The image is twice as wide as tall: it fills the middle band
  CHECK(alphaAt(canvas, 2.0f, 10.0f) > 0.9f);
  CHECK(alphaAt(canvas, 18.0f, 10.0f) < 0.1f);
  CHECK(alphaAt(canvas, 2.0f, 1.0f) < 0.01f);
}


TEST_CASE(quick_wheel_lights_the_picked_cell) {
  VrCanvas canvas(VrQuickWheelImageLayout::ImageSize, VrQuickWheelImageLayout::ImageSize);
  VrQuickWheelView view;
  view.picked = 2;

  vrDrawQuickWheel(canvas, view, nullptr);

  float size   = float(VrQuickWheelImageLayout::ImageSize);
  float centre = 0.5f * size;
  float radius = 0.40f * size;

  // The right cell is picked, the left one is not
  float picked = alphaAt(canvas, centre + radius, centre);
  float other  = alphaAt(canvas, centre - radius, centre);
  CHECK(picked > other + 0.1f);
  // Outside the wheel nothing is drawn
  CHECK(alphaAt(canvas, 2.0f, 2.0f) < 0.01f);
}
