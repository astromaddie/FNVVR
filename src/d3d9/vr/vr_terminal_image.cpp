#include <algorithm>
#include <cmath>
#include <iterator>

#include "vr_terminal_font.h"
#include "vr_terminal_image.h"
#include "vr_terminal_keyboard.h"

namespace dxvk {

  namespace {

    constexpr int32_t Width  = int32_t(VrTerminalLayout::ImageWidth);
    constexpr int32_t Height = int32_t(VrTerminalLayout::ImageHeight);

    // The case fills the image, with rounded corners
    constexpr float CaseRadius = 40.0f;

    // The screen, in pixels of the image, sunk in a dark groove
    constexpr int32_t ScreenX      = 48;
    constexpr int32_t ScreenY      = 40;
    constexpr int32_t ScreenWidth  = 928;
    constexpr int32_t ScreenHeight = 568;
    constexpr float   ScreenRadius = 32.0f;
    constexpr float   GrooveWidth  = 10.0f;

    // Text cells, centred on the screen
    constexpr int32_t GlyphWidth  = int32_t(VrTerminalFont::GlyphWidth);
    constexpr int32_t GlyphHeight = int32_t(VrTerminalFont::GlyphHeight);
    constexpr int32_t LineHeight  = 22;
    constexpr int32_t TextWidth   = int32_t(VrTerminalLayout::Columns) * GlyphWidth;
    constexpr int32_t TextHeight  = int32_t(VrTerminalLayout::Lines) * LineHeight;
    constexpr int32_t TextX       = ScreenX + (ScreenWidth  - TextWidth)  / 2;
    constexpr int32_t TextY       = ScreenY + (ScreenHeight - TextHeight) / 2;

    // Pixels the glow of the text spreads over
    constexpr int32_t GlowRadius  = 2;

    // The keyboard below the screen, in a well
    constexpr int32_t KeyboardX      = 48;
    constexpr int32_t KeyboardY      = 640;
    constexpr int32_t KeyboardWidth  = 928;
    constexpr float   KeyUnit        = float(KeyboardWidth) / VrTerminalKeyboard::Width;
    constexpr float   RowHeight      = 72.0f;
    constexpr float   WellMargin     = 10.0f;
    constexpr float   WellRadius     = 18.0f;

    // Each key is drawn inside its cell: the hover outline, the key's
    // darker side under its face, and the face, which sinks when pressed
    constexpr float KeyInset       = 4.0f;
    constexpr float KeyRadius      = 9.0f;
    constexpr float OutlineWidth   = 3.0f;
    constexpr float FaceInset      = 2.0f;
    constexpr float KeySide        = 7.0f;
    constexpr float PressedSink    = 4.0f;

    struct Color {
      float r, g, b;
    };

    // Metal of the terminal's case, lit from above
    constexpr Color CaseTop      = {  70.0f,  74.0f,  58.0f };
    constexpr Color CaseBottom   = {  46.0f,  49.0f,  39.0f };
    constexpr Color CaseEdge     = {  30.0f,  32.0f,  26.0f };
    constexpr Color Groove       = {  22.0f,  24.0f,  19.0f };
    constexpr Color Well         = {  32.0f,  34.0f,  28.0f };

    // Glass of the screen, brighter at the centre, and its phosphor
    constexpr Color GlassCentre  = {  12.0f,  34.0f,  21.0f };
    constexpr Color GlassEdge    = {   2.0f,   8.0f,   5.0f };
    constexpr Color Phosphor     = {  70.0f, 255.0f, 140.0f };
    constexpr float GlowStrength = 0.6f;
    // Every third row of the screen is darker, as a scan line
    constexpr float ScanLine     = 0.7f;

    // Beige keys, as on the terminals in the world
    constexpr Color KeyFace      = { 190.0f, 180.0f, 148.0f };
    constexpr Color KeyHovered   = { 222.0f, 214.0f, 180.0f };
    constexpr Color KeyPressed   = { 150.0f, 142.0f, 116.0f };
    constexpr Color KeyLit       = { 150.0f, 226.0f, 170.0f };
    constexpr Color ExitFace     = { 172.0f,  92.0f,  70.0f };
    constexpr Color ExitHovered  = { 204.0f, 114.0f,  88.0f };
    constexpr float KeySideShade = 0.68f;
    constexpr Color KeyLabel     = {  44.0f,  42.0f,  34.0f };

    constexpr const char* HeaderText[] = {
      "ROBCO INDUSTRIES (TM) TERMLINK PROTOCOL",
      "ENTER COMMAND NOW",
    };

    constexpr char Prompt[] = "> ";

