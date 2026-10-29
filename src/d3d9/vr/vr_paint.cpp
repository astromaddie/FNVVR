#include <algorithm>
#include <cmath>

#include "vr_paint.h"

namespace dxvk {

  namespace {

    constexpr float TwoPi = 6.2831853f;

    // Longest straight piece of a curved edge, in pixels
    constexpr float SegmentLength = 4.0f;

    // Points on each rounded corner of a rectangle
    constexpr uint32_t CornerPoints = 8;

    uint32_t channelByte(float value) {
      return uint32_t(std::clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f);
    }

    // How much of a pixel at a signed distance inside an edge is covered
    float coverageOf(float distanceInside) {
      return std::clamp(distanceInside + 0.5f, 0.0f, 1.0f);
    }

    // A point at an angle clockwise from the top, the Y axis pointing down
    VrPaintVertex polar(float cx, float cy, float radius, float angle, uint32_t colour) {
      VrPaintVertex vertex;
      vertex.x      = cx + std::sin(angle) * radius;
      vertex.y      = cy - std::cos(angle) * radius;
      vertex.colour = colour;
      return vertex;
    }

    uint32_t curveSegments(float span, float radius) {
      return std::max(uint32_t(std::ceil(span * radius / SegmentLength)), 1u);
    }

  }


  VrColour VrColour::fromRgba(uint32_t rgba) {
    return { float((rgba >> 24) & 0xFF) / 255.0f, float((rgba >> 16) & 0xFF) / 255.0f,
      float((rgba >> 8) & 0xFF) / 255.0f, float(rgba & 0xFF) / 255.0f };
  }


  uint32_t vrPremultiplied(const VrColour& colour) {
    float a = std::clamp(colour.a, 0.0f, 1.0f);
    return channelByte(a) << 24 | channelByte(colour.r * a) << 16
         | channelByte(colour.g * a) << 8 | channelByte(colour.b * a);
  }


  void VrPaintList::clear() {
    m_blend = VrPaintBlend::Over;
    m_vertices.clear();
    m_batches.clear();
  }


  void VrPaintList::fillRect(float left, float top, float right, float bottom, const VrColour& colour) {
    if (right <= left || bottom <= top)
      return;

    uint32_t c = vrPremultiplied(colour);

    begin(nullptr, VrPaintImageUse::Shape);
    quad({ left, top, 0.0f, 0.0f, c }, { right, top, 0.0f, 0.0f, c },
         { right, bottom, 0.0f, 0.0f, c }, { left, bottom, 0.0f, 0.0f, c });
  }


  void VrPaintList::fillRoundedRect(float left, float top, float right, float bottom, float radius,
      const VrColour& colour) {
    if (right <= left || bottom <= top)
      return;

    radius = std::clamp(radius, 0.0f, 0.5f * std::min(right - left, bottom - top));

    // Corners of the rectangle shrunk by the radius, clockwise from the
    // top right, and where each corner's arc starts
    const float corners[4][2] = {
      { right - radius, top + radius },
      { right - radius, bottom - radius },
      { left + radius,  bottom - radius },
      { left + radius,  top + radius },
    };

    // The outline at a distance from the edge, outwards: corners of
    // another radius, or of none moved inwards
    auto outline = [&] (float offset, uint32_t colour) {
      std::vector<VrPaintVertex> points;
      float r = radius + offset;
      float inward = std::max(-r, 0.0f);
      r = std::max(r, 0.0f);

      for (uint32_t i = 0; i < 4; i++) {
        float cx = corners[i][0] + (i < 2 ? -inward : inward);
        float cy = corners[i][1] + (i == 0 || i == 3 ? inward : -inward);

        for (uint32_t j = 0; j < CornerPoints; j++) {
          float angle = 0.25f * TwoPi * (float(i) + float(j) / float(CornerPoints - 1));
          points.push_back(polar(cx, cy, r, angle, colour));
        }
      }

      return points;
    };

    uint32_t c = vrPremultiplied(colour);
    std::vector<VrPaintVertex> inside  = outline(-0.5f, c);
    std::vector<VrPaintVertex> outside = outline( 0.5f, 0u);

    VrPaintVertex centre = { 0.5f * (left + right), 0.5f * (top + bottom), 0.0f, 0.0f, c };

    begin(nullptr, VrPaintImageUse::Shape);

    for (size_t i = 0; i < inside.size(); i++) {
      size_t next = (i + 1) % inside.size();
      triangle(centre, inside[i], inside[next]);
      quad(inside[i], outside[i], outside[next], inside[next]);
    }
  }


