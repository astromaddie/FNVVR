#pragma once

#include <cstdint>

namespace dxvk {

  /**
   * \brief Keys the game's console understands besides characters
   *
   * Codes InterfaceManager::GetKeyFromScancodeAndSetModifiers returns for
   * the keyboard's editing keys, which MenuConsole::Idle handles, see
   * findings/console.md. A character is passed as its own code.
   */
  struct VrConsoleKey {
    constexpr static uint32_t Backspace = 0x80000000u;
    constexpr static uint32_t Left      = 0x80000001u;
    constexpr static uint32_t Right     = 0x80000002u;
    /// Previous command of the history
    constexpr static uint32_t Up        = 0x80000003u;
    /// Next command of the history
    constexpr static uint32_t Down      = 0x80000004u;
    constexpr static uint32_t Home      = 0x80000005u;
    constexpr static uint32_t End       = 0x80000006u;
    constexpr static uint32_t Delete    = 0x80000007u;
    constexpr static uint32_t Enter     = 0x80000008u;
    /// Scrolls the output up by a page
    constexpr static uint32_t PageUp    = 0x80000009u;
    /// Scrolls the output down by a page
    constexpr static uint32_t PageDown  = 0x8000000Au;
  };

  /**
   * \brief What a key of the virtual keyboard does
   */
  enum class VrTerminalKeyKind : uint32_t {
    /// Types a character, the shifted one while Shift is on
    Char,
    /// Sends one of the VrConsoleKey codes
    Edit,
    /// Shifts the next character
    Shift,
    /// Closes the console
    Close,
  };

  /**
   * \brief One key of the virtual keyboard
   *
   * Positions are in key units: a character key is one unit wide and
   * one row high, and the keyboard is VrTerminalKeyboard::Width units
   * wide and VrTerminalKeyboard::Rows rows high.
   */
  struct VrTerminalKey {
    VrTerminalKeyKind kind;
    float             x;
    uint32_t          row;
    float             width;
    /// Character typed, for VrTerminalKeyKind::Char
    char              lower;
    /// Character typed while Shift is on
    char              upper;
    /// VrConsoleKey code, for VrTerminalKeyKind::Edit
    uint32_t          code;
    /// Name shown on the key, \c nullptr to show the character typed
    const char*       label;
  };

  /**
   * \brief Layout of the virtual keyboard
   *
   * A US layout without the keys the console cannot use ('`', '~' and
   * '|': the game draws the console's text cursor with '|'), with the
   * console's editing keys on the bottom row: the arrows move the text
   * cursor and walk the command history, Page Up and Page Down scroll the
   * output.
   */
  struct VrTerminalKeyboard {
    constexpr static float    Width = 14.0f;
    constexpr static uint32_t Rows  = 5;

    /// All keys, with their count
    static const VrTerminalKey* keys(uint32_t& count);

    /**
     * \brief Finds the key at a point
     *
     * \param [in] x Distance from the keyboard's left edge, in key units
     * \param [in] y Distance from the keyboard's top edge, in rows
     * \returns Index of the key, or -1 if the point is on no key
     */
    static int32_t findKey(float x, float y);

    /**
     * \brief Code a character or editing key sends to the console
     *
     * \param [in] key Key of kind Char or Edit
     * \param [in] shift Whether Shift is on
     */
    static uint32_t code(const VrTerminalKey& key, bool shift);
  };

}
