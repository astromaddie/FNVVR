#include <algorithm>
#include <cmath>
#include <cstring>

#include "vr_terminal_image.h"

namespace dxvk {

  namespace {

    // Texels darker than this, joined to the plate's edge, are the empty
    // texture around the plate's pieces and are left transparent
    constexpr uint32_t BackgroundLuminance = 24;

    // Dark ink of the keys' names, and how much of it covers a key
    constexpr uint32_t InkColour  = 0x2E2B22u;
    constexpr float    InkOpacity = 0.9f;

    // Size of a key's name: one character in font pixels per image
    // pixel, longer names shrunk to fit the key
    constexpr float CharScale      = 1.2f;
    constexpr float NameScale      = 0.72f;
    constexpr float MinScale       = 0.44f;
    constexpr float NameWidthShare = 0.85f;

    // A lit key is brightened, a pressed key darkened and its name moved down
    constexpr float HoverGain    = 1.3f;
    constexpr float HoverOffset  = 24.0f;
    constexpr float PressedGain  = 0.7f;
    constexpr float PressedShift = 1.6f;

    // The orange light while Shift or Caps Lock is on: its core, then a
    // glow fading out to GlowExtent times its radius
    constexpr float LightRed    = 255.0f;
    constexpr float LightGreen  = 168.0f;
    constexpr float LightBlue   = 64.0f;
    constexpr float GlowExtent  = 3.0f;
    constexpr float GlowOpacity = 0.6f;

    uint32_t channel(uint32_t pixel, uint32_t shift) {
      return (pixel >> shift) & 0xFF;
    }

    uint32_t pack(float a, float r, float g, float b) {
      auto clamp = [] (float v) { return uint32_t(std::clamp(v + 0.5f, 0.0f, 255.0f)); };
      return clamp(a) << 24 | clamp(r) << 16 | clamp(g) << 8 | clamp(b);
    }

    uint32_t luminance(uint32_t pixel) {
      return (channel(pixel, 16) * 3 + channel(pixel, 8) * 6 + channel(pixel, 0)) / 10;
    }

  }


  bool VrTerminalImage::initialize(const VrImage& texture, const VrBitmapFont& font, const VrImage& glyphs) {
    m_valid = false;

    if (texture.width < uint32_t(VrTerminalKeyboard::Plate.right) + 1
     || texture.height < uint32_t(VrTerminalKeyboard::Plate.bottom) + 1
     || !glyphs.width || !glyphs.height)
      return false;

    m_font   = font;
    m_glyphs = glyphs;
    buildPlate(texture);

    m_pixels.assign(size_t(Width) * Height, 0u);
    return true;
  }


  bool VrTerminalImage::update(const VrTerminalKeyboardView& view) {
    if (m_plate.empty() || (m_valid && view == m_drawn))
      return false;

    std::memcpy(m_pixels.data(), m_plate.data(), m_pixels.size() * sizeof(uint32_t));

    uint32_t count = 0;
    const VrTerminalKey* keys = VrTerminalKeyboard::keys(count);

    for (uint32_t i = 0; i < count; i++) {
      drawKey(keys[i], int32_t(i) == view.hoveredKey, int32_t(i) == view.pressedKey,
        VrTerminalKeyboard::isShifted(keys[i], view.shift, view.capsLock));
    }

    if (view.shift || view.capsLock)
      drawLight();

    m_drawn = view;
    m_valid = true;
    return true;
  }