  void VrPaintList::fillSector(float cx, float cy, float inner, float outer, float start, float end,
      const VrColour& colour) {
    float span = end - start;

    if (span <= 0.0f || outer <= inner || colour.a <= 0.0f)
      return;

    bool full = span >= TwoPi - 1.0e-4f;

    if (full)
      span = TwoPi;

    // Rows of points across the ring where its coverage changes: the
    // edges fade out over one pixel, as the pixels they cross are covered
    float radii[4];
    uint32_t rows = 0;

    if (inner > 0.0f) {
      radii[rows++] = std::max(inner - 0.5f, 0.0f);
      radii[rows++] = inner + 0.5f;
    } else {
      radii[rows++] = 0.0f;
    }

    radii[rows++] = std::max(outer - 0.5f, 0.0f);
    radii[rows++] = outer + 0.5f;
    std::sort(radii, radii + rows);

    auto radialCoverage = [&] (float r) {
      float coverage = coverageOf(outer - r);
      return inner > 0.0f ? coverage * coverageOf(r - inner) : coverage;
    };

    // Columns along the ring; a sector's straight edges fade out too, one
    // pixel across at each row's radius
    uint32_t segments = curveSegments(span, outer + 0.5f);
    uint32_t columns  = full ? segments + 1 : segments + 3;

    auto point = [&] (uint32_t row, uint32_t column) {
      float r = radii[row];
      float angle;
      float coverage = radialCoverage(r);

      if (full) {
        angle = start + span * float(column) / float(segments);
      } else {
        // Half a pixel along the arc, at most half the sector
        float half = r > 0.0f ? std::min(0.5f / r, 0.5f * span) : 0.0f;

        if (column == 0 || column == columns - 1) {
          angle = column == 0 ? start - half : start + span + half;
          coverage = 0.0f;
        } else {
          float inside = r > 0.0f ? std::min(span * r, 1.0f) : 1.0f;
          angle = start + half + (span - 2.0f * half) * float(column - 1) / float(segments);
          coverage *= inside;
        }
      }

      return polar(cx, cy, r, angle, vrPremultiplied(colour.faded(coverage)));
    };

    begin(nullptr, VrPaintImageUse::Shape);

    for (uint32_t row = 0; row + 1 < rows; row++) {
      VrPaintVertex lower = point(row, 0);
      VrPaintVertex upper = point(row + 1, 0);

      for (uint32_t column = 1; column < columns; column++) {
        VrPaintVertex nextLower = point(row, column);
        VrPaintVertex nextUpper = point(row + 1, column);

        if (lower.colour || upper.colour || nextLower.colour || nextUpper.colour)
          quad(lower, upper, nextUpper, nextLower);

        lower = nextLower;
        upper = nextUpper;
      }
    }
  }


  void VrPaintList::fillGlow(float cx, float cy, float inner, float outer, const VrColour& innerColour,
      const VrColour& outerColour) {
    if (outer <= inner)
      return;

    uint32_t segments = curveSegments(TwoPi, outer);
    uint32_t in  = vrPremultiplied(innerColour);
    uint32_t out = vrPremultiplied(outerColour);

    begin(nullptr, VrPaintImageUse::Shape);

    for (uint32_t i = 0; i < segments; i++) {
      float a0 = TwoPi * float(i)     / float(segments);
      float a1 = TwoPi * float(i + 1) / float(segments);

      quad(polar(cx, cy, inner, a0, in), polar(cx, cy, outer, a0, out),
           polar(cx, cy, outer, a1, out), polar(cx, cy, inner, a1, in));
    }
  }


  void VrPaintList::drawShape(const VrImage& image, float cx, float cy, float size, const VrColour& colour) {
    if (!image.width || !image.height || size <= 0.0f)
      return;

    float scale  = size / float(std::max(image.width, image.height));
    float width  = float(image.width)  * scale;
    float height = float(image.height) * scale;
    float left   = cx - 0.5f * width;
    float top    = cy - 0.5f * height;

    drawImage(image, VrPaintImageUse::Shape, left, top, left + width, top + height, 0.0f, 0.0f, 1.0f, 1.0f, colour);
  }


  void VrPaintList::drawImage(const VrImage& image, VrPaintImageUse use, float left, float top, float right,
      float bottom, float u0, float v0, float u1, float v1, const VrColour& colour) {
    if (!image.width || !image.height || right <= left || bottom <= top)
      return;

    uint32_t c = vrPremultiplied(colour);

    begin(&image, use);
    quad({ left, top, u0, v0, c }, { right, top, u1, v0, c },
         { right, bottom, u1, v1, c }, { left, bottom, u0, v1, c });
  }


  float VrPaintList::measureText(const VrPaintFont& font, const std::string& text, float scale) {
    float width = 0.0f;

    for (char c : text)
      width += font.metrics.glyph(c).advance();

    return width * scale;
  }


  void VrPaintList::drawText(const VrPaintFont& font, const std::string& text, float x, float baseline, float scale,
      const VrColour& colour) {
    for (char c : text) {
      const VrGlyph& glyph = font.metrics.glyph(c);

      if (glyph.width > 0.0f && glyph.height > 0.0f) {
        float left = x + glyph.before * scale;
        float top  = baseline - glyph.ascent * scale;

        drawImage(font.glyphs, VrPaintImageUse::Mask, left, top, left + glyph.width * scale,
          top + glyph.height * scale, glyph.u0, glyph.v0, glyph.u1, glyph.v1, colour);
      }

      x += glyph.advance() * scale;
    }
  }


  void VrPaintList::drawTextBlock(const VrPaintFont& font, const std::string& text, float cx, float cy,
      float maxWidth, float scale, float minScale, uint32_t maxLines, const VrColour& colour) {
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


  void VrPaintList::begin(const VrImage* image, VrPaintImageUse use) {
    if (!m_batches.empty()) {
      const VrPaintBatch& last = m_batches.back();

      if (last.image == image && (!image || last.use == use) && last.blend == m_blend)
        return;
    }

    VrPaintBatch batch;
    batch.image = image;
    batch.use   = use;
    batch.blend = m_blend;
    batch.first = uint32_t(m_vertices.size());
    m_batches.push_back(batch);
  }


  void VrPaintList::triangle(const VrPaintVertex& a, const VrPaintVertex& b, const VrPaintVertex& c) {
    m_vertices.push_back(a);
    m_vertices.push_back(b);
    m_vertices.push_back(c);
    m_batches.back().count += 3;
  }


  void VrPaintList::quad(const VrPaintVertex& a, const VrPaintVertex& b, const VrPaintVertex& c,
      const VrPaintVertex& d) {
    triangle(a, b, c);
    triangle(a, c, d);
  }


  std::vector<std::string> vrWrapText(const VrPaintFont& font, const std::string& text, float scale, float maxWidth) {
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

      if (!line.empty() && VrPaintList::measureText(font, joined, scale) > maxWidth) {
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
