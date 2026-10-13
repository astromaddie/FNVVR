#pragma once

#include <cstdint>
#include <unordered_set>

namespace dxvk {

  /**
   * \brief Requests the texture of visible menu images that have none
   *
   * The game loads the texture of an image tile when the tile's texture
   * update runs while the tile is visible or promoted. If no texture is
   * found while the image is hidden, the request is dropped, and showing
   * the image later only updates its culling (Tile::UpdateTile). The image
   * is then drawn without a texture, so the tile shader samples the last
   * texture bound, a font page: the gamepad button icons of the Pip-Boy
   * showed glyph fragments this way: their menus are built at the main
   * menu, before the VR controllers act as a gamepad, while the icons are
   * hidden. Each time such an image is visible,
   * its texture update is requested once more. Only images cut from a
   * texture atlas are concerned: other images, such as the map, may have
   * their texture set and cleared by the game's code.
   */
  class VrTileTextureRetry {

  public:

    bool initialize();

    /**
     * \brief Requests the missing textures of the visible atlas images
     *
     * Called on the game's main thread, outside the interface passes, so
     * the culling seen is the one the game sets.
     */
    void update();

  private:

    bool     m_available = false;
    uint32_t m_logged    = 0;

    // Images without a texture whose update was requested again while
    // they stayed visible; they are not requested again until hidden
    std::unordered_set<uintptr_t> m_requested;
    std::unordered_set<uintptr_t> m_stillMissing;

    void visit(uintptr_t tile, uint32_t depth);

  };

}
