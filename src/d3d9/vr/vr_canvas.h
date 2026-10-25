#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "vr_bitmap_font.h"
#include "vr_dds.h"

namespace dxvk {

  /**
   * \brief Colour with straight alpha, each channel from 0 to 1
   */
  struct VrColour {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 0.0f;

    /// The same colour with its alpha scaled
    VrColour faded(float factor) const {
      return { r, g, b, a * factor };
    }

    /// The colour from 0xRRGGBBAA, as the game stores the HUD's colour
    static VrColour fromRgba(uint32_t rgba);
  };

  /**
   * \brief A game font and the texture holding its glyphs
   */
  struct VrCanvasFont {
    VrBitmapFont metrics;
    /// The font's texture, glyphs in its alpha
    VrImage      glyphs;
  };

  /**
   * \brief Image drawn on the CPU for a panel
   *
   * Rows from the top, each pixel 0xAARRGGBB with straight alpha, as
   * VrImage. Everything is drawn over what is there, with soft edges.
   */
  class VrCanvas {

  public:

    VrCanvas(uint32_t width, uint32_t height);

    uint32_t width() const {
      return m_width;
    }

    uint32_t height() const {
      return m_height;
    }

    const uint32_t* pixels() const {
      return m_pixels.data();
    }

    uint32_t at(uint32_t x, uint32_t y) const {
      return m_pixels[size_t(y) * m_width + x];
    }

    /**
     * \brief Makes every pixel transparent
     */
    void clear();

    /**
     * \brief Fills a rectangle with rounded corners
     *
     * \param [in] left Left edge, in pixels
     * \param [in] top Top edge, in pixels
     * \param [in] right Right edge, in pixels
     * \param [in] bottom Bottom edge, in pixels
     * \param [in] radius Radius of the corners, in pixels
     * \param [in] colour Colour
     */
    void fillRoundedRect(float left, float top, float right, float bottom, float radius, const VrColour& colour);

    /**
     * \brief Fills part of a ring
     *
     * Angles in radians, clockwise from the top of the image. The sector
     * runs clockwise from \c start to \c end, \c end may wrap past 2 pi.
     *
     * \param [in] cx Horizontal centre, in pixels
     * \param [in] cy Vertical centre, in pixels
     * \param [in] inner Inner radius, in pixels, 0 for a disc
     * \param [in] outer Outer radius, in pixels
     * \param [in] start First angle
     * \param [in] end Last angle
     * \param [in] colour Colour
     */
    void fillSector(float cx, float cy, float inner, float outer, float start, float end, const VrColour& colour);

    /**
     * \brief Draws an image's shape in one colour
     *
     * The image's alpha times its brightness is the shape, as the game
     * tints its interface icons with the HUD's colour. The image keeps its
     * aspect and fits in a square.
     *
     * \param [in] image The image
     * \param [in] cx Horizontal centre of the square, in pixels
     * \param [in] cy Vertical centre of the square, in pixels
     * \param [in] size Side of the square, in pixels
     * \param [in] colour Colour
     */
    void drawShape(const VrImage& image, float cx, float cy, float size, const VrColour& colour);

    /**
     * \brief Width of a line of text, in pixels
     */
    static float measureText(const VrCanvasFont& font, const std::string& text, float scale);

    /**
     * \brief Draws a line of text
     *
     * \param [in] font The font
     * \param [in] text The text, one byte per character
     * \param [in] x Left end of the line, in pixels
     * \param [in] baseline Height of the line's baseline, in pixels
     * \param [in] scale Pixels per font pixel
     * \param [in] colour Colour
     */
    void drawText(const VrCanvasFont& font, const std::string& text, float x, float baseline, float scale,
      const VrColour& colour);

    /**
     * \brief Draws lines of text centred on a point
     *
     * Words are wrapped to fit the width; lines that do not fit in
     * \c maxLines are dropped, the last one shown ending with "...".
     * The text shrinks down to \c minScale before it wraps.
     *
     * \param [in] font The font
     * \param [in] text The text
     * \param [in] cx Horizontal centre, in pixels
     * \param [in] cy Vertical centre of the lines, in pixels
     * \param [in] maxWidth Widest a line may be, in pixels
     * \param [in] scale Pixels per font pixel
     * \param [in] minScale Smallest scale before wrapping
     * \param [in] maxLines Most lines drawn
     * \param [in] colour Colour
     */
    void drawTextBlock(const VrCanvasFont& font, const std::string& text, float cx, float cy, float maxWidth,
      float scale, float minScale, uint32_t maxLines, const VrColour& colour);

  private:

    uint32_t              m_width;
    uint32_t              m_height;
    std::vector<uint32_t> m_pixels;

    void blend(uint32_t x, uint32_t y, const VrColour& colour, float coverage);

  };

  /**
   * \brief Splits a text into lines no wider than a width
   *
   * Words longer than the width are kept whole on their own line.
   *
   * \param [in] font The font
   * \param [in] text The text
   * \param [in] scale Pixels per font pixel
   * \param [in] maxWidth Widest a line may be, in pixels
   */
  std::vector<std::string> vrWrapText(const VrCanvasFont& font, const std::string& text, float scale, float maxWidth);

}
