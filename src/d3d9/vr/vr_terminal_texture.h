#pragma once

#include <cstdint>
#include <vector>

#include "vr_terminal_image.h"

namespace dxvk {

  /**
   * \brief Texture of the console's 3D keyboard
   *
   * The world terminal's keyboard is drawn with the terminal texture
   * (terminal01.dds), shared with every terminal of the world. While it
   * draws the console's keyboard, the game is given a copy of that texture
   * instead, enlarged so that the keys' names stay sharp up close, whose
   * key tops are those of the keyboard image (VrTerminalImage): names,
   * lit and pressed keys, the Shift light. The rest of the copy is the
   * enlarged texture, which the bracket and the keyboard's sides use.
   */
  class VrTerminalTexture {

  public:

    /// Pixels per texel of the terminal texture, the keyboard image's
    constexpr static uint32_t Scale = VrTerminalImage::Scale;

    /// The key tops on the terminal texture, in whole texels: the part of
    /// the copy taken from the keyboard image (findings/console.md §11)
    constexpr static uint32_t KeyTopsLeft   = 46;
    constexpr static uint32_t KeyTopsTop    = 295;
    constexpr static uint32_t KeyTopsRight  = 249;
    constexpr static uint32_t KeyTopsBottom = 379;

    /// The key tops on the copy, in pixels
    constexpr static uint32_t KeyTopsX      = KeyTopsLeft * Scale;
    constexpr static uint32_t KeyTopsY      = KeyTopsTop  * Scale;
    constexpr static uint32_t KeyTopsWidth  = (KeyTopsRight  - KeyTopsLeft) * Scale;
    constexpr static uint32_t KeyTopsHeight = (KeyTopsBottom - KeyTopsTop)  * Scale;

    /**
     * \brief Takes the terminal texture
     *
     * \param [in] texture The terminal texture (terminal01.dds)
     * \returns \c false if the texture does not hold the keyboard plate
     */
    bool initialize(const VrImage& texture);

    uint32_t width() const {
      return m_texture.width * Scale;
    }

    uint32_t height() const {
      return m_texture.height * Scale;
    }

    /**
     * \brief Draws the whole enlarged texture, without the key tops
     *
     * \param [out] pixels width() by height() pixels, see VrImage
     */
    void drawBase(std::vector<uint32_t>& pixels) const;

    /**
     * \brief Draws the key tops of the copy
     *
     * The keyboard image is laid over the enlarged texture where it is
     * opaque; the texture's alpha is kept.
     *
     * \param [in] keyboard The keyboard image
     * \param [out] pixels KeyTopsWidth by KeyTopsHeight pixels
     */
    void drawKeyTops(const VrTerminalImage& keyboard, uint32_t* pixels) const;

  private:

    VrImage               m_texture;
    std::vector<uint32_t> m_keyTops;

    uint32_t sample(float x, float y) const;

  };

}
