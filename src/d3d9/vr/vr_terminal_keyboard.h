#pragma once

#include <cstdint>

#include "vr_types.h"

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
    /// Shifts the letters until pressed again
    CapsLock,
    /// Closes the console
    Close,
  };

  /**
   * \brief A rectangle on the keyboard's texture, in texels
   */
  struct VrTexelRect {
    float left;
    float top;
    float right;
    float bottom;

    constexpr float width() const {
      return right - left;
    }

    constexpr float height() const {
      return bottom - top;
    }
  };

  /**
   * \brief One key of the virtual keyboard
   */
  struct VrTerminalKey {
    VrTerminalKeyKind kind;
    /// Where the key is drawn on the terminal texture
    VrTexelRect       rect;
    /// Character typed, for VrTerminalKeyKind::Char
    char              lower;
    /// Character typed while shifted
    char              upper;
    /// VrConsoleKey code, for VrTerminalKeyKind::Edit
    uint32_t          code;
    /// Name shown on the key, \c nullptr to show the character typed
    const char*       label;
  };

  /**
   * \brief Layout of the virtual keyboard
   *
   * The keyboard is the one of the terminals in the game's world, whose
   * texture (textures\\terminals\\terminal01.dds, 512 texels square) has
   * a keyboard plate with unlabelled keys, see findings/console.md §7.
   * Every key of the plate gets a key of a US layout or of the console:
   * the yellow keys above scroll the output and close the console, the
   * arrows move the text cursor and walk the command history. The
   * orange light shows that Shift or Caps Lock is on.
   *
   * Key rectangles were measured on the texture, in texels.
   */
  struct VrTerminalKeyboard {
    /// The plate shown as the keyboard, on the texture
    constexpr static VrTexelRect Plate = { 2.0f, 287.0f, 294.0f, 386.0f };
    /// A piece of another part of the terminal inside the plate's
    /// rectangle, below its left wing, left out
    constexpr static VrTexelRect OtherPiece = { 2.0f, 358.0f, 19.0f, 386.0f };
    /// Centre and radius of the orange light, on the texture
    constexpr static float LightX      = 84.2f;
    constexpr static float LightY      = 364.0f;
    constexpr static float LightRadius = 2.6f;

    /// All keys, with their count
    static const VrTerminalKey* keys(uint32_t& count);

    /**
     * \brief Finds the key at a point of the plate
     *
     * Points in the narrow gaps between keys belong to the nearest key.
     *
     * \param [in] uv Point on the plate, (0, 0) top left, (1, 1) bottom right
     * \returns Index of the key, or -1 if the point is on no key
     */
    static int32_t findKey(VrVector2 uv);

    /**
     * \brief Whether a key shows and types its shifted character
     *
     * Shift shifts every character, Caps Lock only the letters.
     */
    static bool isShifted(const VrTerminalKey& key, bool shift, bool capsLock);

    /**
     * \brief Code a character or editing key sends to the console
     *
     * \param [in] key Key of kind Char or Edit
     * \param [in] shifted Whether the key is shifted, see isShifted
     */
    static uint32_t code(const VrTerminalKey& key, bool shifted);
  };

}