    Color mix(const Color& a, const Color& b, float t) {
      return { a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t };
    }

    Color scale(const Color& c, float f) {
      return { c.r * f, c.g * f, c.b * f };
    }

    uint32_t channel(float value) {
      return uint32_t(std::clamp(std::lround(value), 0l, 255l));
    }

    uint32_t pack(const Color& c, float alpha) {
      return (channel(alpha * 255.0f) << 24) | (channel(c.r) << 16) | (channel(c.g) << 8) | channel(c.b);
    }

    Color unpack(uint32_t pixel) {
      return { float((pixel >> 16) & 0xFF), float((pixel >> 8) & 0xFF), float(pixel & 0xFF) };
    }

    // Puts a colour over a pixel with the given coverage
    void blend(uint32_t& pixel, const Color& c, float coverage) {
      if (coverage <= 0.0f)
        return;

      float below = float(pixel >> 24) / 255.0f;
      float alpha = coverage + below * (1.0f - coverage);

      if (alpha <= 0.0f)
        return;

      // Straight alpha: the colours are weighted by their share of the result
      Color under = unpack(pixel);
      float keep  = below * (1.0f - coverage) / alpha;
      float put   = coverage / alpha;

      pixel = pack({ under.r * keep + c.r * put,
                     under.g * keep + c.g * put,
                     under.b * keep + c.b * put }, alpha);
    }

    // Signed distance from a pixel's centre to a rounded rectangle,
    // negative inside
    float roundedDistance(int32_t x, int32_t y, float x0, float y0, float x1, float y1, float radius) {
      float px = float(x) + 0.5f;
      float py = float(y) + 0.5f;
      float qx = std::abs(px - 0.5f * (x0 + x1)) - (0.5f * (x1 - x0) - radius);
      float qy = std::abs(py - 0.5f * (y0 + y1)) - (0.5f * (y1 - y0) - radius);
      float ox = std::max(qx, 0.0f);
      float oy = std::max(qy, 0.0f);
      return std::sqrt(ox * ox + oy * oy) + std::min(std::max(qx, qy), 0.0f) - radius;
    }

    float coverage(float distance) {
      return std::clamp(0.5f - distance, 0.0f, 1.0f);
    }

    void fillRounded(std::vector<uint32_t>& pixels, float x0, float y0, float x1, float y1,
        float radius, const Color& color) {
      int32_t left   = std::max(int32_t(std::floor(x0)), 0);
      int32_t top    = std::max(int32_t(std::floor(y0)), 0);
      int32_t right  = std::min(int32_t(std::ceil(x1)), Width);
      int32_t bottom = std::min(int32_t(std::ceil(y1)), Height);

      for (int32_t y = top; y < bottom; y++) {
        for (int32_t x = left; x < right; x++)
          blend(pixels[y * Width + x], color, coverage(roundedDistance(x, y, x0, y0, x1, y1, radius)));
      }
    }

    float scanLine(int32_t y) {
      return (y - ScreenY) % 3 == 2 ? ScanLine : 1.0f;
    }

    // Draws text with each glyph pixel scaled to a square
    void drawLabel(std::vector<uint32_t>& pixels, const char* text, float centreX, float centreY,
        int32_t scale, const Color& color) {
      int32_t length = int32_t(std::char_traits<char>::length(text));
      int32_t left = int32_t(std::lround(centreX - 0.5f * float(length * GlyphWidth * scale)));
      int32_t top  = int32_t(std::lround(centreY - 0.5f * float(GlyphHeight * scale)));

      for (int32_t i = 0; i < length; i++) {
        const uint16_t* glyph = vrTerminalGlyph(text[i]);

        for (int32_t gy = 0; gy < GlyphHeight; gy++) {
          for (int32_t gx = 0; gx < GlyphWidth; gx++) {
            if (!(glyph[gy] & (1u << (GlyphWidth - 1 - gx))))
              continue;

            for (int32_t sy = 0; sy < scale; sy++) {
              for (int32_t sx = 0; sx < scale; sx++) {
                int32_t x = left + (i * GlyphWidth + gx) * scale + sx;
                int32_t y = top + gy * scale + sy;

                if (x >= 0 && x < Width && y >= 0 && y < Height)
                  blend(pixels[y * Width + x], color, 1.0f);
              }
            }
          }
        }
      }
    }

    struct KeyCell {
      float x0, y0, x1, y1;
    };

    KeyCell keyCell(const VrTerminalKey& key) {
      KeyCell cell;
      cell.x0 = float(KeyboardX) + key.x * KeyUnit;
      cell.y0 = float(KeyboardY) + float(key.row) * RowHeight;
      cell.x1 = cell.x0 + key.width * KeyUnit;
      cell.y1 = cell.y0 + RowHeight;
      return cell;
    }

  }


