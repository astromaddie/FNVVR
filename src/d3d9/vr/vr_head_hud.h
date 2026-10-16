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
   * \brief Rearranges the HUD image for the panel in front of the head
   *
   * The messages and objectives are in the top left corner and the
   * subtitles at the bottom, in the centre. Seen from the head, a corner
   * is too far to the side to read comfortably, so the upper half of the
   * image moves towards the centre by \c shift and the lower half stays.
   * The XP meter, moved by the VR layer to the top right corner (see
   * VrXpMeter), moves towards the centre by the same distance, from the
   * other side.
   *
   * \param [in] size Size of the image, in pixels
   * \param [in] shift How far the corners move towards the centre, in pixels
   * \param [in] xpMeter Crop of the XP meter in pixels, or \c nullptr if it
   *    was not moved to the top right corner
   */
  VrHeadHudLayout vrComputeHeadHudLayout(
    const VrVector2&            size,
          float                 shift,
    const VrInterfaceRect*      xpMeter);

}
