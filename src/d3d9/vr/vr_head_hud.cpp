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

    // Space kept between the messages or objectives and the meters, in
    // interface units
    constexpr float HudCornerGap = 10.0f;

  }


  VrInterfaceRect vrComputeRadiationMeterCrop(
    const VrInterfaceRect&      group,
    const VrVector2&            size) {
    return {
      std::clamp(group.left,                          0.0f, size.x),
      std::clamp(group.top,                           0.0f, size.y),
      size.x,
      std::clamp(group.top + VrRadiationMeterHeight,  0.0f, size.y) };
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


  bool vrComputeHudMetersCrop(
    const VrInterfaceRect*      radiation,
    const VrInterfaceRect*      xp,
    const VrVector2&            size,
          VrInterfaceRect&      crop) {
    if (radiation && xp) {
      VrInterfaceRect a = vrComputeRadiationMeterCrop(*radiation, size);
      VrInterfaceRect b = vrComputeXpMeterCrop(*xp, size);

      crop = {
        std::min(a.left,   b.left),
        std::min(a.top,    b.top),
        std::max(a.right,  b.right),
        std::max(a.bottom, b.bottom) };
    } else if (radiation) {
      crop = vrComputeRadiationMeterCrop(*radiation, size);
    } else if (xp) {
      crop = vrComputeXpMeterCrop(*xp, size);
    } else {
      return false;
    }

    return true;
  }


  float vrComputeHudCornerRight(
    const VrInterfaceRect*      messages,
    const VrInterfaceRect*      objectives) {
    if (!messages && !objectives)
      return 0.0f;

    float right = 0.0f;

    if (messages)
      right = std::max(right, messages->right);

    if (objectives)
      right = std::max(right, objectives->right);

    return right + HudCornerGap;
  }


  VrHeadHudLayout vrComputeHeadHudLayout(
    const VrVector2&            size,
          float                 shift,
    const VrInterfaceRect*      meters,
          float                 cornerRight) {
    VrHeadHudLayout layout;

    float split = std::floor(0.5f * size.y);
    shift = std::clamp(shift, 0.0f, std::floor(0.5f * size.x));

    layout.copies[layout.count++] = {
      { 0.0f, split, size.x, size.y },
      { 0.0f, split, size.x, size.y } };

    // The upper half without the meters' columns, so that the meters are
    // not copied twice
    float upperRight = size.x - shift;

    if (meters)
      upperRight = std::min(upperRight, meters->left);

    layout.copies[layout.count++] = {
      { 0.0f,  0.0f, upperRight,         split },
      { shift, 0.0f, upperRight + shift, split } };

    if (meters) {
      VrInterfaceRect source = {
        meters->left, std::max(meters->top, 0.0f),
        std::min(meters->right, size.x), std::min(meters->bottom, split) };

      // The meters' copy replaces what lies under it, so it must not reach
      // the messages and objectives moved the other way
      float metersShift = std::clamp(meters->left - (cornerRight + shift), 0.0f, shift);

      if (!source.isEmpty()) {
        layout.copies[layout.count++] = { source,
          { source.left - metersShift, source.top, source.right - metersShift, source.bottom } };
      }
    }

    return layout;
  }

}
