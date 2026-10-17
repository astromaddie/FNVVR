#include "vr_terminal_keyboard.h"

namespace dxvk {

  namespace {

    constexpr VrTerminalKey ch(float x, uint32_t row, char lower, char upper) {
      return { VrTerminalKeyKind::Char, x, row, 1.0f, lower, upper, 0u, nullptr };
    }

    constexpr VrTerminalKey edit(float x, uint32_t row, float width, uint32_t code, const char* label) {
      return { VrTerminalKeyKind::Edit, x, row, width, '\0', '\0', code, label };
    }

    constexpr VrTerminalKey special(VrTerminalKeyKind kind, float x, uint32_t row, float width, const char* label) {
      return { kind, x, row, width, '\0', '\0', 0u, label };
    }

    // Rows are indented as on a keyboard, so that each row is Width units
    // wide with its wide keys
    constexpr VrTerminalKey Keys[] = {
      ch( 0.0f, 0, '1', '!'), ch( 1.0f, 0, '2', '@'), ch( 2.0f, 0, '3', '#'),
      ch( 3.0f, 0, '4', '$'), ch( 4.0f, 0, '5', '%'), ch( 5.0f, 0, '6', '^'),
      ch( 6.0f, 0, '7', '&'), ch( 7.0f, 0, '8', '*'), ch( 8.0f, 0, '9', '('),
      ch( 9.0f, 0, '0', ')'), ch(10.0f, 0, '-', '_'), ch(11.0f, 0, '=', '+'),
      edit(12.0f, 0, 2.0f, VrConsoleKey::Backspace, "BACK"),

      ch( 0.5f, 1, 'q', 'Q'), ch( 1.5f, 1, 'w', 'W'), ch( 2.5f, 1, 'e', 'E'),
      ch( 3.5f, 1, 'r', 'R'), ch( 4.5f, 1, 't', 'T'), ch( 5.5f, 1, 'y', 'Y'),
      ch( 6.5f, 1, 'u', 'U'), ch( 7.5f, 1, 'i', 'I'), ch( 8.5f, 1, 'o', 'O'),
      ch( 9.5f, 1, 'p', 'P'), ch(10.5f, 1, '[', '{'), ch(11.5f, 1, ']', '}'),
      ch(12.5f, 1, '\\', '\\'),

      ch( 0.75f, 2, 'a', 'A'), ch( 1.75f, 2, 's', 'S'), ch( 2.75f, 2, 'd', 'D'),
      ch( 3.75f, 2, 'f', 'F'), ch( 4.75f, 2, 'g', 'G'), ch( 5.75f, 2, 'h', 'H'),
      ch( 6.75f, 2, 'j', 'J'), ch( 7.75f, 2, 'k', 'K'), ch( 8.75f, 2, 'l', 'L'),
      ch( 9.75f, 2, ';', ':'), ch(10.75f, 2, '\'', '"'),
      edit(11.75f, 2, 2.25f, VrConsoleKey::Enter, "ENTER"),

      special(VrTerminalKeyKind::Shift, 0.0f, 3, 2.25f, "SHIFT"),
      ch( 2.25f, 3, 'z', 'Z'), ch( 3.25f, 3, 'x', 'X'), ch( 4.25f, 3, 'c', 'C'),
      ch( 5.25f, 3, 'v', 'V'), ch( 6.25f, 3, 'b', 'B'), ch( 7.25f, 3, 'n', 'N'),
      ch( 8.25f, 3, 'm', 'M'), ch( 9.25f, 3, ',', '<'), ch(10.25f, 3, '.', '>'),
      ch(11.25f, 3, '/', '?'),
      special(VrTerminalKeyKind::Shift, 12.25f, 3, 1.75f, "SHIFT"),

      special(VrTerminalKeyKind::Close, 0.0f, 4, 2.0f, "EXIT"),
      edit( 2.0f, 4, 1.5f, VrConsoleKey::PageUp,   "PGUP"),
      edit( 3.5f, 4, 1.5f, VrConsoleKey::PageDown, "PGDN"),
      { VrTerminalKeyKind::Char, 5.0f, 4, 5.0f, ' ', ' ', 0u, "SPACE" },
      edit(10.0f, 4, 1.0f, VrConsoleKey::Left,  "<"),
      edit(11.0f, 4, 1.0f, VrConsoleKey::Right, ">"),
      edit(12.0f, 4, 1.0f, VrConsoleKey::Up,    "^"),
      edit(13.0f, 4, 1.0f, VrConsoleKey::Down,  "v"),
    };

  }


  const VrTerminalKey* VrTerminalKeyboard::keys(uint32_t& count) {
    count = uint32_t(sizeof(Keys) / sizeof(Keys[0]));
    return Keys;
  }


  int32_t VrTerminalKeyboard::findKey(float x, float y) {
    if (!(y >= 0.0f && y < float(Rows)))
      return -1;

    uint32_t row = uint32_t(y);
    uint32_t count = 0;
    const VrTerminalKey* keys = VrTerminalKeyboard::keys(count);

    for (uint32_t i = 0; i < count; i++) {
      const VrTerminalKey& key = keys[i];

      if (key.row == row && x >= key.x && x < key.x + key.width)
        return int32_t(i);
    }

    return -1;
  }


  uint32_t VrTerminalKeyboard::code(const VrTerminalKey& key, bool shift) {
    if (key.kind == VrTerminalKeyKind::Edit)
      return key.code;

    return uint32_t(uint8_t(shift ? key.upper : key.lower));
  }

}