  int32_t VrTerminalLayout::findKey(VrVector2 uv) {
    float x = (uv.x * float(ImageWidth)  - float(KeyboardX)) / KeyUnit;
    float y = (uv.y * float(ImageHeight) - float(KeyboardY)) / RowHeight;
    return VrTerminalKeyboard::findKey(x, y);
  }


  std::vector<std::string> vrWrapTerminalLines(
    const std::vector<std::string>& lines,
          uint32_t                  columns,
          uint32_t                  rows) {
    std::vector<std::string> wrapped;

    if (!columns)
      return wrapped;

    for (std::string line : lines) {
      while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
        line.pop_back();

      std::replace(line.begin(), line.end(), '\t', ' ');

      while (line.size() > columns) {
        size_t space = line.rfind(' ', columns);

        if (space != std::string::npos && space > 0) {
          wrapped.push_back(line.substr(0, space));
          line.erase(0, space + 1);
        } else {
          wrapped.push_back(line.substr(0, columns));
          line.erase(0, columns);
        }
      }

      wrapped.push_back(line);
    }

    if (wrapped.size() > rows)
      wrapped.erase(wrapped.begin(), wrapped.end() - rows);

    return wrapped;
  }


  VrTerminalImage::VrTerminalImage()
  : m_base  (size_t(Width) * size_t(Height)),
    m_mask  (size_t(ScreenWidth) * size_t(ScreenHeight)),
    m_blur  (size_t(ScreenWidth) * size_t(ScreenHeight)),
    m_lines (VrTerminalLayout::Lines) {
    drawBase();
    m_pixels = m_base;
  }


  bool VrTerminalImage::update(const VrTerminalView& view) {
    uint32_t count = 0;
    const VrTerminalKey* keys = VrTerminalKeyboard::keys(count);

    if (!m_valid) {
      drawScreen(view);

      for (uint32_t i = 0; i < count; i++)
        drawKey(int32_t(i), view);

      m_drawn = view;
      m_valid = true;
      return true;
    }

    bool changed = false;

    if (view.output != m_drawn.output || view.input != m_drawn.input
     || view.cursor != m_drawn.cursor || view.cursorShown != m_drawn.cursorShown)
      changed = drawScreen(view);

    for (uint32_t i = 0; i < count; i++) {
      int32_t index = int32_t(i);

      bool look = (index == view.hoveredKey) != (index == m_drawn.hoveredKey)
               || (index == view.pressedKey) != (index == m_drawn.pressedKey);

      // Shift changes the shifted keys' labels and lights the Shift keys
      if (view.shift != m_drawn.shift)
        look |= keys[i].kind == VrTerminalKeyKind::Char || keys[i].kind == VrTerminalKeyKind::Shift;

      if (look) {
        drawKey(index, view);
        changed = true;
      }
    }

    m_drawn = view;
    return changed;
  }


  void VrTerminalImage::drawBase() {
    std::fill(m_base.begin(), m_base.end(), 0u);

    // The case, with a darker rim
    for (int32_t y = 0; y < Height; y++) {
      Color metal = mix(CaseTop, CaseBottom, float(y) / float(Height - 1));

      for (int32_t x = 0; x < Width; x++) {
        float distance = roundedDistance(x, y, 0.0f, 0.0f, float(Width), float(Height), CaseRadius);
        Color color = mix(metal, CaseEdge, std::clamp(distance + 3.0f, 0.0f, 1.0f));
        blend(m_base[y * Width + x], color, coverage(distance));
      }
    }

    // The screen sits in a groove of the case
    fillRounded(m_base, float(ScreenX) - GrooveWidth, float(ScreenY) - GrooveWidth,
      float(ScreenX + ScreenWidth) + GrooveWidth, float(ScreenY + ScreenHeight) + GrooveWidth,
      ScreenRadius + GrooveWidth, Groove);

    float halfWidth  = 0.5f * float(ScreenWidth);
    float halfHeight = 0.5f * float(ScreenHeight);

    for (int32_t y = ScreenY; y < ScreenY + ScreenHeight; y++) {
      for (int32_t x = ScreenX; x < ScreenX + ScreenWidth; x++) {
        float nx = (float(x) + 0.5f - float(ScreenX) - halfWidth)  / halfWidth;
        float ny = (float(y) + 0.5f - float(ScreenY) - halfHeight) / halfHeight;
        float edge = std::clamp(0.5f * (nx * nx + ny * ny), 0.0f, 1.0f);

        Color glass = scale(mix(GlassCentre, GlassEdge, edge * edge * (3.0f - 2.0f * edge)), scanLine(y));
        float cover = coverage(roundedDistance(x, y, float(ScreenX), float(ScreenY),
          float(ScreenX + ScreenWidth), float(ScreenY + ScreenHeight), ScreenRadius));

        blend(m_base[y * Width + x], glass, cover);
      }
    }

    // The keyboard sits in a well
    float rows = float(VrTerminalKeyboard::Rows) * RowHeight;

    fillRounded(m_base, float(KeyboardX) - WellMargin, float(KeyboardY) - WellMargin,
      float(KeyboardX + KeyboardWidth) + WellMargin, float(KeyboardY) + rows + WellMargin,
      WellRadius, Well);
  }


