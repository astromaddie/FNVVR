#include <cstring>
#include <string>
#include <vector>

#include "test.h"

#include "../vr_terminal_font.h"
#include "../vr_terminal_image.h"
#include "../vr_terminal_keyboard.h"

using namespace dxvk;

namespace {

  int32_t findChar(char c) {
    uint32_t count = 0;
    const VrTerminalKey* keys = VrTerminalKeyboard::keys(count);

    for (uint32_t i = 0; i < count; i++) {
      if (keys[i].kind == VrTerminalKeyKind::Char && keys[i].lower == c)
        return int32_t(i);
    }

    return -1;
  }

  int32_t findLabel(const char* label) {
    uint32_t count = 0;
    const VrTerminalKey* keys = VrTerminalKeyboard::keys(count);

    for (uint32_t i = 0; i < count; i++) {
      if (keys[i].label && !std::strcmp(keys[i].label, label))
        return int32_t(i);
    }

    return -1;
  }

  const VrTerminalKey& keyAt(int32_t index) {
    uint32_t count = 0;
    return VrTerminalKeyboard::keys(count)[index];
  }

  // Centre of a key on the keyboard, in key units
  void keyCentre(int32_t index, float& x, float& y) {
    const VrTerminalKey& key = keyAt(index);
    x = key.x + 0.5f * key.width;
    y = float(key.row) + 0.5f;
  }

  bool differs(const uint32_t* a, const std::vector<uint32_t>& b) {
    return std::memcmp(a, b.data(), b.size() * sizeof(uint32_t)) != 0;
  }

}


TEST_CASE(terminal_keyboard_rows_fill_its_width) {
  uint32_t count = 0;
  const VrTerminalKey* keys = VrTerminalKeyboard::keys(count);

  for (uint32_t i = 0; i < count; i++) {
    CHECK(keys[i].row < VrTerminalKeyboard::Rows);
    CHECK(keys[i].x >= 0.0f);
    CHECK(keys[i].x + keys[i].width <= VrTerminalKeyboard::Width + 1e-4f);

    // Keys of a row never overlap
    for (uint32_t j = i + 1; j < count; j++) {
      if (keys[j].row == keys[i].row) {
        CHECK(keys[j].x >= keys[i].x + keys[i].width - 1e-4f
           || keys[i].x >= keys[j].x + keys[j].width - 1e-4f);
      }
    }
  }
}


TEST_CASE(terminal_keyboard_finds_the_key_under_a_point) {
  for (char c : std::string("1qa z.")) {
    int32_t index = findChar(c);
    CHECK(index >= 0);

    float x, y;
    keyCentre(index, x, y);
    CHECK(VrTerminalKeyboard::findKey(x, y) == index);
  }

  int32_t enter = findLabel("ENTER");
  float x, y;
  keyCentre(enter, x, y);
  CHECK(VrTerminalKeyboard::findKey(x, y) == enter);

  // The indent of the second row and points off the keyboard are no key
  CHECK(VrTerminalKeyboard::findKey(0.2f, 1.5f) == -1);
  CHECK(VrTerminalKeyboard::findKey(5.0f, -0.1f) == -1);
  CHECK(VrTerminalKeyboard::findKey(5.0f, 5.0f) == -1);
  CHECK(VrTerminalKeyboard::findKey(14.5f, 0.5f) == -1);
}


TEST_CASE(terminal_keyboard_never_types_the_console_cursor) {
  uint32_t count = 0;
  const VrTerminalKey* keys = VrTerminalKeyboard::keys(count);

  for (uint32_t i = 0; i < count; i++) {
    if (keys[i].kind != VrTerminalKeyKind::Char)
      continue;

    CHECK(keys[i].lower != '|' && keys[i].upper != '|');
    CHECK(keys[i].lower != '`' && keys[i].upper != '~');
  }
}


TEST_CASE(terminal_keyboard_codes_follow_shift) {
  const VrTerminalKey& quote = keyAt(findChar('\''));
  CHECK(VrTerminalKeyboard::code(quote, false) == uint32_t('\''));
  CHECK(VrTerminalKeyboard::code(quote, true)  == uint32_t('"'));

  const VrTerminalKey& period = keyAt(findChar('.'));
  CHECK(VrTerminalKeyboard::code(period, false) == uint32_t('.'));

  const VrTerminalKey& back = keyAt(findLabel("BACK"));
  CHECK(VrTerminalKeyboard::code(back, false) == VrConsoleKey::Backspace);
  CHECK(VrTerminalKeyboard::code(back, true)  == VrConsoleKey::Backspace);

  CHECK(keyAt(findLabel("EXIT")).kind  == VrTerminalKeyKind::Close);
  CHECK(keyAt(findLabel("SHIFT")).kind == VrTerminalKeyKind::Shift);
  CHECK(keyAt(findLabel("SPACE")).kind == VrTerminalKeyKind::Char);
  CHECK(VrTerminalKeyboard::code(keyAt(findLabel("SPACE")), true) == uint32_t(' '));
}


