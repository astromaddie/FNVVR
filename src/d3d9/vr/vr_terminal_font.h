#pragma once

#include <cstdint>

namespace dxvk {

  /**
   * \brief Bitmap font of the console terminal
   *
   * The printable ASCII characters of the X11 "misc-fixed" 10x20 font
   * (-Misc-Fixed-Medium-R-Normal--20-200-75-75-C-100-ISO8859-1), which is
   * in the public domain ("Public domain font. Share and enjoy."). Each
   * glyph is a cell of GlyphWidth by GlyphHeight pixels, one row per
   * 16-bit value from the top, the cell's leftmost pixel in bit
   * GlyphWidth - 1.
   */
  struct VrTerminalFont {
    constexpr static uint32_t GlyphWidth  = 10;
    constexpr static uint32_t GlyphHeight = 20;
    /// Characters with a glyph, from ' ' to '~'
    constexpr static char     First = ' ';
    constexpr static char     Last  = '~';
  };

  /**
   * \brief Rows of a character's glyph
   *
   * \param [in] c Character, a character without a glyph gives '?'
   * \returns GlyphHeight rows
   */
  const uint16_t* vrTerminalGlyph(char c);

}
