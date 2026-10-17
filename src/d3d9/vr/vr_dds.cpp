#include <cstring>

#include "vr_dds.h"

namespace dxvk {

  namespace {

    constexpr uint32_t Magic             = 0x20534444u; // "DDS "
    constexpr size_t   HeaderSize        = 128;
    constexpr size_t   HeightOffset      = 12;
    constexpr size_t   WidthOffset       = 16;
    constexpr size_t   FormatFlagsOffset = 80;
    constexpr size_t   FourCcOffset      = 84;
    constexpr size_t   BitCountOffset    = 88;
    constexpr size_t   MasksOffset       = 92;

    constexpr uint32_t FlagAlphaPixels = 0x1;
    constexpr uint32_t FlagFourCc      = 0x4;

    // Images larger than this are not textures of the game
    constexpr uint32_t MaxExtent = 16384;

    constexpr uint32_t fourCc(char a, char b, char c, char d) {
      return uint32_t(uint8_t(a)) | uint32_t(uint8_t(b)) << 8
           | uint32_t(uint8_t(c)) << 16 | uint32_t(uint8_t(d)) << 24;
    }

    uint32_t read32(const uint8_t* data) {
      uint32_t value;
      std::memcpy(&value, data, sizeof(value));
      return value;
    }

    uint16_t read16(const uint8_t* data) {
      uint16_t value;
      std::memcpy(&value, data, sizeof(value));
      return value;
    }

    uint32_t argb(uint32_t a, uint32_t r, uint32_t g, uint32_t b) {
      return a << 24 | r << 16 | g << 8 | b;
    }

    // R5G6B5 to 8 bits per channel, alpha left at zero
    void expand565(uint16_t c, uint32_t rgb[3]) {
      uint32_t r = (c >> 11) & 0x1F;
      uint32_t g = (c >> 5)  & 0x3F;
      uint32_t b =  c        & 0x1F;
      rgb[0] = (r << 3) | (r >> 2);
      rgb[1] = (g << 2) | (g >> 4);
      rgb[2] = (b << 3) | (b >> 2);
    }

    // The colour part of a DXT block: two end points and 2-bit indices.
    // DXT1 blocks whose first end point is not above the second have a
    // transparent fourth colour, the other formats always use four colours.
    void decodeColours(const uint8_t* block, bool dxt1, uint32_t out[16], bool transparent[16]) {
      uint16_t c0 = read16(block);
      uint16_t c1 = read16(block + 2);
      uint32_t indices = read32(block + 4);

      uint32_t palette[4][3];
      expand565(c0, palette[0]);
      expand565(c1, palette[1]);

      bool threeColours = dxt1 && c0 <= c1;

      for (uint32_t i = 0; i < 3; i++) {
        if (threeColours) {
          palette[2][i] = (palette[0][i] + palette[1][i]) / 2;
          palette[3][i] = 0;
        } else {
          palette[2][i] = (2 * palette[0][i] + palette[1][i]) / 3;
          palette[3][i] = (palette[0][i] + 2 * palette[1][i]) / 3;
        }
      }

      for (uint32_t i = 0; i < 16; i++) {
        uint32_t index = (indices >> (2 * i)) & 0x3;
        out[i] = argb(0, palette[index][0], palette[index][1], palette[index][2]);
        transparent[i] = threeColours && index == 3;
      }
    }

    // DXT3: 4 explicit bits per pixel
    void decodeExplicitAlpha(const uint8_t* block, uint32_t alpha[16]) {
      for (uint32_t i = 0; i < 16; i++) {
        uint32_t a = (block[i / 2] >> (4 * (i % 2))) & 0xF;
        alpha[i] = a * 17;
      }
    }

    // DXT5: two end points and 3-bit indices
    void decodeInterpolatedAlpha(const uint8_t* block, uint32_t alpha[16]) {
      uint32_t a0 = block[0];
      uint32_t a1 = block[1];

      uint32_t palette[8] = { a0, a1 };

      if (a0 > a1) {
        for (uint32_t i = 1; i < 7; i++)
          palette[i + 1] = ((7 - i) * a0 + i * a1) / 7;
      } else {
        for (uint32_t i = 1; i < 5; i++)
          palette[i + 1] = ((5 - i) * a0 + i * a1) / 5;

        palette[6] = 0;
        palette[7] = 255;
      }

      uint64_t indices = 0;

      for (uint32_t i = 0; i < 6; i++)
        indices |= uint64_t(block[2 + i]) << (8 * i);

      for (uint32_t i = 0; i < 16; i++)
        alpha[i] = palette[(indices >> (3 * i)) & 0x7];
    }

