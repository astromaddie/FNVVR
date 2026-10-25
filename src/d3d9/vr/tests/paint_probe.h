#pragma once

#include "../vr_paint.h"

namespace dxvk::test {

  /**
   * \brief Colour a paint list leaves at a point, premultiplied
   *
   * The triangles covering the point are blended Over in order, their
   * vertex colours interpolated; images are not read. Points on the
   * triangles' shared edges would count twice, so tests probe off them.
   */
  struct PaintProbe {
    float a = 0.0f;
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
  };

  inline PaintProbe probePaint(const VrPaintList& list, float x, float y) {
    PaintProbe result;
    const std::vector<VrPaintVertex>& v = list.vertices();

    auto channel = [] (uint32_t colour, uint32_t shift) {
      return float((colour >> shift) & 0xFF) / 255.0f;
    };

    for (const VrPaintBatch& batch : list.batches()) {
      for (uint32_t i = batch.first; i + 3 <= batch.first + batch.count; i += 3) {
        const VrPaintVertex& p0 = v[i];
        const VrPaintVertex& p1 = v[i + 1];
        const VrPaintVertex& p2 = v[i + 2];

        float area = (p1.x - p0.x) * (p2.y - p0.y) - (p2.x - p0.x) * (p1.y - p0.y);

        if (std::abs(area) < 1.0e-6f)
          continue;

        float w1 = ((x - p0.x) * (p2.y - p0.y) - (p2.x - p0.x) * (y - p0.y)) / area;
        float w2 = ((p1.x - p0.x) * (y - p0.y) - (x - p0.x) * (p1.y - p0.y)) / area;
        float w0 = 1.0f - w1 - w2;

        if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f)
          continue;

        auto mix = [&] (uint32_t shift) {
          return w0 * channel(p0.colour, shift) + w1 * channel(p1.colour, shift) + w2 * channel(p2.colour, shift);
        };

        float a = mix(24);
        result.r = mix(16) + result.r * (1.0f - a);
        result.g = mix(8)  + result.g * (1.0f - a);
        result.b = mix(0)  + result.b * (1.0f - a);
        result.a = a       + result.a * (1.0f - a);
      }
    }

    return result;
  }

}
