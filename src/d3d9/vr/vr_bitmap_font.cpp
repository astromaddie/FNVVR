#include <cstring>

#include "vr_bitmap_font.h"

namespace dxvk {

  namespace {

    // Line height, then the number of textures, then one record per
    // texture (an index and the texture's name), then the glyphs
    constexpr size_t TextureCountOffset = 4;
    constexpr size_t TexturesOffset     = 8;
    constexpr size_t TextureRecordSize  = 0x120;

    // Texture index, the u and v of the top left, top right, bottom left
    // and bottom right corners, width, height, space before and after,
    // height above the baseline
    constexpr size_t GlyphFloats = 14;
    constexpr size_t GlyphSize   = GlyphFloats * sizeof(float);

    float readFloat(const uint8_t* data) {
      float value;
      std::memcpy(&value, data, sizeof(value));
      return value;
    }

  }


  bool VrBitmapFont::parse(const uint8_t* data, size_t size) {
    *this = VrBitmapFont();

    if (!data || size < TexturesOffset)
      return false;

    uint32_t textures;
    std::memcpy(&textures, data + TextureCountOffset, sizeof(textures));

    size_t glyphsOffset = TexturesOffset + TextureRecordSize;

    if (textures != 1 || size != glyphsOffset + GlyphCount * GlyphSize)
      return false;

    m_lineHeight = readFloat(data);

    for (uint32_t i = 0; i < GlyphCount; i++) {
      float values[GlyphFloats];
      std::memcpy(values, data + glyphsOffset + i * GlyphSize, sizeof(values));

      VrGlyph& glyph = m_glyphs[i];
      glyph.u0     = values[1];
      glyph.v0     = values[2];
      glyph.u1     = values[7];
      glyph.v1     = values[8];
      glyph.width  = values[9];
      glyph.height = values[10];
      glyph.before = values[11];
      glyph.after  = values[12];
      glyph.ascent = values[13];
    }

    return m_lineHeight > 0.0f;
  }

}
