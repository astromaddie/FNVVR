#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace dxvk {

  /**
   * \brief One character of a game font
   *
   * Texture coordinates of the glyph's top left and bottom right corners
   * in the font's texture, and its metrics in the font's pixels.
   */
  struct VrGlyph {
    float u0      = 0.0f;
    float v0      = 0.0f;
    float u1      = 0.0f;
    float v1      = 0.0f;
    float width   = 0.0f;
    float height  = 0.0f;
    /// Space left of the glyph
    float before  = 0.0f;
    /// Space right of the glyph
    float after   = 0.0f;
    /// Height of the glyph above the baseline
    float ascent  = 0.0f;

    float advance() const {
      return before + width + after;
    }
  };

  /**
   * \brief Metrics of a game font (.fnt)
   *
   * The file holds the line height, the font's textures and one record
   * per character code, see findings/console.md §7. Only fonts drawn
   * from a single texture are read.
   */
  class VrBitmapFont {

  public:

    /// Characters of a font, one per byte value
    constexpr static uint32_t GlyphCount = 256;

    /**
     * \brief Reads a font file
     *
     * \param [in] data The file's content
     * \param [in] size Size of the content in bytes
     * \returns \c false if the content is not a font of a single texture
     */
    bool parse(const uint8_t* data, size_t size);

    float lineHeight() const {
      return m_lineHeight;
    }

    /// The character's glyph, empty for a character the font lacks
    const VrGlyph& glyph(char c) const {
      return m_glyphs[uint8_t(c)];
    }

  private:

    float m_lineHeight = 0.0f;
    std::array<VrGlyph, GlyphCount> m_glyphs = { };

  };

}
