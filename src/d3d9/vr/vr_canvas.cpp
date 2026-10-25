#include <algorithm>
#include <cmath>

#include "vr_canvas.h"

namespace dxvk {

  namespace {

    constexpr float TwoPi = 6.2831853f;

    // Sub-samples per axis at most when an image is drawn smaller
    constexpr int32_t MaxSubSamples = 4;

    float channel(uint32_t pixel, uint32_t shift) {
      return float((pixel >> shift) & 0xFF) / 255.0f;
    }

    // How much of a shape covers a point: its alpha times its brightness.
    // A shape drawn smaller averages several of these per pixel, from the
    // nearest texel (a box filter); one drawn larger blends between texels.
    float texelCoverage(const VrImage& image, float u, float v) {
      // Points are at least -0.5 texel from the image: truncating rounds
      uint32_t x = std::min(uint32_t(u + 0.5f), image.width  - 1);
      uint32_t y = std::min(uint32_t(v + 0.5f), image.height - 1);
      uint32_t texel = image.at(x, y);

      return channel(texel, 24) * std::max({ channel(texel, 16), channel(texel, 8), channel(texel, 0) });
    }

    float sampledCoverage(const VrImage& image, float u, float v) {
      float alpha  = vrSampleChannel(image, u, v, 24) / 255.0f;
      float bright = std::max({ vrSampleChannel(image, u, v, 16), vrSampleChannel(image, u, v, 8),
        vrSampleChannel(image, u, v, 0) }) / 255.0f;
      return alpha * bright;
    }

    uint32_t pack(float a, float r, float g, float b) {
      auto byte = [] (float v) {
        return uint32_t(std::lround(std::clamp(v, 0.0f, 1.0f) * 255.0f));
      };

      return (byte(a) << 24) | (byte(r) << 16) | (byte(g) << 8) | byte(b);
    }

    float coverageOf(float distanceInside) {
      return std::clamp(distanceInside + 0.5f, 0.0f, 1.0f);
    }

  }


  VrColour VrColour::fromRgba(uint32_t rgba) {
    return { float((rgba >> 24) & 0xFF) / 255.0f, float((rgba >> 16) & 0xFF) / 255.0f,
      float((rgba >> 8) & 0xFF) / 255.0f, float(rgba & 0xFF) / 255.0f };
  }


  VrCanvas::VrCanvas(uint32_t width, uint32_t height)
  : m_width(width), m_height(height), m_pixels(size_t(width) * height, 0u) { }


  void VrCanvas::clear() {
    std::fill(m_pixels.begin(), m_pixels.end(), 0u);
  }


  void VrCanvas::blend(uint32_t x, uint32_t y, const VrColour& colour, float coverage) {
    float a = std::clamp(colour.a * coverage, 0.0f, 1.0f);

    if (a <= 0.0f)
      return;

    uint32_t& pixel = m_pixels[size_t(y) * m_width + x];

    float da = channel(pixel, 24);
    float oa = a + da * (1.0f - a);

    auto mix = [&] (float source, uint32_t shift) {
      return (source * a + channel(pixel, shift) * da * (1.0f - a)) / oa;
    };

    pixel = pack(oa, mix(colour.r, 16), mix(colour.g, 8), mix(colour.b, 0));
  }


  void VrCanvas::fillRoundedRect(float left, float top, float right, float bottom, float radius,
      const VrColour& colour) {
    radius = std::clamp(radius, 0.0f, 0.5f * std::min(right - left, bottom - top));

    int32_t x0 = std::max(int32_t(std::floor(left)), 0);
    int32_t y0 = std::max(int32_t(std::floor(top)), 0);
    int32_t x1 = std::min(int32_t(std::ceil(right)), int32_t(m_width));
    int32_t y1 = std::min(int32_t(std::ceil(bottom)), int32_t(m_height));

    for (int32_t y = y0; y < y1; y++) {
      for (int32_t x = x0; x < x1; x++) {
        // Distance outside the rectangle shrunk by the radius
        float px = float(x) + 0.5f;
        float py = float(y) + 0.5f;
        float dx = std::max({ left + radius - px, px - (right - radius), 0.0f });
        float dy = std::max({ top + radius - py, py - (bottom - radius), 0.0f });

        blend(uint32_t(x), uint32_t(y), colour, coverageOf(radius - std::sqrt(dx * dx + dy * dy)));
      }
    }
  }


