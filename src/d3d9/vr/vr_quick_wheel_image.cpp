#include <cmath>

#include "vr_quick_wheel_image.h"

namespace dxvk {

  namespace {

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

  }


  void vrDrawQuickWheel(VrCanvas& canvas, const VrQuickWheelView& view, const VrCanvasFont* font) {
    using Layout = VrQuickWheelImageLayout;

    canvas.clear();

    float size   = float(canvas.width());
    float centre = 0.5f * size;
    float outer  = Layout::OuterRadius * size;
    float inner  = Layout::InnerRadius * size;

    canvas.fillSector(centre, centre, 0.0f, outer, 0.0f, TwoPi, Shade);

    for (uint32_t i = 0; i < VrQuickWheelCellCount; i++) {
      const VrQuickWheelCellView& cell = view.cells[i];
      bool picked = int32_t(i) == view.picked;

      float middle = vrQuickWheelCellAngle(i);
      float start  = middle - 0.5f * CellSpan + 0.5f * Layout::CellGap;
      float end    = middle + 0.5f * CellSpan - 0.5f * Layout::CellGap;

      canvas.fillSector(centre, centre, inner, outer, start, end,
        view.colour.faded(picked && cell.usable ? PickedAlpha : CellAlpha));

      // The icon's middle, clockwise from the top in an image whose Y axis
      // points down
      float x = centre + std::sin(middle) * Layout::IconRadius * size;
      float y = centre - std::cos(middle) * Layout::IconRadius * size;
      float alpha = cell.usable ? IconAlpha : UnusableAlpha;

      if (cell.filled && cell.icon) {
        canvas.drawShape(*cell.icon, x, y, Layout::IconSize * size, view.colour.faded(picked ? 1.0f : alpha));
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

    if (!font)
      return;

    // The picked item's name in the middle, or the title
    const VrQuickWheelCellView* picked = view.picked >= 0 && view.picked < int32_t(VrQuickWheelCellCount)
      ? &view.cells[view.picked] : nullptr;

    std::string text = picked && picked->filled ? itemLabel(*picked) : view.title;
    float width = 1.7f * inner;

    if (!picked && view.titleIcon) {
      canvas.drawShape(*view.titleIcon, centre, centre - 0.35f * inner, 0.8f * inner, view.colour);
      canvas.drawTextBlock(*font, text, centre, centre + 0.45f * inner, width, NameScale, MinNameScale, 2, view.colour);
      return;
    }

    canvas.drawTextBlock(*font, text, centre, centre, width, NameScale, MinNameScale, 3, view.colour);
  }

}
