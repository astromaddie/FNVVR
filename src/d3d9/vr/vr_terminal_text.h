#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace dxvk {

  /**
   * \brief Text layout of the console on the terminal's screen
   *
   * The screen is laid out like the game's terminal menu: a 4:3 area with
   * the text 100 units from its left edge, in the terminal font, whose
   * characters are all 17 units wide and whose lines are 25 units high
   * (see findings/console.md §6.3). Two header lines come first, then the
   * console's output, then the command line at the bottom.
   */
  struct VrTerminalTextLayout {
    /// Characters per line
    constexpr static uint32_t Columns     = 44;
    /// Lines of output between the header and the command line
    constexpr static uint32_t OutputLines = 20;
    /// Prompt in front of the command, as on the game's terminals
    constexpr static char     Prompt[]    = "> ";
  };

  /**
   * \brief What the terminal's screen shows
   */
  struct VrTerminalScreenText {
    /// Output lines, oldest first, wrapped to the screen's width
    std::vector<std::string> output;
    /// Command line as shown, the prompt included
    std::string command;
    /// Column of the text cursor on the command line
    uint32_t    cursorColumn = 0;
    /// Whether the blinking text cursor is lit
    bool        cursorShown  = false;
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
   * \brief Lays out the command line
   *
   * The command follows the prompt and scrolls sideways to keep the text
   * cursor on the line.
   *
   * \param [in] input Command being typed
   * \param [in] cursor Position of the text cursor in \c input
   * \param [in] columns Characters per screen line
   * \param [out] text The line as shown
   * \param [out] cursorColumn Column of the text cursor on it
   */
  void vrLayoutTerminalCommand(
    const std::string&              input,
          uint32_t                  cursor,
          uint32_t                  columns,
          std::string&              text,
          uint32_t&                 cursorColumn);

}