  void VrCanvas::fillSector(float cx, float cy, float inner, float outer, float start, float end, const VrColour& colour) {
    float span = end - start;

    if (span <= 0.0f || outer <= inner)
      return;

    bool full = span >= TwoPi;

    // Bounds of the sector: the corners of its straight edges, and the
    // outer circle's extremes within its angles
    float left = cx - outer, right = cx + outer, top = cy - outer, bottom = cy + outer;

    if (!full) {
      left = right = cx + std::sin(start) * inner;
      top = bottom = cy - std::cos(start) * inner;

      auto include = [&] (float radius, float angle) {
        float x = cx + std::sin(angle) * radius;
        float y = cy - std::cos(angle) * radius;
        left   = std::min(left, x);
        right  = std::max(right, x);
        top    = std::min(top, y);
        bottom = std::max(bottom, y);
      };

      include(inner, end);
      include(outer, start);
      include(outer, end);

      constexpr float QuarterTurn = 0.25f * TwoPi;

      for (float axis = QuarterTurn * std::ceil(start / QuarterTurn); axis <= end; axis += QuarterTurn)
        include(outer, axis);
    }

    int32_t x0 = std::max(int32_t(std::floor(left - 1.0f)), 0);
    int32_t y0 = std::max(int32_t(std::floor(top - 1.0f)), 0);
    int32_t x1 = std::min(int32_t(std::ceil(right + 1.0f)), int32_t(m_width));
    int32_t y1 = std::min(int32_t(std::ceil(bottom + 1.0f)), int32_t(m_height));

    for (int32_t y = y0; y < y1; y++) {
      for (int32_t x = x0; x < x1; x++) {
        float dx = float(x) + 0.5f - cx;
        float dy = float(y) + 0.5f - cy;
        float distance = std::sqrt(dx * dx + dy * dy);

        float coverage = coverageOf(outer - distance);

        if (inner > 0.0f)
          coverage *= coverageOf(distance - inner);

        if (coverage <= 0.0f)
          continue;

        if (!full) {
          // Clockwise from the top: the image's Y axis points down
          float angle = std::atan2(dx, -dy) - start;
          angle -= TwoPi * std::floor(angle / TwoPi);

          // Distance to the nearest straight edge, in pixels along the arc
          float inside = angle <= span
            ? std::min(angle, span - angle) * distance
            : -std::min(angle - span, TwoPi - angle) * distance;

          coverage *= coverageOf(inside);
        }

        blend(uint32_t(x), uint32_t(y), colour, coverage);
      }
    }
  }


  void VrCanvas::drawShape(const VrImage& image, float cx, float cy, float size, const VrColour& colour) {
    if (!image.width || !image.height || size <= 0.0f)
      return;

    float scale  = size / float(std::max(image.width, image.height));
    float width  = float(image.width)  * scale;
    float height = float(image.height) * scale;
    float left   = cx - 0.5f * width;
    float top    = cy - 0.5f * height;

    // Each pixel averages the texels it covers
    int32_t samples = std::clamp(int32_t(std::ceil(1.0f / scale)), 1, MaxSubSamples);
    float   step    = 1.0f / float(samples);

    int32_t x0 = std::max(int32_t(std::floor(left)), 0);
    int32_t y0 = std::max(int32_t(std::floor(top)), 0);
    int32_t x1 = std::min(int32_t(std::ceil(left + width)), int32_t(m_width));
    int32_t y1 = std::min(int32_t(std::ceil(top + height)), int32_t(m_height));

    for (int32_t y = y0; y < y1; y++) {
      for (int32_t x = x0; x < x1; x++) {
        float sum = 0.0f;

        for (int32_t sy = 0; sy < samples; sy++) {
          for (int32_t sx = 0; sx < samples; sx++) {
            float u = (float(x) + (float(sx) + 0.5f) * step - left) / scale - 0.5f;
            float v = (float(y) + (float(sy) + 0.5f) * step - top)  / scale - 0.5f;

            if (u < -0.5f || v < -0.5f || u > float(image.width) - 0.5f || v > float(image.height) - 0.5f)
              continue;

            sum += samples > 1 ? texelCoverage(image, u, v) : sampledCoverage(image, u, v);
          }
        }

        blend(uint32_t(x), uint32_t(y), colour, sum / float(samples * samples));
      }
    }
  }


