#pragma once

#include <cstdint>
#include <vector>

#include "vr_bitmap_font.h"
#include "vr_dds.h"
#include "vr_terminal_keyboard.h"

namespace dxvk {

  /**
   * \brief What the keyboard shows
   */
  struct VrTerminalKeyboardView {
    /// Key the active laser points at, -1 if none
    int32_t hoveredKey = -1;
    /// Key drawn pressed, -1 if none
    int32_t pressedKey = -1;
    bool    shift      = false;
    bool    capsLock   = false;

    bool operator == (const VrTerminalKeyboardView& other) const {
      return hoveredKey == other.hoveredKey && pressedKey == other.pressedKey
          && shift == other.shift && capsLock == other.capsLock;
    }

    bool operator != (const VrTerminalKeyboardView& other) const {
      return !(*this == other);
    }
  };

  /**
   * \brief Image of the console's keyboard
   *
   * The keyboard plate of the game's terminal texture, enlarged, with the
   * name of each key printed on it in the game's terminal font, the key
   * the laser points at lit, the key just typed pressed down, and the
   * orange light lit while Shift or Caps Lock is on. The plate's dark
   * surroundings on the texture are transparent.
   */
  class VrTerminalImage {

  public:

    /// Image pixels per texel of the terminal texture
    constexpr static uint32_t Scale  = 5;
    constexpr static uint32_t Width  = uint32_t(VrTerminalKeyboard::Plate.width())  * Scale;
    constexpr static uint32_t Height = uint32_t(VrTerminalKeyboard::Plate.height()) * Scale;

    /**
     * \brief Prepares the plate and the font
     *
     * \param [in] texture The terminal texture (terminal01.dds)
     * \param [in] font The terminal font's metrics
     * \param [in] glyphs The terminal font's texture, glyphs in its alpha
     * \returns \c false if the texture is too small to hold the plate
     */
    bool initialize(const VrImage& texture, const VrBitmapFont& font, const VrImage& glyphs);

    /**
     * \brief Draws the keyboard again if its look changed
     *
     * \param [in] view What the keyboard shows
     * \returns Whether the image changed
     */
    bool update(const VrTerminalKeyboardView& view);

    /**
     * \brief The image, Width by Height pixels, see VrImage
     */
    const uint32_t* pixels() const {
      return m_pixels.data();
    }

  private:

    std::vector<uint32_t>  m_plate;
    std::vector<uint32_t>  m_pixels;
    VrBitmapFont           m_font;
    VrImage                m_glyphs;

    VrTerminalKeyboardView m_drawn;
    bool                   m_valid = false;

    void buildPlate(const VrImage& texture);

    void drawKey(const VrTerminalKey& key, bool hovered, bool pressed, bool shifted);

    void drawLabel(const char* text, float centreX, float centreY, float maxWidth, float scale);

    void drawGlyph(const VrGlyph& glyph, float x, float baseline, float scale);

    void drawLight();

  };

}
