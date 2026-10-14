#include "test.h"

#include "../vr_render_profile.h"

using namespace dxvk;

TEST_CASE(renderProfileSumsPassesPerFrame) {
  VrRenderProfile profile(2);

  profile.add(0, VrRenderPass::Eye, 5.0);
  profile.add(0, VrRenderPass::Shadows, 1.0);
  profile.add(0, VrRenderPass::Shadows, 0.5);
  profile.add(1, VrRenderPass::Eye, 4.0);
  CHECK(!profile.endFrame());

  profile.add(0, VrRenderPass::Eye, 3.0);
  profile.add(1, VrRenderPass::Eye, 4.0);
  profile.add(1, VrRenderPass::Scene, 2.0);
  CHECK(profile.endFrame());

  CHECK(profile.frames() == 2);
  CHECK_NEAR(profile.averageMs(0, VrRenderPass::Eye), 4.0, 1e-9);
  CHECK_NEAR(profile.averageMs(0, VrRenderPass::Shadows), 0.75, 1e-9);
  CHECK_NEAR(profile.maxMs(0, VrRenderPass::Shadows), 1.5, 1e-9);
  CHECK(profile.passFrames(0, VrRenderPass::Shadows) == 1);
  CHECK(profile.passFrames(1, VrRenderPass::Shadows) == 0);
  CHECK_NEAR(profile.averageMs(1, VrRenderPass::Scene), 1.0, 1e-9);
}

TEST_CASE(renderProfileOtherIsTheEyeOutsideThePasses) {
  VrRenderProfile profile(1);

  profile.add(1, VrRenderPass::Eye, 6.0);
  profile.add(1, VrRenderPass::Scene, 3.0);
  profile.add(1, VrRenderPass::ImageSpace, 1.0);
  profile.endFrame();

  CHECK_NEAR(profile.otherMs(1), 2.0, 1e-9);
  CHECK_NEAR(profile.otherMs(0), 0.0, 1e-9);
}

TEST_CASE(renderProfileIgnoresFramesAfterAFullWindow) {
  VrRenderProfile profile(1);

  profile.add(0, VrRenderPass::Eye, 2.0);
  CHECK(profile.endFrame());

  profile.add(0, VrRenderPass::Eye, 100.0);
  CHECK(profile.endFrame());
  CHECK(profile.frames() == 1);
  CHECK_NEAR(profile.averageMs(0, VrRenderPass::Eye), 2.0, 1e-9);

  profile.reset();
  CHECK(profile.frames() == 0);
  CHECK_NEAR(profile.maxMs(0, VrRenderPass::Eye), 0.0, 1e-9);
}
