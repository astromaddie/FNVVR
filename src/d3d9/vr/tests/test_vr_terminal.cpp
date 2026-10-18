#include <cstring>
#include <string>
#include <vector>

#include "test.h"

#include "../vr_bitmap_font.h"
#include "../vr_dds.h"
#include "../vr_terminal_image.h"
#include "../vr_terminal_keyboard.h"
#include "../vr_terminal_text.h"
#include "../vr_terminal_texture.h"

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

  // Centre of a key, as a point of the plate
  VrVector2 keyCentre(int32_t index) {
    const VrTexelRect& plate = VrTerminalKeyboard::Plate;
    const VrTexelRect& rect  = keyAt(index).rect;

    VrVector2 uv;
    uv.x = (0.5f * (rect.left + rect.right) - plate.left) / plate.width();
    uv.y = (0.5f * (rect.top + rect.bottom) - plate.top) / plate.height();
    return uv;
  }

  bool differs(const uint32_t* a, const std::vector<uint32_t>& b) {
    return std::memcmp(a, b.data(), b.size() * sizeof(uint32_t)) != 0;
  }

  void put32(std::vector<uint8_t>& data, size_t offset, uint32_t value) {
    std::memcpy(data.data() + offset, &value, sizeof(value));
  }

  void putFloat(std::vector<uint8_t>& data, size_t offset, float value) {
    std::memcpy(data.data() + offset, &value, sizeof(value));
  }

  // A DDS header for a width x height image, without its pixels
  std::vector<uint8_t> ddsHeader(uint32_t width, uint32_t height) {
    std::vector<uint8_t> data(128, 0);
    put32(data, 0, 0x20534444u);
    put32(data, 4, 124);
    put32(data, 12, height);
    put32(data, 16, width);
    put32(data, 76, 32);
    return data;
  }

  std::vector<uint8_t> dxtFile(const char* format, const std::vector<uint8_t>& blocks) {
    std::vector<uint8_t> data = ddsHeader(4, 4);
    put32(data, 80, 0x4);
    std::memcpy(data.data() + 84, format, 4);
    data.insert(data.end(), blocks.begin(), blocks.end());
    return data;
  }

  // A font file: line height 25, one texture, every glyph a box of the
  // given size at the top left of the texture
  std::vector<uint8_t> fontFile(float width, float height) {
    constexpr size_t GlyphsOffset = 8 + 0x120;
    std::vector<uint8_t> data(GlyphsOffset + 256 * 56, 0);
    putFloat(data, 0, 25.0f);
    put32(data, 4, 1);

    for (uint32_t i = 32; i < 127; i++) {
      size_t glyph = GlyphsOffset + i * 56;
      float values[14] = { 0.0f, 0.0f, 0.0f, 0.5f, 0.0f, 0.0f, 0.5f, 0.5f, 0.5f,
        width, height, 1.0f, 2.0f, 18.0f };

      // Capitals are taller than small letters, so that shifting shows
      if (i >= 'a' && i <= 'z')
        values[10] = height / 2.0f;

      std::memcpy(data.data() + glyph, values, sizeof(values));
    }

    // The space has no glyph
    std::memset(data.data() + GlyphsOffset + ' ' * 56, 0, 56);
    putFloat(data, GlyphsOffset + ' ' * 56 + 11 * 4, 17.0f);
    return data;
  }

  // The terminal texture: a light plate whose outermost texels are black
  VrImage terminalTexture() {
    VrImage image;
    image.width  = 512;
    image.height = 512;
    image.pixels.assign(512 * 512, 0xFF000000u);

    const VrTexelRect& plate = VrTerminalKeyboard::Plate;

    for (uint32_t y = uint32_t(plate.top) + 3; y < uint32_t(plate.bottom) - 3; y++) {
      for (uint32_t x = uint32_t(plate.left) + 3; x < uint32_t(plate.right) - 3; x++)
        image.pixels[y * 512 + x] = 0xFF969682u;
    }

    return image;
  }

  VrImage solidGlyphs() {
    VrImage image;
    image.width  = 32;
    image.height = 32;
    image.pixels.assign(32 * 32, 0xFFFFFFFFu);
    return image;
  }

}


TEST_CASE(terminal_keys_lie_on_the_plate_without_overlapping) {
  uint32_t count = 0;
  const VrTerminalKey* keys = VrTerminalKeyboard::keys(count);
  const VrTexelRect& plate = VrTerminalKeyboard::Plate;

  CHECK(count == 63);

  for (uint32_t i = 0; i < count; i++) {
    const VrTexelRect& a = keys[i].rect;
    CHECK(a.left >= plate.left && a.right <= plate.right);
    CHECK(a.top >= plate.top && a.bottom <= plate.bottom);
    CHECK(a.width() > 5.0f && a.height() > 5.0f);

    for (uint32_t j = i + 1; j < count; j++) {
      const VrTexelRect& b = keys[j].rect;
      CHECK(a.right <= b.left || b.right <= a.left || a.bottom <= b.top || b.bottom <= a.top);
    }
  }
}