  bool VrTerminalImage::drawScreen(const VrTerminalView& view) {
    constexpr uint32_t PromptLength = uint32_t(sizeof(Prompt) - 1);
    constexpr uint32_t InputColumns = VrTerminalLayout::Columns - PromptLength;
    constexpr uint32_t InputLine    = VrTerminalLayout::Lines - 1;

    std::vector<std::string> lines(VrTerminalLayout::Lines);

    for (uint32_t i = 0; i < std::size(HeaderText); i++)
      lines[i] = HeaderText[i];

    std::vector<std::string> output = vrWrapTerminalLines(view.output,
      VrTerminalLayout::Columns, VrTerminalLayout::OutputLines);

    for (uint32_t i = 0; i < output.size(); i++)
      lines[VrTerminalLayout::HeaderLines + i] = std::move(output[i]);

    // The command line scrolls to keep the cursor in view
    uint32_t cursor = std::min(view.cursor, uint32_t(view.input.size()));
    uint32_t start  = cursor >= InputColumns ? cursor - InputColumns + 1 : 0;

    lines[InputLine] = Prompt + view.input.substr(start, InputColumns);

    int32_t cursorColumn = view.cursorShown ? int32_t(PromptLength + cursor - start) : -1;

    // Only the lines that changed are drawn again, so that the cursor's
    // blinking costs one line
    bool changed = false;

    for (uint32_t i = 0; i < VrTerminalLayout::Lines; i++) {
      bool cursorMoved = i == InputLine && cursorColumn != m_cursorColumn;

      if (m_valid && lines[i] == m_lines[i] && !cursorMoved)
        continue;

      int32_t top = TextY - ScreenY + int32_t(i) * LineHeight;

      for (int32_t y = top; y < top + LineHeight; y++)
        std::fill_n(&m_mask[y * ScreenWidth + TextX - ScreenX], TextWidth, uint8_t(0));

      printLine(i, 0, lines[i]);

      if (i == InputLine && cursorColumn >= 0)
        invertCell(i, uint32_t(cursorColumn));

      composeLine(i);
      changed = true;
    }

    m_lines        = std::move(lines);
    m_cursorColumn = cursorColumn;
    return changed;
  }


  void VrTerminalImage::composeLine(uint32_t line) {
    // The glow is the text blurred over a square, and reaches into the
    // neighbouring lines' rows, whose text is still in the mask
    constexpr int32_t Left  = TextX - ScreenX - GlowRadius;
    constexpr int32_t Right = TextX - ScreenX + TextWidth + GlowRadius;

    int32_t top    = TextY - ScreenY + int32_t(line) * LineHeight - GlowRadius;
    int32_t bottom = top + LineHeight + 2 * GlowRadius;

    for (int32_t y = std::max(top - GlowRadius, 0); y < std::min(bottom + GlowRadius, ScreenHeight); y++) {
      for (int32_t x = Left; x < Right; x++) {
        uint32_t sum = 0;

        for (int32_t d = -GlowRadius; d <= GlowRadius; d++)
          sum += m_mask[y * ScreenWidth + x + d];

        m_blur[y * ScreenWidth + x] = uint16_t(sum);
      }
    }

    constexpr float GlowArea = float((2 * GlowRadius + 1) * (2 * GlowRadius + 1)) * 255.0f;

    for (int32_t y = top; y < bottom; y++) {
      int32_t imageY = ScreenY + y;
      float scan = scanLine(imageY);

      for (int32_t x = Left; x < Right; x++) {
        uint32_t glow = 0;

        for (int32_t d = -GlowRadius; d <= GlowRadius; d++)
          glow += m_blur[(y + d) * ScreenWidth + x];

        float light = scan * (float(m_mask[y * ScreenWidth + x]) / 255.0f
                    + GlowStrength * float(glow) / GlowArea);

        size_t index = size_t(imageY) * size_t(Width) + size_t(ScreenX + x);

        if (light <= 0.0f) {
          m_pixels[index] = m_base[index];
          continue;
        }

        Color glass = unpack(m_base[index]);

        m_pixels[index] = pack({ glass.r + Phosphor.r * light,
                                 glass.g + Phosphor.g * light,
                                 glass.b + Phosphor.b * light }, float(m_base[index] >> 24) / 255.0f);
      }
    }
  }


