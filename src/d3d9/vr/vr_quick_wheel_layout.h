#pragma once

#include <cstdint>

#include "vr_types.h"

namespace dxvk {

  /// Cells of the quick wheel, clockwise from the top
  constexpr uint32_t VrQuickWheelCellCount = 8;

  /**
   * \brief The game's hotkey slot of a cell of the quick wheel
   *
   * The cells follow the gamepad's D-pad, see findings/quick-menu.md §2:
   * up is the ammunition swap (slot 1), up and right slot 2, right 4,
   * down and right 7, down 6, down and left 5, left 3, up and left 0.
   *
   * \param [in] cell Cell, clockwise from the top
   */
  uint32_t vrQuickWheelSlot(uint32_t cell);

  /**
   * \brief The cell of a hotkey slot, the inverse of vrQuickWheelSlot
   */
  uint32_t vrQuickWheelCell(uint32_t slot);

  /**
   * \brief Angle of a cell's middle, in radians clockwise from the top
   */
  float vrQuickWheelCellAngle(uint32_t cell);

  /**
   * \brief How the hand picks a cell of the quick wheel
   *
   * Distances in metres in the wheel's plane, from where the hand was when
   * the wheel opened.
   */
  struct VrQuickWheelPicking {
    /// The hand nearer the middle than this picks nothing
    float deadZone   = 0.035f;
    /// A picked cell is let go of only nearer the middle than this
    float keepZone   = 0.025f;
    /// A picked cell stays picked this far past its edges, in radians
    float hysteresis = 0.09f;
  };

  /**
   * \brief Which cell the hand picks
   *
   * \param [in] offset Hand offset from the middle of the wheel, X right
   *    and Y up in the wheel's plane, in metres
   * \param [in] current Cell picked so far, or -1
   * \param [in] picking How cells are picked
   * \returns The picked cell, or -1 in the middle
   */
  int32_t vrPickQuickWheelCell(const VrVector2& offset, int32_t current, const VrQuickWheelPicking& picking);

}
