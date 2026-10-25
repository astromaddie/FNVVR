#include <cmath>

#include "vr_quick_wheel_image.h"

namespace dxvk {

  namespace {

    using Layout = VrQuickWheelImageLayout;

    constexpr float TwoPi    = 6.2831853f;
    constexpr float CellSpan = TwoPi / float(VrQuickWheelCellCount);

    const VrColour Shade = { 0.0f, 0.0f, 0.0f, 0.62f };

    // How much of the HUD's colour each part takes
    constexpr float CellAlpha     = 0.14f;
    constexpr float PickedAlpha   = 0.50f;
    constexpr float IconAlpha     = 0.85f;
    constexpr float UnusableAlpha = 0.30f;
    constexpr float MarkAlpha     = 0.45f;

    // Size of the name in the middle, in font pixels per image pixel
    constexpr float NameScale    = 1.0f;
    constexpr float MinNameScale = 0.7f;

    std::string itemLabel(const VrQuickWheelCellView& cell) {
      return cell.count > 1 ? cell.name + " (" + std::to_string(cell.count) + ")" : cell.name;
    }

    // A cell's background, then its icon (or its mark) and its equipped dot
    void drawCell(VrCanvas& canvas, const VrQuickWheelView& view, uint32_t index, const VrColour& background,
        float alpha, const VrCanvasFont* font) {
      const VrQuickWheelCellView& cell = view.cells[index];

      float size   = float(canvas.width());
      float centre = 0.5f * size;

      float middle = vrQuickWheelCellAngle(index);
      float start  = middle - 0.5f * CellSpan + 0.5f * Layout::CellGap;
      float end    = middle + 0.5f * CellSpan - 0.5f * Layout::CellGap;

      canvas.fillSector(centre, centre, Layout::InnerRadius * size, Layout::OuterRadius * size, start, end, background);

      // The icon's middle, clockwise from the top in an image whose Y axis
      // points down
      float x = centre + std::sin(middle) * Layout::IconRadius * size;
      float y = centre - std::cos(middle) * Layout::IconRadius * size;

      if (cell.filled && cell.icon) {
        canvas.drawShape(*cell.icon, x, y, Layout::IconSize * size, view.colour.faded(alpha));
      } else if (font && !cell.mark.empty()) {
        canvas.drawTextBlock(*font, cell.mark, x, y, Layout::IconSize * size, NameScale, MinNameScale, 1,
          view.colour.faded(cell.filled ? alpha : MarkAlpha));
      }

      // A dot towards the rim for what is equipped
      if (cell.worn) {
        float rim = (Layout::OuterRadius - 0.035f) * size;
        canvas.fillSector(centre + std::sin(middle) * rim, centre - std::cos(middle) * rim,
          0.0f, 0.012f * size, 0.0f, TwoPi, view.colour);
      }
    }

  }


  void vrDrawQuickWheelRing(VrCanvas& canvas, const VrQuickWheelView& view, const VrCanvasFont* font) {
    canvas.clear();

    float size   = float(canvas.width());
    float centre = 0.5f * size;

    canvas.fillSector(centre, centre, 0.0f, Layout::OuterRadius * size, 0.0f, TwoPi, Shade);

    for (uint32_t i = 0; i < VrQuickWheelCellCount; i++) {
      const VrQuickWheelCellView& cell = view.cells[i];
      float alpha = cell.usable ? IconAlpha : UnusableAlpha;

      drawCell(canvas, view, i, view.colour.faded(CellAlpha), alpha, font);
    }
  }


  void vrDrawQuickWheelPick(VrCanvas& canvas, const VrQuickWheelView& view, const VrCanvasFont* font) {
    if (view.picked < 0 || view.picked >= int32_t(VrQuickWheelCellCount))
      return;

    const VrQuickWheelCellView& cell = view.cells[view.picked];

    // Over the unlit cell, this adds up to the lit cell's alpha
    float lit = cell.usable ? 1.0f - (1.0f - PickedAlpha) / (1.0f - CellAlpha) : 0.0f;
    drawCell(canvas, view, uint32_t(view.picked), view.colour.faded(lit), 1.0f, font);

    if (!font || !cell.filled)
      return;

    float size  = float(canvas.width());
    float inner = Layout::InnerRadius * size;

    canvas.drawTextBlock(*font, itemLabel(cell), 0.5f * size, 0.5f * size, 1.7f * inner,
      NameScale, MinNameScale, 3, view.colour);
  }

}
