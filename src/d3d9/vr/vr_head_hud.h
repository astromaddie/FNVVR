#pragma once

#include <array>
#include <cstdint>

#include "vr_hand_hud.h"

namespace dxvk {

  /**
   * \brief Part of the HUD image copied into the head panel's image
   */
  struct VrHudCopy {
    VrInterfaceRect source;
    VrInterfaceRect target;
  };

  /**
   * \brief How the HUD image is rearranged into the head panel's image
   */
  struct VrHeadHudLayout {
    std::array<VrHudCopy, 3> copies;
    uint32_t                 count = 0;
  };

  /**
   * \brief Height of the radiation meter, in interface units
   *
   * Height of its bracket image (radiation_bracket in hud_main_menu.xml),
   * which holds the bar, its pointer and the rads per second. The XP meter
   * is placed this far below it. See findings/hud.md §12.
   */
  constexpr float VrRadiationMeterHeight = 120.0f;

  /**
   * \brief Crop of the radiation meter in the HUD image
   *
   * The bracket image is wider than the group and reaches past the right
   * side of the screen, so the crop does too.
   *
   * \param [in] group Rectangle of the RadiationMeter group, in interface units
   * \param [in] size Size of the interface, in interface units
   */
  VrInterfaceRect vrComputeRadiationMeterCrop(
    const VrInterfaceRect&      group,
    const VrVector2&            size);

  /**
   * \brief Crop of the XP meter in the HUD image
   *
   * The XPMeter group has no size of its own. Its bracket image, 256 units
   * high, starts at the group's position, and its texts, the "LEVEL UP"
   * line included, lie inside it. The bracket is wider than the space the
   * game leaves on the right, so the crop reaches the right side of the
   * screen. See findings/hud.md §11.
   *
   * \param [in] group Rectangle of the XPMeter group, in interface units
   * \param [in] size Size of the interface, in interface units
   */
  VrInterfaceRect vrComputeXpMeterCrop(
    const VrInterfaceRect&      group,
    const VrVector2&            size);

  /**
   * \brief Crop of the column of meters in the top right corner
   *
   * Covers the crops of the meters placed there.
   *
   * \param [in] radiation Rectangle of the RadiationMeter group in
   *    interface units, or \c nullptr if it was not placed there
   * \param [in] xp Rectangle of the XPMeter group in interface units, or
   *    \c nullptr if it was not placed there
   * \param [in] size Size of the interface, in interface units
   * \param [out] crop The column, in interface units
   * \returns \c false if neither meter was placed there
   */
  bool vrComputeHudMetersCrop(
    const VrInterfaceRect*      radiation,
    const VrInterfaceRect*      xp,
    const VrVector2&            size,
          VrInterfaceRect&      crop);

  /**
   * \brief Rearranges the HUD image for the panel in front of the head
   *
   * The messages and objectives are in the top left corner and the
   * subtitles at the bottom, in the centre. Seen from the head, a corner
   * is too far to the side to read comfortably, so the upper half of the
   * image moves towards the centre by \c shift and the lower half stays.
   * The radiation and XP meters, placed by the VR layer in a column in the
   * top right corner (see VrHudMeters), move towards the centre by the same
   * distance, from the other side.
   *
   * \param [in] size Size of the image, in pixels
   * \param [in] shift How far the corners move towards the centre, in pixels
   * \param [in] meters Crop of the meters' column in pixels, or \c nullptr
   *    if neither was placed in the top right corner
   */
  VrHeadHudLayout vrComputeHeadHudLayout(
    const VrVector2&            size,
          float                 shift,
    const VrInterfaceRect*      meters);

}
