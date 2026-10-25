#pragma once

#include <cstdint>

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
   * Drawn by the GPU, see VrPainter.
   */
  class VrTerminalTexture {

  public:

    /// Pixels per texel of the terminal texture, the keyboard image's
    constexpr static uint32_t Scale = VrTerminalImage::Scale;

    /// The key tops on the terminal texture, in whole texels: the part of
    /// the copy taken from the keyboard image (findings/console.md §11)
    constexpr static VrTexelRect KeyTops = { 46.0f, 295.0f, 249.0f, 379.0f };

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
     * \brief Draws the copy
     *
     * The enlarged texture, then the keyboard image laid over its key tops
     * where it is opaque; the texture's alpha is kept.
     *
     * \param [out] list Paint list of width() by height() pixels
     * \param [in] keyboard The keyboard image
     * \param [in] view What the keyboard shows
     */
    void paint(VrPaintList& list, const VrTerminalImage& keyboard, const VrTerminalKeyboardView& view) const;

  private:

    VrImage m_texture;

  };

}