    bool decodeBlocks(const uint8_t* data, size_t size, uint32_t format, VrImage& image) {
      bool   dxt1      = format == fourCc('D', 'X', 'T', '1');
      size_t blockSize = dxt1 ? 8 : 16;

      uint32_t blocksX = (image.width  + 3) / 4;
      uint32_t blocksY = (image.height + 3) / 4;

      if (size < size_t(blocksX) * blocksY * blockSize)
        return false;

      for (uint32_t by = 0; by < blocksY; by++) {
        for (uint32_t bx = 0; bx < blocksX; bx++) {
          const uint8_t* block = data + (size_t(by) * blocksX + bx) * blockSize;

          uint32_t colours[16];
          bool     transparent[16];
          uint32_t alpha[16];

          if (dxt1) {
            decodeColours(block, true, colours, transparent);

            for (uint32_t i = 0; i < 16; i++)
              alpha[i] = transparent[i] ? 0 : 255;
          } else {
            decodeColours(block + 8, false, colours, transparent);

            if (format == fourCc('D', 'X', 'T', '5'))
              decodeInterpolatedAlpha(block, alpha);
            else
              decodeExplicitAlpha(block, alpha);
          }

          for (uint32_t i = 0; i < 16; i++) {
            uint32_t x = bx * 4 + i % 4;
            uint32_t y = by * 4 + i / 4;

            if (x < image.width && y < image.height)
              image.pixels[size_t(y) * image.width + x] = colours[i] | alpha[i] << 24;
          }
        }
      }

      return true;
    }

    // A channel of an uncompressed pixel, scaled to 8 bits
    uint32_t channel(uint32_t pixel, uint32_t mask, uint32_t missing) {
      if (!mask)
        return missing;

      uint32_t shift = 0;

      while (!((mask >> shift) & 1))
        shift++;

      uint32_t maximum = mask >> shift;
      return ((pixel & mask) >> shift) * 255 / maximum;
    }

    bool decodePixels(const uint8_t* data, size_t size, const uint8_t* header, uint32_t flags, VrImage& image) {
      uint32_t bits = read32(header + BitCountOffset);

      if (bits != 16 && bits != 24 && bits != 32)
        return false;

      uint32_t masks[4];

      for (uint32_t i = 0; i < 4; i++)
        masks[i] = read32(header + MasksOffset + 4 * i);

      if (!(flags & FlagAlphaPixels))
        masks[3] = 0;

      size_t bytesPerPixel = bits / 8;

      if (size < size_t(image.width) * image.height * bytesPerPixel)
        return false;

      for (size_t i = 0; i < image.pixels.size(); i++) {
        uint32_t pixel = 0;
        std::memcpy(&pixel, data + i * bytesPerPixel, bytesPerPixel);

        image.pixels[i] = argb(channel(pixel, masks[3], 255), channel(pixel, masks[0], 0),
          channel(pixel, masks[1], 0), channel(pixel, masks[2], 0));
      }

      return true;
    }

  }


  bool vrDecodeDds(const uint8_t* data, size_t size, VrImage& image) {
    image = VrImage();

    if (!data || size < HeaderSize || read32(data) != Magic)
      return false;

    uint32_t width  = read32(data + WidthOffset);
    uint32_t height = read32(data + HeightOffset);

    if (!width || !height || width > MaxExtent || height > MaxExtent)
      return false;

    image.width  = width;
    image.height = height;
    image.pixels.assign(size_t(width) * height, 0u);

    uint32_t flags  = read32(data + FormatFlagsOffset);
    uint32_t format = read32(data + FourCcOffset);

    const uint8_t* pixels = data + HeaderSize;
    size_t         left   = size - HeaderSize;

    bool decoded = false;

    if (flags & FlagFourCc) {
      if (format == fourCc('D', 'X', 'T', '1') || format == fourCc('D', 'X', 'T', '3')
       || format == fourCc('D', 'X', 'T', '5'))
        decoded = decodeBlocks(pixels, left, format, image);
    } else {
      decoded = decodePixels(pixels, left, data, flags, image);
    }

    if (!decoded)
      image = VrImage();

    return decoded;
  }

}