  float VrCanvas::measureText(const VrCanvasFont& font, const std::string& text, float scale) {
    float width = 0.0f;

    for (char c : text)
      width += font.metrics.glyph(c).advance();

    return width * scale;
  }


  void VrCanvas::drawText(const VrCanvasFont& font, const std::string& text, float x, float baseline, float scale,
      const VrColour& colour) {
    float atlasWidth  = float(font.glyphs.width);
    float atlasHeight = float(font.glyphs.height);

    for (char c : text) {
      const VrGlyph& glyph = font.metrics.glyph(c);

      if (glyph.width > 0.0f && glyph.height > 0.0f && font.glyphs.width) {
        float left = x + glyph.before * scale;
        float top  = baseline - glyph.ascent * scale;
        float w    = glyph.width  * scale;
        float h    = glyph.height * scale;

        int32_t x0 = std::max(int32_t(std::floor(left)), 0);
        int32_t y0 = std::max(int32_t(std::floor(top)), 0);
        int32_t x1 = std::min(int32_t(std::ceil(left + w)), int32_t(m_width));
        int32_t y1 = std::min(int32_t(std::ceil(top + h)), int32_t(m_height));

        for (int32_t py = y0; py < y1; py++) {
          for (int32_t px = x0; px < x1; px++) {
            // Position in the glyph, from 0 to 1, then in the font's texture
            float gx = (float(px) + 0.5f - left) / w;
            float gy = (float(py) + 0.5f - top)  / h;

            if (gx < 0.0f || gx > 1.0f || gy < 0.0f || gy > 1.0f)
              continue;

            float u = (glyph.u0 + gx * (glyph.u1 - glyph.u0)) * atlasWidth  - 0.5f;
            float v = (glyph.v0 + gy * (glyph.v1 - glyph.v0)) * atlasHeight - 0.5f;

            blend(uint32_t(px), uint32_t(py), colour, vrSampleChannel(font.glyphs, u, v, 24) / 255.0f);
          }
        }
      }

      x += glyph.advance() * scale;
    }
  }


  void VrCanvas::drawTextBlock(const VrCanvasFont& font, const std::string& text, float cx, float cy, float maxWidth,
      float scale, float minScale, uint32_t maxLines, const VrColour& colour) {
    float width = measureText(font, text, 1.0f);

    if (width <= 0.0f || !maxLines)
      return;

    std::vector<std::string> lines;
    float fit = std::min(scale, maxWidth / width);

    if (fit >= minScale) {
      scale = fit;
      lines.push_back(text);
    } else {
      scale = minScale;
      lines = vrWrapText(font, text, scale, maxWidth);

      if (lines.size() > maxLines) {
        lines.resize(maxLines);
        std::string& last = lines.back();

        while (!last.empty() && measureText(font, last + "...", scale) > maxWidth)
          last.pop_back();

        last += "...";
      }
    }

    float lineHeight = font.metrics.lineHeight() * scale;
    float capitals   = font.metrics.glyph('A').ascent * scale;
    float top        = cy - 0.5f * lineHeight * float(lines.size());

    for (size_t i = 0; i < lines.size(); i++) {
      float lineWidth = measureText(font, lines[i], scale);
      float baseline  = top + (float(i) + 0.5f) * lineHeight + 0.5f * capitals;
      drawText(font, lines[i], cx - 0.5f * lineWidth, baseline, scale, colour);
    }
  }


  std::vector<std::string> vrWrapText(const VrCanvasFont& font, const std::string& text, float scale, float maxWidth) {
    std::vector<std::string> lines;
    std::string line;
    size_t start = 0;

    while (start < text.size()) {
      size_t end = text.find(' ', start);

      if (end == std::string::npos)
        end = text.size();

      std::string word = text.substr(start, end - start);
      start = end + 1;

      if (word.empty())
        continue;

      std::string joined = line.empty() ? word : line + " " + word;

      if (!line.empty() && VrCanvas::measureText(font, joined, scale) > maxWidth) {
        lines.push_back(line);
        line = word;
      } else {
        line = joined;
      }
    }

    if (!line.empty())
      lines.push_back(line);

    return lines;
  }

}
