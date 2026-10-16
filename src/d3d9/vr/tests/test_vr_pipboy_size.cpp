#include "test.h"

#include "../vr_pipboy_size.h"

using namespace dxvk;

namespace {

  constexpr float Frame = 1.0f / 90.0f;

  float run(VrPipBoyGrowth& growth, bool grown, float scale, float duration) {
    float factor = growth.factor();

    for (float t = 0.0f; t < duration; t += Frame)
      factor = growth.update(grown, scale, Frame);

    return factor;
  }

}


TEST_CASE(closest_point_on_line_projects_onto_the_axis) {
  VrVector3 point = vrClosestPointOnLine({ 0.0f, 0.0f, 0.0f }, { 10.0f, 0.0f, 0.0f }, { 4.0f, 3.0f, -2.0f });
  CHECK_NEAR(point.x, 4.0f, 1e-6);
  CHECK_NEAR(point.y, 0.0f, 1e-6);
  CHECK_NEAR(point.z, 0.0f, 1e-6);

  // Beyond the ends, the line goes on
  point = vrClosestPointOnLine({ 1.0f, 1.0f, 0.0f }, { 1.0f, 2.0f, 0.0f }, { 5.0f, -3.0f, 0.0f });
  CHECK_NEAR(point.x, 1.0f, 1e-6);
  CHECK_NEAR(point.y, -3.0f, 1e-6);

  // Without a line, the first point
  point = vrClosestPointOnLine({ 1.0f, 2.0f, 3.0f }, { 1.0f, 2.0f, 3.0f }, { 5.0f, 5.0f, 5.0f });
  CHECK_NEAR(point.x, 1.0f, 1e-6);
  CHECK_NEAR(point.z, 3.0f, 1e-6);
}


TEST_CASE(scaling_about_a_point_keeps_the_point) {
  VrNodeScale node = { { 2.0f, -1.0f, 0.5f }, 1.5f };
  VrVector3 pivot  = { 3.0f, 0.0f, 0.0f };

  VrNodeScale scaled = vrScaleAboutPoint(node, pivot, 2.0f);
  CHECK_NEAR(scaled.scale, 3.0f, 1e-6);

  // A node point x (rotation left out, identity) lands at s * x + t: the
  // node point at the pivot stays there, others move away by the factor
  VrVector3 atPivot = { (pivot.x - node.translate.x) / node.scale,
                        (pivot.y - node.translate.y) / node.scale,
                        (pivot.z - node.translate.z) / node.scale };
  CHECK_NEAR(atPivot.x * scaled.scale + scaled.translate.x, pivot.x, 1e-5);
  CHECK_NEAR(atPivot.y * scaled.scale + scaled.translate.y, pivot.y, 1e-5);
  CHECK_NEAR(atPivot.z * scaled.scale + scaled.translate.z, pivot.z, 1e-5);

  // The node's origin was 1 unit left of the pivot, it goes to 2
  CHECK_NEAR(scaled.translate.x, 1.0f, 1e-6);
  CHECK_NEAR(scaled.translate.y, -2.0f, 1e-6);

  // The factor 1 leaves the node alone
  VrNodeScale same = vrScaleAboutPoint(node, pivot, 1.0f);
  CHECK_NEAR(same.translate.x, node.translate.x, 1e-6);
  CHECK_NEAR(same.scale, node.scale, 1e-6);
}


TEST_CASE(pipboy_grows_while_open_and_shrinks_back) {
  VrPipBoyGrowth growth;
  CHECK_NEAR(growth.factor(), 1.0f, 1e-6);

  // Halfway through the grow time, halfway to the size
  float factor = growth.update(true, 2.0f, VrPipBoyGrowth::GrowTime * 0.5f);
  CHECK_NEAR(factor, 1.5f, 1e-5);

  CHECK_NEAR(run(growth, true, 2.0f, 1.0f), 2.0f, 1e-6);

  // Closed: back to the game's size in the same time
  growth.update(false, 2.0f, VrPipBoyGrowth::GrowTime * 0.5f);
  CHECK_NEAR(growth.factor(), 1.5f, 1e-5);
  CHECK_NEAR(run(growth, false, 2.0f, 1.0f), 1.0f, 1e-6);
}


TEST_CASE(pipboy_keeps_its_size_at_the_normal_option) {
  VrPipBoyGrowth growth;
  CHECK_NEAR(run(growth, true, 1.0f, 1.0f), 1.0f, 1e-6);

  // Sizes below the game's are not used
  CHECK_NEAR(run(growth, true, 0.5f, 1.0f), 1.0f, 1e-6);
}


TEST_CASE(pipboy_follows_a_size_changed_while_open) {
  VrPipBoyGrowth growth;
  run(growth, true, 2.0f, 1.0f);

  // Set back to Normal from the options page while open
  CHECK_NEAR(run(growth, true, 1.0f, 1.0f), 1.0f, 1e-6);

  CHECK_NEAR(run(growth, true, 1.5f, 1.0f), 1.5f, 1e-6);
}