TEST_CASE(terminal_keyboard_finds_the_key_under_a_point) {
  uint32_t count = 0;
  VrTerminalKeyboard::keys(count);

  for (uint32_t i = 0; i < count; i++)
    CHECK(VrTerminalKeyboard::findKey(keyCentre(int32_t(i))) == int32_t(i));

  // The plate's corners and the light are no key
  CHECK(VrTerminalKeyboard::findKey({ 0.02f, 0.02f }) == -1);
  CHECK(VrTerminalKeyboard::findKey({ 0.98f, 0.98f }) == -1);

  const VrTexelRect& plate = VrTerminalKeyboard::Plate;
  VrVector2 light = { (VrTerminalKeyboard::LightX - plate.left) / plate.width(),
                      (VrTerminalKeyboard::LightY - plate.top)  / plate.height() };
  CHECK(VrTerminalKeyboard::findKey(light) == -1);
}


TEST_CASE(terminal_keyboard_has_the_console_keys) {
  for (const char* label : { "ESC", "EXIT", "PGUP", "PGDN", "BACK", "DEL", "CAPS", "ENTER",
                             "SHIFT", "HOME", "END", "<", ">", "^", "v" })
    CHECK(findLabel(label) >= 0);

  for (char c : std::string("1234567890-=qwertyuiop[]\\asdfghjkl;'zxcvbnm,./ "))
    CHECK(findChar(c) >= 0);

  CHECK(keyAt(findLabel("ESC")).kind  == VrTerminalKeyKind::Close);
  CHECK(keyAt(findLabel("EXIT")).kind == VrTerminalKeyKind::Close);
  CHECK(keyAt(findLabel("CAPS")).kind == VrTerminalKeyKind::CapsLock);
  CHECK(keyAt(findLabel("SHIFT")).kind == VrTerminalKeyKind::Shift);
  CHECK(VrTerminalKeyboard::code(keyAt(findLabel("^")), false) == VrConsoleKey::Up);
  CHECK(VrTerminalKeyboard::code(keyAt(findLabel("v")), false) == VrConsoleKey::Down);
  CHECK(VrTerminalKeyboard::code(keyAt(findLabel("DEL")), false) == VrConsoleKey::Delete);
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


TEST_CASE(terminal_keyboard_codes_follow_shift_and_caps_lock) {
  const VrTerminalKey& quote = keyAt(findChar('\''));
  CHECK(VrTerminalKeyboard::code(quote, false) == uint32_t('\''));
  CHECK(VrTerminalKeyboard::code(quote, true)  == uint32_t('"'));

  const VrTerminalKey& q   = keyAt(findChar('q'));
  const VrTerminalKey& one = keyAt(findChar('1'));

  // Caps Lock shifts the letters only, Shift every character, both cancel out
  CHECK( VrTerminalKeyboard::isShifted(q,   false, true));
  CHECK(!VrTerminalKeyboard::isShifted(one, false, true));
  CHECK( VrTerminalKeyboard::isShifted(one, true,  false));
  CHECK(!VrTerminalKeyboard::isShifted(q,   true,  true));
  CHECK(VrTerminalKeyboard::code(one, true) == uint32_t('!'));

  const VrTerminalKey& back = keyAt(findLabel("BACK"));
  CHECK(VrTerminalKeyboard::code(back, false) == VrConsoleKey::Backspace);
  CHECK(VrTerminalKeyboard::code(back, true)  == VrConsoleKey::Backspace);
  CHECK(VrTerminalKeyboard::code(keyAt(findChar(' ')), true) == uint32_t(' '));
}


TEST_CASE(dds_decodes_dxt1) {
  // One block: white and black end points; the first pixel takes white,
  // the second black, the third white again, the others the darker of
  // the two colours between them
  std::vector<uint8_t> block = { 0xFF, 0xFF, 0x00, 0x00, 0x04, 0xFF, 0xFF, 0xFF };
  std::vector<uint8_t> file = dxtFile("DXT1", block);

  VrImage image;
  CHECK(vrDecodeDds(file.data(), file.size(), image));
  CHECK(image.width == 4 && image.height == 4);
  CHECK(image.at(0, 0) == 0xFFFFFFFFu);
  CHECK(image.at(1, 0) == 0xFF000000u);
  CHECK(image.at(2, 0) == 0xFFFFFFFFu);
  CHECK(image.at(3, 3) == 0xFF555555u);
}


TEST_CASE(dds_decodes_dxt5_alpha) {
  // Alpha end points 255 and 0, every index 1 (the second end point),
  // then an all-white colour block
  std::vector<uint8_t> block = { 0xFF, 0x00, 0x49, 0x92, 0x24, 0x49, 0x92, 0x24,
                                 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00 };
  std::vector<uint8_t> file = dxtFile("DXT5", block);

  VrImage image;
  CHECK(vrDecodeDds(file.data(), file.size(), image));
  CHECK(image.at(0, 0) == 0x00FFFFFFu);
  CHECK(image.at(3, 2) == 0x00FFFFFFu);
}


TEST_CASE(dds_decodes_a4r4g4b4) {
  std::vector<uint8_t> file = ddsHeader(2, 1);
  put32(file, 80, 0x41);
  put32(file, 88, 16);
  put32(file, 92,  0x0F00);
  put32(file, 96,  0x00F0);
  put32(file, 100, 0x000F);
  put32(file, 104, 0xF000);

  for (uint8_t byte : { uint8_t(0x00), uint8_t(0xF0), uint8_t(0x5F), uint8_t(0x0A) })
    file.push_back(byte);

  VrImage image;
  CHECK(vrDecodeDds(file.data(), file.size(), image));
  CHECK(image.at(0, 0) == 0xFF000000u);
  CHECK(image.at(1, 0) == 0x00AA55FFu);
}


TEST_CASE(dds_rejects_other_files) {
  std::vector<uint8_t> file = dxtFile("ATI2", std::vector<uint8_t>(16, 0));
  VrImage image;
  CHECK(!vrDecodeDds(file.data(), file.size(), image));
  CHECK(image.pixels.empty());

  // Too short for its block
  file = dxtFile("DXT1", std::vector<uint8_t>(4, 0));
  CHECK(!vrDecodeDds(file.data(), file.size(), image));

  const uint8_t text[] = "not a texture";
  CHECK(!vrDecodeDds(text, sizeof(text), image));
}


TEST_CASE(bitmap_font_reads_glyph_metrics) {
  std::vector<uint8_t> file = fontFile(14.0f, 20.0f);
  VrBitmapFont font;
  CHECK(font.parse(file.data(), file.size()));
  CHECK_NEAR(font.lineHeight(), 25.0f, 1e-6);

  const VrGlyph& a = font.glyph('A');
  CHECK_NEAR(a.width, 14.0f, 1e-6);
  CHECK_NEAR(a.height, 20.0f, 1e-6);
  CHECK_NEAR(a.advance(), 17.0f, 1e-6);
  CHECK_NEAR(a.u1, 0.5f, 1e-6);
  CHECK_NEAR(a.ascent, 18.0f, 1e-6);

  // A file of another size is not a font of one texture
  file.pop_back();
  CHECK(!font.parse(file.data(), file.size()));
}


TEST_CASE(terminal_image_draws_only_what_changed) {
  std::vector<uint8_t> file = fontFile(14.0f, 20.0f);
  VrBitmapFont font;
  font.parse(file.data(), file.size());

  VrTerminalImage image;
  CHECK(image.initialize(terminalTexture(), font, solidGlyphs()));

  VrTerminalKeyboardView view;
  CHECK(image.update(view));

  constexpr uint32_t Width  = VrTerminalImage::Width;
  constexpr uint32_t Height = VrTerminalImage::Height;
  std::vector<uint32_t> first(image.pixels(), image.pixels() + Width * Height);

  // The black edge of the plate is transparent, its middle opaque
  CHECK((first[0] >> 24) == 0);
  CHECK((first[(Height / 2) * Width + Width / 2] >> 24) == 0xFF);

  // Nothing changed, nothing is drawn
  CHECK(!image.update(view));

  // The hovered key, a pressed key and Shift change the image
  view.hoveredKey = findChar('q');
  CHECK(image.update(view));
  CHECK(differs(image.pixels(), first));

  view.hoveredKey = -1;
  CHECK(image.update(view));
  CHECK(!differs(image.pixels(), first));

  view.pressedKey = findLabel("ENTER");
  CHECK(image.update(view));
  CHECK(differs(image.pixels(), first));

  view.pressedKey = -1;
  view.shift = true;
  CHECK(image.update(view));
  CHECK(differs(image.pixels(), first));

  view.shift = false;
  CHECK(image.update(view));
  CHECK(!differs(image.pixels(), first));
}


TEST_CASE(terminal_image_needs_the_whole_plate) {
  VrImage small;
  small.width  = 256;
  small.height = 256;
  small.pixels.assign(256 * 256, 0xFF808080u);

  std::vector<uint8_t> file = fontFile(14.0f, 20.0f);
  VrBitmapFont font;
  font.parse(file.data(), file.size());

  VrTerminalImage image;
  CHECK(!image.initialize(small, font, solidGlyphs()));
  CHECK(!image.update(VrTerminalKeyboardView()));
}


TEST_CASE(terminal_texture_lays_the_keys_on_the_enlarged_texture) {
  std::vector<uint8_t> file = fontFile(14.0f, 20.0f);
  VrBitmapFont font;
  font.parse(file.data(), file.size());

  // A texture whose alpha the keyboard must keep
  VrImage texture = terminalTexture();

  for (uint32_t& texel : texture.pixels)
    texel = (texel & 0x00FFFFFFu) | 0x80000000u;

  VrTerminalImage image;
  CHECK(image.initialize(terminalTexture(), font, solidGlyphs()));

  VrTerminalKeyboardView view;
  view.hoveredKey = findChar('q');
  image.update(view);

  VrTerminalTexture copy;
  CHECK(copy.initialize(texture));
  CHECK(copy.width()  == 512 * VrTerminalTexture::Scale);
  CHECK(copy.height() == 512 * VrTerminalTexture::Scale);

  std::vector<uint32_t> base;
  copy.drawBase(base);
  CHECK(base.size() == size_t(copy.width()) * copy.height());

  // The middle of a texel keeps its colour
  constexpr uint32_t Scale = VrTerminalTexture::Scale;
  uint32_t texelX = 100, texelY = 340;
  CHECK(base[size_t(texelY * Scale + Scale / 2) * copy.width() + texelX * Scale + Scale / 2]
    == texture.at(texelX, texelY));

  std::vector<uint32_t> keyTops(size_t(VrTerminalTexture::KeyTopsWidth) * VrTerminalTexture::KeyTopsHeight);
  copy.drawKeyTops(image, keyTops.data());

  // On the lit key the keyboard's colours, with the texture's alpha
  const VrTerminalKey& key = keyAt(findChar('q'));
  uint32_t x = uint32_t(0.5f * (key.rect.left + key.rect.right)) * Scale;
  uint32_t y = uint32_t(0.5f * (key.rect.top + key.rect.bottom)) * Scale;

  uint32_t drawn = keyTops[size_t(y - VrTerminalTexture::KeyTopsY) * VrTerminalTexture::KeyTopsWidth
    + x - VrTerminalTexture::KeyTopsX];
  uint32_t keyboard = image.pixels()[size_t(y - uint32_t(VrTerminalKeyboard::Plate.top) * Scale) * VrTerminalImage::Width
    + x - uint32_t(VrTerminalKeyboard::Plate.left) * Scale];

  CHECK((drawn >> 24) == 0x80);
  CHECK((drawn & 0x00FFFFFFu) == (keyboard & 0x00FFFFFFu));
  CHECK((drawn & 0x00FFFFFFu) != (texture.at(x / Scale, y / Scale) & 0x00FFFFFFu));
}


TEST_CASE(terminal_texture_needs_the_whole_plate) {
  VrImage small;
  small.width  = 256;
  small.height = 256;
  small.pixels.assign(256 * 256, 0xFF808080u);

  VrTerminalTexture copy;
  CHECK(!copy.initialize(small));
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


TEST_CASE(terminal_command_follows_the_prompt) {
  std::string text;
  uint32_t column = 0;

  vrLayoutTerminalCommand("tgm", 3, 44, text, column);
  CHECK(text == "> tgm");
  CHECK(column == 5);

  vrLayoutTerminalCommand("tgm", 1, 44, text, column);
  CHECK(column == 3);

  // A cursor past the end stands after the last character
  vrLayoutTerminalCommand("tgm", 9, 44, text, column);
  CHECK(column == 5);
}


TEST_CASE(terminal_command_scrolls_to_the_cursor) {
  std::string input(60, 'a');
  input[59] = 'z';

  std::string text;
  uint32_t column = 0;

  vrLayoutTerminalCommand(input, 60, 44, text, column);
  CHECK(text.size() <= 44);
  CHECK(text.substr(0, 2) == "> ");
  CHECK(text.back() == 'z');
  CHECK(column < 44);
  CHECK(column == text.size());

  // At the start, the beginning of the command is shown
  vrLayoutTerminalCommand(input, 0, 44, text, column);
  CHECK(column == 2);
  CHECK(text.back() == 'a');
}
