#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace dxvk {

  /**
   * \brief Image in system memory
   *
   * Rows from the top, each pixel 0xAARRGGBB with straight alpha.
   */
  struct VrImage {
    uint32_t              width  = 0;
    uint32_t              height = 0;
    std::vector<uint32_t> pixels;

    uint32_t at(uint32_t x, uint32_t y) const {
      return pixels[size_t(y) * width + x];
    }
  };

  /**
   * \brief Decodes the top level of a DDS file
   *
   * Reads the formats the game's textures use: DXT1, DXT3 and DXT5, and
   * uncompressed 16, 24 and 32 bit pixels described by their channel masks
   * (such as the A4R4G4B4 of the game's font textures). Mipmaps, cube maps
   * and volume textures beyond the first image are ignored.
   *
   * \param [in] data The file's content
   * \param [in] size Size of the content in bytes
   * \param [out] image The decoded image
   * \returns \c false if the file is not a DDS file in one of these formats
   */
  bool vrDecodeDds(const uint8_t* data, size_t size, VrImage& image);

  /**
   * \brief Samples one channel of an image with bilinear filtering
   *
   * \param [in] image The image
   * \param [in] x Horizontal position in pixels, pixel centres at whole numbers
   * \param [in] y Vertical position in pixels
   * \param [in] shift Bit position of the channel: 24 alpha, 16 red, 8 green, 0 blue
   * \returns The channel's value, from 0 to 255
   */
  float vrSampleChannel(const VrImage& image, float x, float y, uint32_t shift);

}
