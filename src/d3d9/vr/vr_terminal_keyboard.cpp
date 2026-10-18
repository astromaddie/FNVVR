#include <cctype>
#include <vector>

#include "vr_terminal_keyboard.h"

namespace dxvk {

  namespace {

    // Measured on terminal01.dds: the small keys of a row are 9.6 texels
    // wide, one every 10.73 texels
    constexpr float KeyPitch = 10.73f;
    constexpr float KeyWidth = 9.6f;

    struct Row {
      float top;
      float bottom;
    };

    constexpr Row Rows[] = {
      { 313.5f, 323.5f },
      { 325.0f, 334.5f },
      { 336.0f, 345.5f },
      { 347.0f, 357.0f },
      { 359.0f, 368.5f },
    };

    // The three yellow keys above the keyboard
    constexpr Row FunctionRow = { 297.5f, 306.25f };

    // A point this close to a key, in texels, belongs to it
    constexpr float HitMargin = 0.6f;

    VrTexelRect span(const Row& row, float left, float right) {
      return { left, row.top, right, row.bottom };
    }

    VrTerminalKey special(VrTerminalKeyKind kind, VrTexelRect rect, uint32_t code, const char* label) {
      return { kind, rect, '\0', '\0', code, label };
    }

    // Small keys typing the characters of `lower`, from `left` on
    void addChars(std::vector<VrTerminalKey>& keys, const Row& row, float left,
        const char* lower, const char* upper) {
      for (uint32_t i = 0; lower[i]; i++) {
        float x = left + float(i) * KeyPitch;
        keys.push_back({ VrTerminalKeyKind::Char, span(row, x, x + KeyWidth), lower[i], upper[i], 0u, nullptr });
      }
    }

    void addEdit(std::vector<VrTerminalKey>& keys, VrTexelRect rect, uint32_t code, const char* label) {
      keys.push_back(special(VrTerminalKeyKind::Edit, rect, code, label));
    }

    std::vector<VrTerminalKey> buildKeys() {
      std::vector<VrTerminalKey> keys;

      const Row& f = FunctionRow;
      addEdit(keys, span(f,  74.5f,  86.25f), VrConsoleKey::PageUp,   "PGUP");
      addEdit(keys, span(f,  86.75f, 99.25f), VrConsoleKey::PageDown, "PGDN");
      keys.push_back(special(VrTerminalKeyKind::Close, span(f, 100.0f, 111.75f), 0u, "EXIT"));

      // The key left of 1 is where Escape is on most keyboards
      keys.push_back(special(VrTerminalKeyKind::Close, span(Rows[0], 55.6f, 55.6f + KeyWidth), 0u, "ESC"));
      addChars(keys, Rows[0], 55.6f + KeyPitch, "1234567890-=", "!@#$%^&*()_+");
      addEdit(keys, span(Rows[0], 195.6f, 211.9f), VrConsoleKey::Backspace, "BACK");

      addEdit(keys, span(Rows[1], 56.2f, 70.6f), VrConsoleKey::Delete, "DEL");
      addChars(keys, Rows[1], 72.5f, "qwertyuiop[]\\", "QWERTYUIOP{}\\");

      keys.push_back(special(VrTerminalKeyKind::CapsLock, span(Rows[2], 56.0f, 73.1f), 0u, "CAPS"));
      addChars(keys, Rows[2], 75.6f, "asdfghjkl;'", "ASDFGHJKL:\"");
      addEdit(keys, span(Rows[2], 193.1f, 211.25f), VrConsoleKey::Enter, "ENTER");

      keys.push_back(special(VrTerminalKeyKind::Shift, span(Rows[3], 56.0f, 79.4f), 0u, "SHIFT"));
      addChars(keys, Rows[3], 80.6f, "zxcvbnm,./", "ZXCVBNM<>?");
      keys.push_back(special(VrTerminalKeyKind::Shift, span(Rows[3], 188.1f, 211.25f), 0u, "SHIFT"));

      addEdit(keys, span(Rows[4],  55.6f,  65.0f), VrConsoleKey::Home, "HOME");
      addEdit(keys, span(Rows[4],  66.9f,  76.2f), VrConsoleKey::End,  "END");
      keys.push_back({ VrTerminalKeyKind::Char, span(Rows[4], 91.2f, 168.0f), ' ', ' ', 0u, "" });
      addEdit(keys, span(Rows[4], 170.0f, 179.4f), VrConsoleKey::Left,  "<");
      addEdit(keys, span(Rows[4], 181.0f, 190.6f), VrConsoleKey::Up,    "^");
      addEdit(keys, span(Rows[4], 191.9f, 200.6f), VrConsoleKey::Down,  "v");
      addEdit(keys, span(Rows[4], 201.9f, 211.0f), VrConsoleKey::Right, ">");

      return keys;
    }

    const std::vector<VrTerminalKey> g_keys = buildKeys();

    bool contains(const VrTexelRect& rect, float x, float y) {
      return x >= rect.left - HitMargin && x <= rect.right + HitMargin
          && y >= rect.top - HitMargin && y <= rect.bottom + HitMargin;
    }

  }


  const VrTerminalKey* VrTerminalKeyboard::keys(uint32_t& count) {
    count = uint32_t(g_keys.size());
    return g_keys.data();
  }


  int32_t VrTerminalKeyboard::findKey(VrVector2 uv) {
    float x = Plate.left + uv.x * Plate.width();
    float y = Plate.top  + uv.y * Plate.height();

    for (size_t i = 0; i < g_keys.size(); i++) {
      if (contains(g_keys[i].rect, x, y))
        return int32_t(i);
    }

    return -1;
  }


  bool VrTerminalKeyboard::isShifted(const VrTerminalKey& key, bool shift, bool capsLock) {
    bool letter = std::isalpha(static_cast<unsigned char>(key.lower)) != 0;
    return letter ? shift != capsLock : shift;
  }


  uint32_t VrTerminalKeyboard::code(const VrTerminalKey& key, bool shifted) {
    if (key.kind == VrTerminalKeyKind::Edit)
      return key.code;

    return uint32_t(uint8_t(shifted ? key.upper : key.lower));
  }

}
