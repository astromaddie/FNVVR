#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "vr_types.h"

namespace dxvk {

  /**
   * \brief What the console terminal shows
   */
  struct VrTerminalView {
    /// Lines the console printed, oldest first, as many as fit or more
    std::vector<std::string> output;
    /// Command being typed
    std::string input;
    /// Position of the text cursor in \c input
    uint32_t    cursor      = 0;
    /// Whether the blinking text cursor is lit
    bool        cursorShown = true;
    /// Key the active laser points at, -1 if none
    int32_t     hoveredKey  = -1;
    /// Key drawn pressed, -1 if none
    int32_t     pressedKey  = -1;
    /// Whether the next character is shifted
    bool        shift       = false;
  };

  /**
   * \brief Layout of the console terminal's image
   *
   * The image is a terminal seen from the front: a screen in the style of
   * the game's terminal and hacking menus at the top, green text over a
   * dark screen with scan lines, and a keyboard below it, with the beige
   * keys of the terminals in the game's world.
   */
  struct VrTerminalLayout {
    constexpr static uint32_t ImageWidth  = 1024;
    constexpr static uint32_t ImageHeight = 1024;

    /// Characters per line of the screen
    constexpr static uint32_t Columns     = 85;
    /// Lines of the screen: a header, the output, and the command line
    constexpr static uint32_t Lines       = 22;
    constexpr static uint32_t HeaderLines = 3;
    constexpr static uint32_t OutputLines = Lines - HeaderLines - 1;

    /**
     * \brief Finds the key at a point of the image
     *
     * \param [in] uv Point on the image, (0, 0) top left, (1, 1) bottom right
     * \returns Index of the key in VrTerminalKeyboard::keys, -1 if none
     */
    static int32_t findKey(VrVector2 uv);
  };

  /**
   * \brief Wraps console lines to the screen's width
   *
   * Lines break after the last space that fits, or at the width when a
   * word is longer than a line. Tabs become spaces.
   *
   * \param [in] lines Lines, oldest first
   * \param [in] columns Characters per screen line
   * \param [in] rows Screen lines to keep
   * \returns The last \c rows screen lines, oldest first
   */
  std::vector<std::string> vrWrapTerminalLines(
    const std::vector<std::string>& lines,
          uint32_t                  columns,
          uint32_t                  rows);

  /**
   * \brief Image of the console terminal
   *
   * The parts that never change (the case, the screen's glass and the
   * keyboard's well) are drawn once. Each update then only draws again
   * the lines of the screen whose text changed and the keys whose look
   * changed, so that typing and the cursor's blinking stay cheap.
   */
  class VrTerminalImage {

  public:

    VrTerminalImage();

    /**
     * \brief Draws what changed since the previous update
     *
     * \param [in] view What the terminal shows
     * \returns Whether the image changed
     */
    bool update(const VrTerminalView& view);

    /**
     * \brief The image
     *
     * VrTerminalLayout::ImageWidth by ImageHeight pixels, rows from the
     * top, each pixel 0xAARRGGBB with straight alpha. The rounded corners
     * of the case are transparent.
     */
    const uint32_t* pixels() const {
      return m_pixels.data();
    }

  private:

    std::vector<uint32_t> m_base;
    std::vector<uint32_t> m_pixels;
    std::vector<uint8_t>  m_mask;
    std::vector<uint16_t> m_blur;

    // Text of each line of the screen as drawn, and the cursor's column on
    // the command line, -1 while it is off
    std::vector<std::string> m_lines;
    int32_t               m_cursorColumn = -1;

    VrTerminalView        m_drawn;
    bool                  m_valid = false;

    void drawBase();

    bool drawScreen(const VrTerminalView& view);

    void composeLine(uint32_t line);

    void drawKey(int32_t index, const VrTerminalView& view);

    void printLine(uint32_t line, uint32_t column, const std::string& text);

    void invertCell(uint32_t line, uint32_t column);

  };

}
