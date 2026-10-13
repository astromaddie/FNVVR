#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

namespace dxvk {

  /**
   * \brief Diagnostic: logs the state of the gamepad button icons in menus
   *
   * The button icons of the Pip-Boy are image tiles that take a part of a
   * texture atlas. When such a tile has no texture, the tile shader binds
   * none and the icon is drawn with the previous texture, a font page, at
   * the atlas coordinates. This probe walks the menu tiles and logs, for
   * every icon, the tile's update flags, its visibility and its texture,
   * each time they change, to find out why the texture is missing.
   *
   * Temporary: removed once the cause is known.
   */
  class VrTileProbe {

  public:

    bool initialize();

    /**
     * \brief Logs the icons whose state changed since the last sample
     * \param [in] phase Label of the point in the frame
     */
    void sample(const char* phase);

  private:

    bool m_available = false;
    bool m_loggedStart = false;
    uint32_t m_lines = 0;

    std::unordered_map<uintptr_t, std::string> m_states;

    void walk(uintptr_t tile, uint32_t menuId, const char* phase, uint32_t depth);

    void probeIcon(uintptr_t tile, uint32_t menuId, const char* filename, const char* phase);

  };

}