TEST_CASE(terminal_layout_finds_keys_on_the_image) {
  // The top of the image is the screen
  CHECK(VrTerminalLayout::findKey({ 0.5f, 0.2f }) == -1);

  // The space bar is at the bottom, in the middle
  CHECK(VrTerminalLayout::findKey({ 0.5f, 0.93f }) == findLabel("SPACE"));
}


TEST_CASE(terminal_lines_wrap_at_spaces) {
  auto lines = vrWrapTerminalLines({ "abc def ghi" }, 7, 10);
  CHECK(lines.size() == 2);
  CHECK(lines[0] == "abc def");
  CHECK(lines[1] == "ghi");

  // A word longer than a line is cut
  lines = vrWrapTerminalLines({ "abcdefghij" }, 4, 10);
  CHECK(lines.size() == 3);
  CHECK(lines[0] == "abcd");
  CHECK(lines[2] == "ij");

  // Empty lines are kept, line ends dropped, tabs become spaces
  lines = vrWrapTerminalLines({ "", "a\tb\r\n" }, 10, 10);
  CHECK(lines.size() == 2);
  CHECK(lines[0].empty());
  CHECK(lines[1] == "a b");
}


TEST_CASE(terminal_lines_keep_the_last_rows) {
  auto lines = vrWrapTerminalLines({ "one", "two", "three four" }, 5, 3);
  CHECK(lines.size() == 3);
  CHECK(lines[0] == "two");
  CHECK(lines[1] == "three");
  CHECK(lines[2] == "four");
}


TEST_CASE(terminal_font_has_printable_ascii) {
  // A space is blank, a letter is not, and characters without a glyph are '?'
  const uint16_t* space = vrTerminalGlyph(' ');
  const uint16_t* letter = vrTerminalGlyph('A');
  uint32_t lit = 0;

  for (uint32_t y = 0; y < VrTerminalFont::GlyphHeight; y++) {
    CHECK(space[y] == 0);
    lit += letter[y] ? 1 : 0;
    CHECK(letter[y] < (1u << VrTerminalFont::GlyphWidth));
  }

  CHECK(lit > 0);
  CHECK(vrTerminalGlyph('\x01') == vrTerminalGlyph('?'));
  CHECK(vrTerminalGlyph(char(0xE9)) == vrTerminalGlyph('?'));
}


TEST_CASE(terminal_image_draws_only_what_changed) {
  VrTerminalImage image;
  VrTerminalView view;
  view.output = { "help \"9mm\" 4" };
  view.input  = "player.additem f 1";
  view.cursor = uint32_t(view.input.size());

  CHECK(image.update(view));

  constexpr uint32_t Width  = VrTerminalLayout::ImageWidth;
  constexpr uint32_t Height = VrTerminalLayout::ImageHeight;
  std::vector<uint32_t> first(image.pixels(), image.pixels() + Width * Height);

  // The corners are transparent, the case and the screen opaque
  CHECK((first[0] >> 24) == 0);
  CHECK((first[(Height / 2) * Width + 4] >> 24) == 0xFF);
  CHECK((first[(Height / 4) * Width + Width / 2] >> 24) == 0xFF);

  // Nothing changed, nothing is drawn
  CHECK(!image.update(view));
  CHECK(!differs(image.pixels(), first));

  // The cursor's blinking and the hovered key change the image
  view.cursorShown = false;
  CHECK(image.update(view));
  CHECK(differs(image.pixels(), first));

  view.cursorShown = true;
  CHECK(image.update(view));
  CHECK(!differs(image.pixels(), first));

  view.hoveredKey = findChar('q');
  CHECK(image.update(view));
  CHECK(differs(image.pixels(), first));

  view.hoveredKey = -1;
  CHECK(image.update(view));
  CHECK(!differs(image.pixels(), first));

  // Drawn again from scratch, an image gives the same pixels
  VrTerminalImage other;
  view.shift = true;
  other.update(view);
  view.shift = false;
  other.update(view);
  CHECK(!differs(other.pixels(), first));
}
