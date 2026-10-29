#include "test.h"
#include "paint_probe.h"

#include "../vr_quick_wheel_image.h"

using namespace dxvk;

namespace {

  float alphaAt(const VrPaintList& list, float x, float y) {
    return test::probePaint(list, x, y).a;
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


TEST_CASE(paint_sector_covers_its_angles_only) {
  VrPaintList list(100, 100);
  VrColour white = { 1.0f, 1.0f, 1.0f, 1.0f };

  // The right half of a ring, clockwise from the top
  list.fillSector(50.0f, 50.0f, 20.0f, 40.0f, 0.0f, 3.14159f, white);

  CHECK(alphaAt(list, 80.3f, 50.2f) > 0.99f);
  CHECK(alphaAt(list, 20.3f, 50.2f) < 0.01f);
  // The hole and outside the ring
  CHECK(alphaAt(list, 55.3f, 50.2f) < 0.01f);
  CHECK(alphaAt(list, 95.3f, 50.2f) < 0.01f);
  // The edges fade out over a pixel
  CHECK_NEAR(alphaAt(list, 50.0f, 20.3f), 0.5f, 0.1f);
  CHECK_NEAR(alphaAt(list, 90.0f, 50.2f), 0.5f, 0.1f);
}


TEST_CASE(paint_premultiplies_its_colours) {
  CHECK(vrPremultiplied({ 1.0f, 0.0f, 0.0f, 0.5f }) == 0x80800000u);
  CHECK(vrPremultiplied({ 1.0f, 1.0f, 1.0f, 1.0f }) == 0xFFFFFFFFu);
  CHECK(vrPremultiplied({ 1.0f, 1.0f, 1.0f, 0.0f }) == 0u);
}


TEST_CASE(paint_rounded_rect_fades_at_its_edges) {
  VrPaintList list(100, 50);
  list.fillRoundedRect(0.0f, 0.0f, 100.0f, 50.0f, 10.0f, { 1.0f, 1.0f, 1.0f, 1.0f });

  CHECK(alphaAt(list, 50.3f, 25.2f) > 0.99f);
  CHECK(alphaAt(list, 50.3f, 1.2f) > 0.99f);
  CHECK_NEAR(alphaAt(list, 50.3f, 0.0f), 0.5f, 0.05f);
  // Outside the rounded corner
  CHECK(alphaAt(list, 1.1f, 1.2f) < 0.01f);
}


TEST_CASE(paint_shape_keeps_the_image_aspect) {
  VrImage image;
  image.width  = 2;
  image.height = 1;
  image.pixels = { 0xFFFFFFFFu, 0x00FFFFFFu };

  VrPaintList list(20, 20);
  list.drawShape(image, 10.0f, 10.0f, 20.0f, { 0.0f, 1.0f, 0.0f, 1.0f });

  // The image is twice as wide as tall: it fills the middle band
  CHECK(list.batches().size() == 1);
  CHECK(list.batches()[0].image == &image);
  CHECK(list.batches()[0].use == VrPaintImageUse::Shape);

  float left = 100.0f, right = -100.0f, top = 100.0f, bottom = -100.0f;

  for (const VrPaintVertex& vertex : list.vertices()) {
    left   = std::min(left, vertex.x);
    right  = std::max(right, vertex.x);
    top    = std::min(top, vertex.y);
    bottom = std::max(bottom, vertex.y);
    CHECK(vertex.colour == 0xFF00FF00u);
  }

  CHECK_NEAR(left, 0.0f, 1.0e-4f);
  CHECK_NEAR(right, 20.0f, 1.0e-4f);
  CHECK_NEAR(top, 5.0f, 1.0e-4f);
  CHECK_NEAR(bottom, 15.0f, 1.0e-4f);
}


TEST_CASE(paint_batches_follow_images_and_blending) {
  VrImage image;
  image.width  = 1;
  image.height = 1;
  image.pixels = { 0xFFFFFFFFu };

  VrPaintList list(10, 10);
  VrColour white = { 1.0f, 1.0f, 1.0f, 1.0f };

  list.fillRect(0.0f, 0.0f, 1.0f, 1.0f, white);
  list.fillRect(2.0f, 0.0f, 3.0f, 1.0f, white);
  list.drawShape(image, 5.0f, 5.0f, 2.0f, white);
  list.setBlend(VrPaintBlend::Add);
  list.drawShape(image, 5.0f, 5.0f, 2.0f, white);

  CHECK(list.batches().size() == 3);
  CHECK(list.batches()[0].count == 12);
  CHECK(list.batches()[2].blend == VrPaintBlend::Add);

  list.clear();
  list.fillRect(0.0f, 0.0f, 1.0f, 1.0f, white);
  CHECK(list.batches().size() == 1);
  CHECK(list.batches()[0].blend == VrPaintBlend::Over);
}


TEST_CASE(quick_wheel_lights_the_picked_cell) {
  VrPaintList list(VrQuickWheelImageLayout::ImageSize, VrQuickWheelImageLayout::ImageSize);
  VrQuickWheelView view;
  view.picked = 2;

  vrPaintQuickWheel(list, view, nullptr);

  float size   = float(VrQuickWheelImageLayout::ImageSize);
  float centre = 0.5f * size;
  float radius = 0.40f * size + 0.3f;

  // The right cell is picked, the left one is not
  float picked = alphaAt(list, centre + radius, centre + 0.2f);
  float other  = alphaAt(list, centre - radius, centre + 0.2f);
  CHECK(picked > other + 0.1f);
  CHECK_NEAR(picked, 1.0f - (1.0f - 0.62f) * (1.0f - 0.5f), 0.02f);
  CHECK_NEAR(other, 1.0f - (1.0f - 0.62f) * (1.0f - 0.14f), 0.02f);
  // Outside the wheel nothing is drawn
  CHECK(alphaAt(list, 2.3f, 2.2f) < 0.01f);
}


TEST_CASE(paint_sector_reaches_its_outer_extremes) {
  VrColour white = { 1.0f, 1.0f, 1.0f, 1.0f };

  // Around the right: from 45 to 135 degrees clockwise from the top, the
  // outer circle's rightmost point is between the corners
  VrPaintList right(100, 100);
  right.fillSector(50.0f, 50.0f, 20.0f, 40.0f, 0.785f, 2.356f, white);
  CHECK(alphaAt(right, 88.3f, 50.2f) > 0.99f);
  CHECK(alphaAt(right, 50.3f, 88.2f) < 0.01f);

  // Across the top, past 2 pi
  VrPaintList top(100, 100);
  top.fillSector(50.0f, 50.0f, 20.0f, 40.0f, 5.5f, 7.0f, white);
  CHECK(alphaAt(top, 50.3f, 12.2f) > 0.99f);
  CHECK(alphaAt(top, 50.3f, 88.2f) < 0.01f);
}
