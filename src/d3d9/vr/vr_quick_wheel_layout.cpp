#include <cmath>

#include "vr_quick_wheel_layout.h"

namespace dxvk {

  namespace {

    constexpr float TwoPi    = 6.2831853f;
    constexpr float CellSpan = TwoPi / float(VrQuickWheelCellCount);

    // Hotkey slot of each cell, clockwise from the top
    constexpr uint32_t CellSlots[VrQuickWheelCellCount] = { 1, 2, 4, 7, 6, 5, 3, 0 };

  }


  uint32_t vrQuickWheelSlot(uint32_t cell) {
    return CellSlots[cell % VrQuickWheelCellCount];
  }


  uint32_t vrQuickWheelCell(uint32_t slot) {
    for (uint32_t cell = 0; cell < VrQuickWheelCellCount; cell++) {
      if (CellSlots[cell] == slot)
        return cell;
    }

    return 0;
  }


  float vrQuickWheelCellAngle(uint32_t cell) {
    return float(cell % VrQuickWheelCellCount) * CellSpan;
  }


  int32_t vrPickQuickWheelCell(const VrVector2& offset, int32_t current, const VrQuickWheelPicking& picking) {
    float distance = std::sqrt(offset.x * offset.x + offset.y * offset.y);

    if (distance < (current >= 0 ? picking.keepZone : picking.deadZone))
      return -1;

    // Clockwise from the top
    float angle = std::atan2(offset.x, offset.y);

    if (angle < 0.0f)
      angle += TwoPi;

    if (current >= 0) {
      float away = std::fabs(angle - vrQuickWheelCellAngle(uint32_t(current)));
      away = std::fmin(away, TwoPi - away);

      if (away <= 0.5f * CellSpan + picking.hysteresis)
        return current;
    }

    return int32_t(std::lround(angle / CellSpan)) % int32_t(VrQuickWheelCellCount);
  }

}
