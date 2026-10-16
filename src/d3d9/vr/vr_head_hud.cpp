#include <algorithm>
#include <cmath>

#include "vr_head_hud.h"

namespace dxvk {

  namespace {

    // Height of the XP meter's bracket image (glow_XP_bar_over.dds in
    // hud_main_menu.xml), in interface units
    constexpr float XpMeterHeight = 256.0f;

    // The level number left of the bar is right-aligned 70 units into the
    // group, so a long number reaches slightly past its left side
    constexpr float XpMeterMarginLeft = 12.0f;

  }


  VrInterfaceRect vrComputeXpMeterCrop(
    const VrInterfaceRect&      group,
    const VrVector2&            size) {
    return {
      std::clamp(group.left - XpMeterMarginLeft, 0.0f, size.x),
      std::clamp(group.top,                      0.0f, size.y),
      size.x,
      std::clamp(group.top + XpMeterHeight,      0.0f, size.y) };
  }


  VrHeadHudLayout vrComputeHeadHudLayout(
    const VrVector2&            size,
          float                 shift,
    const VrInterfaceRect*      xpMeter) {
    VrHeadHudLayout layout;

    float split = std::floor(0.5f * size.y);
    shift = std::clamp(shift, 0.0f, std::floor(0.5f * size.x));

    layout.copies[layout.count++] = {
      { 0.0f, split, size.x, size.y },
      { 0.0f, split, size.x, size.y } };

    // The upper half without the XP meter's columns, so that the meter is
    // not copied twice
    float upperRight = size.x - shift;

    if (xpMeter)
      upperRight = std::min(upperRight, xpMeter->left);

    layout.copies[layout.count++] = {
      { 0.0f,  0.0f, upperRight,         split },
      { shift, 0.0f, upperRight + shift, split } };

    if (xpMeter) {
      VrInterfaceRect source = {
        xpMeter->left, std::max(xpMeter->top, 0.0f),
        std::min(xpMeter->right, size.x), std::min(xpMeter->bottom, split) };

      if (!source.isEmpty()) {
        layout.copies[layout.count++] = { source,
          { source.left - shift, source.top, source.right - shift, source.bottom } };
      }
    }

    return layout;
  }

}