  void VrTerminalImage::buildPlate(const VrImage& texture) {
    const VrTexelRect& plate = VrTerminalKeyboard::Plate;

    uint32_t left = uint32_t(plate.left);
    uint32_t top  = uint32_t(plate.top);
    uint32_t w    = uint32_t(plate.width());
    uint32_t h    = uint32_t(plate.height());

    // The plate's texels, with the empty texture around its pieces made
    // transparent: a flood fill from the edge through dark texels
    VrImage texels;
    texels.width  = w;
    texels.height = h;
    texels.pixels.resize(size_t(w) * h);

    const VrTexelRect& other = VrTerminalKeyboard::OtherPiece;

    for (uint32_t y = 0; y < h; y++) {
      for (uint32_t x = 0; x < w; x++) {
        float tx = float(left + x);
        float ty = float(top + y);

        bool outside = tx >= other.left && tx < other.right && ty >= other.top && ty < other.bottom;
        texels.pixels[size_t(y) * w + x] = outside ? 0u : texture.at(left + x, top + y) | 0xFF000000u;
      }
    }

    std::vector<uint32_t> stack;

    auto push = [&] (uint32_t x, uint32_t y) {
      uint32_t& texel = texels.pixels[size_t(y) * w + x];

      if ((texel >> 24) && luminance(texel) < BackgroundLuminance) {
        texel = 0u;
        stack.push_back(y * w + x);
      }
    };

    for (uint32_t x = 0; x < w; x++) {
      push(x, 0);
      push(x, h - 1);
    }

    for (uint32_t y = 0; y < h; y++) {
      push(0, y);
      push(w - 1, y);
    }

    while (!stack.empty()) {
      uint32_t index = stack.back();
      stack.pop_back();

      uint32_t x = index % w;
      uint32_t y = index / w;

      if (x > 0)     push(x - 1, y);
      if (x + 1 < w) push(x + 1, y);
      if (y > 0)     push(x, y - 1);
      if (y + 1 < h) push(x, y + 1);
    }

    // Enlarged with bilinear filtering of the premultiplied colours, so
    // that the transparent texels do not darken the edges
    VrImage premultiplied = texels;

    for (uint32_t& texel : premultiplied.pixels) {
      float a = float(texel >> 24) / 255.0f;
      texel = pack(float(texel >> 24), float(channel(texel, 16)) * a,
        float(channel(texel, 8)) * a, float(channel(texel, 0)) * a);
    }

    m_plate.resize(size_t(Width) * Height);

    for (uint32_t y = 0; y < Height; y++) {
      for (uint32_t x = 0; x < Width; x++) {
        float tx = (float(x) + 0.5f) / float(Scale) - 0.5f;
        float ty = (float(y) + 0.5f) / float(Scale) - 0.5f;

        float a = vrSampleChannel(premultiplied, tx, ty, 24);
        float k = a > 0.0f ? 255.0f / a : 0.0f;

        m_plate[size_t(y) * Width + x] = pack(a,
          vrSampleChannel(premultiplied, tx, ty, 16) * k,
          vrSampleChannel(premultiplied, tx, ty, 8)  * k,
          vrSampleChannel(premultiplied, tx, ty, 0)  * k);
      }
    }
  }


  void VrTerminalImage::drawKey(const VrTerminalKey& key, bool hovered, bool pressed, bool shifted) {
    const VrTexelRect& plate = VrTerminalKeyboard::Plate;

    float left   = (key.rect.left   - plate.left) * float(Scale);
    float right  = (key.rect.right  - plate.left) * float(Scale);
    float top    = (key.rect.top    - plate.top)  * float(Scale);
    float bottom = (key.rect.bottom - plate.top)  * float(Scale);

    if (hovered || pressed) {
      float gain   = pressed ? PressedGain : HoverGain;
      float offset = pressed ? 0.0f : HoverOffset;

      uint32_t x0 = uint32_t(std::max(left, 0.0f));
      uint32_t y0 = uint32_t(std::max(top, 0.0f));
      uint32_t x1 = std::min(uint32_t(right), Width);
      uint32_t y1 = std::min(uint32_t(bottom), Height);

      for (uint32_t y = y0; y < y1; y++) {
        for (uint32_t x = x0; x < x1; x++) {
          uint32_t& pixel = m_pixels[size_t(y) * Width + x];
          pixel = pack(float(pixel >> 24),
            float(channel(pixel, 16)) * gain + offset,
            float(channel(pixel, 8))  * gain + offset,
            float(channel(pixel, 0))  * gain + offset);
        }
      }
    }

    char text[2] = { shifted ? key.upper : key.lower, '\0' };
    const char* label = key.label ? key.label : text;

    float scale = label[0] && label[1] ? NameScale : CharScale;
    float shift = pressed ? PressedShift : 0.0f;

    drawLabel(label, 0.5f * (left + right), 0.5f * (top + bottom) + shift,
      (right - left) * NameWidthShare, scale);
  }