  void VrTerminalImage::drawKey(int32_t index, const VrTerminalView& view) {
    uint32_t count = 0;
    const VrTerminalKey& key = VrTerminalKeyboard::keys(count)[index];
    KeyCell cell = keyCell(key);

    int32_t left   = int32_t(std::floor(cell.x0));
    int32_t top    = int32_t(std::floor(cell.y0));
    int32_t right  = std::min(int32_t(std::ceil(cell.x1)), Width);
    int32_t bottom = std::min(int32_t(std::ceil(cell.y1)), Height);

    for (int32_t y = top; y < bottom; y++)
      std::copy_n(&m_base[y * Width + left], right - left, &m_pixels[y * Width + left]);

    bool hovered = index == view.hoveredKey;
    bool pressed = index == view.pressedKey;
    bool exit    = key.kind == VrTerminalKeyKind::Close;
    bool lit     = key.kind == VrTerminalKeyKind::Shift && view.shift;

    float x0 = cell.x0 + KeyInset;
    float y0 = cell.y0 + KeyInset;
    float x1 = cell.x1 - KeyInset;
    float y1 = cell.y1 - KeyInset;

    if (hovered) {
      fillRounded(m_pixels, x0 - OutlineWidth, y0 - OutlineWidth, x1 + OutlineWidth, y1 + OutlineWidth,
        KeyRadius + OutlineWidth, Phosphor);
    }

    Color face = exit ? (hovered ? ExitHovered : ExitFace)
               : lit  ? KeyLit
               : hovered ? KeyHovered
               : KeyFace;

    if (pressed)
      face = exit ? ExitFace : KeyPressed;

    float sink = pressed ? PressedSink : 0.0f;

    fillRounded(m_pixels, x0, y0 + sink, x1, y1, KeyRadius, scale(face, KeySideShade));
    fillRounded(m_pixels, x0 + FaceInset, y0 + sink, x1 - FaceInset, y1 - KeySide + sink * 0.5f,
      KeyRadius - FaceInset, face);

    char character[2] = { key.label ? '\0' : (view.shift ? key.upper : key.lower), '\0' };
    const char* text = key.label ? key.label : character;

    // Labels are drawn twice as large as the screen's text when they fit
    float faceWidth = x1 - x0 - 2.0f * FaceInset;
    int32_t length = int32_t(std::char_traits<char>::length(text));
    int32_t labelScale = float(length * GlyphWidth * 2) <= faceWidth ? 2 : 1;

    drawLabel(m_pixels, text, 0.5f * (x0 + x1), 0.5f * (y0 + y1 - KeySide) + sink,
      labelScale, exit ? KeyFace : KeyLabel);
  }


  void VrTerminalImage::printLine(uint32_t line, uint32_t column, const std::string& text) {
    int32_t top = TextY - ScreenY + int32_t(line) * LineHeight + (LineHeight - GlyphHeight) / 2;

    for (uint32_t i = 0; i < text.size() && column + i < VrTerminalLayout::Columns; i++) {
      const uint16_t* glyph = vrTerminalGlyph(text[i]);
      int32_t left = TextX - ScreenX + int32_t(column + i) * GlyphWidth;

      for (int32_t gy = 0; gy < GlyphHeight; gy++) {
        for (int32_t gx = 0; gx < GlyphWidth; gx++) {
          if (glyph[gy] & (1u << (GlyphWidth - 1 - gx)))
            m_mask[(top + gy) * ScreenWidth + left + gx] = 0xFF;
        }
      }
    }
  }


  void VrTerminalImage::invertCell(uint32_t line, uint32_t column) {
    if (column >= VrTerminalLayout::Columns)
      return;

    int32_t top  = TextY - ScreenY + int32_t(line) * LineHeight + (LineHeight - GlyphHeight) / 2;
    int32_t left = TextX - ScreenX + int32_t(column) * GlyphWidth;

    for (int32_t y = top; y < top + GlyphHeight; y++) {
      for (int32_t x = left; x < left + GlyphWidth; x++)
        m_mask[y * ScreenWidth + x] = uint8_t(0xFF - m_mask[y * ScreenWidth + x]);
    }
  }

}
