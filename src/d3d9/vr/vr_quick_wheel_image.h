#pragma once

#include <array>
#include <cstdint>
#include <string>

#include "vr_canvas.h"
#include "vr_quick_wheel_layout.h"

namespace dxvk {

  /**
   * \brief What a cell of the quick wheel shows
   */
  struct VrQuickWheelCellView {
    /// An item is in the cell
    bool           filled = false;
    /// The item is equipped
    bool           worn   = false;
    /// The cell can be chosen
    bool           usable = true;
    /// The item's name and count, shown in the middle while the cell is
    /// picked
    std::string    name;
    uint32_t       count  = 0;
    /// The item's icon, or \c nullptr
    const VrImage* icon   = nullptr;
    /// Shown instead of an icon, such as the slot's key
    std::string    mark;
  };

  /**
   * \brief What the quick wheel shows
   */
  struct VrQuickWheelView {
    /// Cells clockwise from the top
    std::array<VrQuickWheelCellView, VrQuickWheelCellCount> cells;
    /// The picked cell, or -1
    int32_t        picked = -1;
    /// The HUD's colour
    VrColour       colour = { 1.0f, 0.71f, 0.26f, 1.0f };
  };

  /**
   * \brief Layout of the quick wheel's image, as fractions of its side
   */
  struct VrQuickWheelImageLayout {
    constexpr static uint32_t ImageSize   = 512;
    /// Width of the panel, in metres
    constexpr static float    Width       = 0.30f;
    constexpr static float    OuterRadius = 0.48f;
    constexpr static float    InnerRadius = 0.21f;
    constexpr static float    IconRadius  = 0.345f;
    constexpr static float    IconSize    = 0.15f;
    /// Gap between two cells, in radians
    constexpr static float    CellGap     = 0.035f;
  };

  /**
   * \brief Draws the quick wheel with no cell lit
   *
   * A dark ring of cells, each item's icon in the HUD's colour (dimmed
   * when it cannot be chosen, with a dot when it is equipped). The picked
   * cell is ignored, see vrDrawQuickWheelPick.
   *
   * \param [out] canvas Canvas of VrQuickWheelImageLayout::ImageSize pixels
   * \param [in] view What the wheel shows
   * \param [in] font Font of the slots' keys, or \c nullptr to draw no text
   */
  void vrDrawQuickWheelRing(VrCanvas& canvas, const VrQuickWheelView& view, const VrCanvasFont* font);

  /**
   * \brief Lights the picked cell over the ring
   *
   * Drawing the whole wheel takes too long for a frame, so a change of the
   * picked cell only draws that cell, lit, and the picked item's name in
   * the middle over a copy of the ring.
   *
   * \param [in,out] canvas The ring, drawn by vrDrawQuickWheelRing with the
   *    same view
   * \param [in] view What the wheel shows; nothing is drawn if no cell is
   *    picked
   * \param [in] font Font of the names, or \c nullptr to draw no text
   */
  void vrDrawQuickWheelPick(VrCanvas& canvas, const VrQuickWheelView& view, const VrCanvasFont* font);

}