  void VrTerminalImage::drawLabel(const char* text, float centreX, float centreY, float maxWidth, float scale) {
    float width = 0.0f;

    for (const char* c = text; *c; c++)
      width += m_font.glyph(*c).advance();

    if (width <= 0.0f)
      return;

    scale = std::max(std::min(scale, maxWidth / width), MinScale);

    // Centred on the height of the capitals
    float capitals = m_font.glyph('A').ascent;
    float baseline = centreY + 0.5f * capitals * scale;
    float x        = centreX - 0.5f * width * scale;

    for (const char* c = text; *c; c++) {
      const VrGlyph& glyph = m_font.glyph(*c);
      drawGlyph(glyph, x, baseline, scale);
      x += glyph.advance() * scale;
    }
  }


  void VrTerminalImage::drawGlyph(const VrGlyph& glyph, float x, float baseline, float scale) {
    if (glyph.width <= 0.0f || glyph.height <= 0.0f)
      return;

    float left = x + glyph.before * scale;
    float top  = baseline - glyph.ascent * scale;
    float w    = glyph.width  * scale;
    float h    = glyph.height * scale;

    int32_t x0 = std::max(int32_t(std::floor(left)), 0);
    int32_t y0 = std::max(int32_t(std::floor(top)), 0);
    int32_t x1 = std::min(int32_t(std::ceil(left + w)), int32_t(Width));
    int32_t y1 = std::min(int32_t(std::ceil(top + h)), int32_t(Height));

    float atlasWidth  = float(m_glyphs.width);
    float atlasHeight = float(m_glyphs.height);

    for (int32_t y = y0; y < y1; y++) {
      for (int32_t px = x0; px < x1; px++) {
        // Position in the glyph, from 0 to 1, then in the font's texture
        float gx = (float(px) + 0.5f - left) / w;
        float gy = (float(y)  + 0.5f - top)  / h;

        if (gx < 0.0f || gx > 1.0f || gy < 0.0f || gy > 1.0f)
          continue;

        float u = (glyph.u0 + gx * (glyph.u1 - glyph.u0)) * atlasWidth  - 0.5f;
        float v = (glyph.v0 + gy * (glyph.v1 - glyph.v0)) * atlasHeight - 0.5f;

        float coverage = vrSampleChannel(m_glyphs, u, v, 24) / 255.0f * InkOpacity;

        if (coverage <= 0.0f)
          continue;

        uint32_t& pixel = m_pixels[size_t(y) * Width + px];

        auto blend = [&] (uint32_t shift) {
          return float(channel(pixel, shift)) * (1.0f - coverage) + float(channel(InkColour, shift)) * coverage;
        };

        pixel = pack(float(pixel >> 24), blend(16), blend(8), blend(0));
      }
    }
  }


  void VrTerminalImage::drawLight() {
    const VrTexelRect& plate = VrTerminalKeyboard::Plate;

    float cx     = (VrTerminalKeyboard::LightX - plate.left) * float(Scale);
    float cy     = (VrTerminalKeyboard::LightY - plate.top)  * float(Scale);
    float radius = VrTerminalKeyboard::LightRadius * float(Scale);
    float extent = radius * GlowExtent;

    int32_t x0 = std::max(int32_t(cx - extent), 0);
    int32_t y0 = std::max(int32_t(cy - extent), 0);
    int32_t x1 = std::min(int32_t(cx + extent) + 1, int32_t(Width));
    int32_t y1 = std::min(int32_t(cy + extent) + 1, int32_t(Height));

    for (int32_t y = y0; y < y1; y++) {
      for (int32_t x = x0; x < x1; x++) {
        float d = std::hypot(float(x) + 0.5f - cx, float(y) + 0.5f - cy) / radius;

        if (d >= GlowExtent)
          continue;

        // Opaque inside the light, then fading out
        float weight = d < 1.0f ? 1.0f : GlowOpacity * (GlowExtent - d) / (GlowExtent - 1.0f);

        uint32_t& pixel = m_pixels[size_t(y) * Width + x];

        auto lit = [&] (uint32_t shift, float light) {
          return float(channel(pixel, shift)) * (1.0f - weight) + light * weight;
        };

        pixel = pack(std::max(float(pixel >> 24), weight * 255.0f),
          lit(16, LightRed), lit(8, LightGreen), lit(0, LightBlue));
      }
    }
  }

}
