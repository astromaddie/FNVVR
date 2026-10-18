#include <algorithm>

#include "vr_terminal_texture.h"

namespace dxvk {

  namespace {

    static_assert(VrTerminalTexture::KeyTopsLeft   >= uint32_t(VrTerminalKeyboard::Plate.left)
               && VrTerminalTexture::KeyTopsTop    >= uint32_t(VrTerminalKeyboard::Plate.top)
               && VrTerminalTexture::KeyTopsRight  <= uint32_t(VrTerminalKeyboard::Plate.right)
               && VrTerminalTexture::KeyTopsBottom <= uint32_t(VrTerminalKeyboard::Plate.bottom),
      "The key tops must lie inside the keyboard image");

    uint32_t channel(uint32_t pixel, uint32_t shift) {
      return (pixel >> shift) & 0xFF;
    }

    uint32_t pack(float a, float r, float g, float b) {
      auto clamp = [] (float v) { return uint32_t(std::clamp(v + 0.5f, 0.0f, 255.0f)); };
      return clamp(a) << 24 | clamp(r) << 16 | clamp(g) << 8 | clamp(b);
    }

  }


  bool VrTerminalTexture::initialize(const VrImage& texture) {
    if (texture.width < uint32_t(VrTerminalKeyboard::Plate.right) + 1
     || texture.height < uint32_t(VrTerminalKeyboard::Plate.bottom) + 1)
      return false;

    m_texture = texture;
    m_keyTops.resize(size_t(KeyTopsWidth) * KeyTopsHeight);

    for (uint32_t y = 0; y < KeyTopsHeight; y++) {
      for (uint32_t x = 0; x < KeyTopsWidth; x++)
        m_keyTops[size_t(y) * KeyTopsWidth + x] = sample(float(KeyTopsX + x), float(KeyTopsY + y));
    }

    return true;
  }


  void VrTerminalTexture::drawBase(std::vector<uint32_t>& pixels) const {
    uint32_t w = width();
    uint32_t h = height();
    pixels.resize(size_t(w) * h);

    for (uint32_t y = 0; y < h; y++) {
      for (uint32_t x = 0; x < w; x++)
        pixels[size_t(y) * w + x] = sample(float(x), float(y));
    }
  }


  void VrTerminalTexture::drawKeyTops(const VrTerminalImage& keyboard, uint32_t* pixels) const {
    // The keyboard image starts at the plate's corner, at the same scale
    uint32_t imageX = KeyTopsX - uint32_t(VrTerminalKeyboard::Plate.left) * Scale;
    uint32_t imageY = KeyTopsY - uint32_t(VrTerminalKeyboard::Plate.top)  * Scale;

    const uint32_t* image = keyboard.pixels();

    for (uint32_t y = 0; y < KeyTopsHeight; y++) {
      for (uint32_t x = 0; x < KeyTopsWidth; x++) {
        uint32_t base = m_keyTops[size_t(y) * KeyTopsWidth + x];
        uint32_t over = image[size_t(imageY + y) * VrTerminalImage::Width + imageX + x];

        float a = float(over >> 24) / 255.0f;

        auto blend = [&] (uint32_t shift) {
          return float(channel(over, shift)) * a + float(channel(base, shift)) * (1.0f - a);
        };

        pixels[size_t(y) * KeyTopsWidth + x] = pack(float(base >> 24), blend(16), blend(8), blend(0));
      }
    }
  }


  uint32_t VrTerminalTexture::sample(float x, float y) const {
    // Pixel centres of the copy, in texels of the texture
    float tx = (x + 0.5f) / float(Scale) - 0.5f;
    float ty = (y + 0.5f) / float(Scale) - 0.5f;

    // Bilinear, the four texels read once for every channel
    tx = std::clamp(tx, 0.0f, float(m_texture.width  - 1));
    ty = std::clamp(ty, 0.0f, float(m_texture.height - 1));

    uint32_t x0 = uint32_t(tx);
    uint32_t y0 = uint32_t(ty);
    uint32_t x1 = std::min(x0 + 1, m_texture.width  - 1);
    uint32_t y1 = std::min(y0 + 1, m_texture.height - 1);

    float fx = tx - float(x0);
    float fy = ty - float(y0);

    uint32_t texels[4] = { m_texture.at(x0, y0), m_texture.at(x1, y0), m_texture.at(x0, y1), m_texture.at(x1, y1) };
    float    weights[4] = { (1.0f - fx) * (1.0f - fy), fx * (1.0f - fy), (1.0f - fx) * fy, fx * fy };

    auto filter = [&] (uint32_t shift) {
      float value = 0.0f;

      for (uint32_t i = 0; i < 4; i++)
        value += float(channel(texels[i], shift)) * weights[i];

      return value;
    };

    return pack(filter(24), filter(16), filter(8), filter(0));
  }

}
