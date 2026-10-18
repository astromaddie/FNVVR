#include "test.h"

#include "../vr_math.h"
#include "../vr_terminal_keyboard_mount.h"

using namespace dxvk;

namespace {

  constexpr float Pi = 3.14159265358979f;

  void checkVector(const VrVector3& v, const VrVector3& expected, double epsilon) {
    CHECK_NEAR(v.x, expected.x, epsilon);
    CHECK_NEAR(v.y, expected.y, epsilon);
    CHECK_NEAR(v.z, expected.z, epsilon);
  }

  VrGameRotation rotationAbout(const VrVector3& axis, float angle) {
    return vrGameRotationFromQuaternion(vrQuaternionFromAxisAngle(axis, angle));
  }

  // T01Screen01 in terminal01.nif: tilted back by 10 degrees
  VrGameScaledTransform worldMonitor() {
    VrGameScaledTransform screen;
    screen.rotate.m[0][0] = 0.98481f;  screen.rotate.m[0][1] = 0.0f; screen.rotate.m[0][2] = 0.17365f;
    screen.rotate.m[1][0] = 0.0f;      screen.rotate.m[1][1] = 1.0f; screen.rotate.m[1][2] = 0.0f;
    screen.rotate.m[2][0] = -0.17365f; screen.rotate.m[2][1] = 0.0f; screen.rotate.m[2][2] = 0.98481f;
    screen.translate = { 16.551f, 1.721f, -4.668f };
    return screen;
  }

  int32_t keyIndex(char c) {
    uint32_t count = 0;
    const VrTerminalKey* keys = VrTerminalKeyboard::keys(count);

    for (uint32_t i = 0; i < count; i++) {
      if (keys[i].kind == VrTerminalKeyKind::Char && keys[i].lower == c)
        return int32_t(i);
    }

    return -1;
  }

}


TEST_CASE(transforms_compose_and_invert) {
  VrGameScaledTransform a;
  a.rotate    = rotationAbout({ 0.3f, 1.0f, -0.2f }, 0.7f);
  a.translate = { 1.0f, -2.0f, 3.0f };
  a.scale     = 0.5f;

  VrGameScaledTransform b;
  b.rotate    = rotationAbout({ 1.0f, 0.0f, 0.5f }, -1.2f);
  b.translate = { -4.0f, 0.5f, 2.0f };
  b.scale     = 3.0f;

  VrVector3 p = { 0.25f, -1.5f, 2.0f };

  checkVector(vrApplyTransform(vrComposeTransforms(a, b), p), vrApplyTransform(a, vrApplyTransform(b, p)), 1e-4);
  checkVector(vrApplyTransform(vrInverseTransform(a), vrApplyTransform(a, p)), p, 1e-4);
}


TEST_CASE(terminal_keyboard_mount_lays_the_world_glass_on_the_console_glass) {
  using Mount = VrTerminalKeyboardMount;

  VrGameScaledTransform screen = worldMonitor();
  VrGameScaledTransform root   = Mount::computeRoot(screen);

  // A point of the world monitor, through the model's root
  auto onConsole = [&] (const VrVector3& p) {
    return vrApplyTransform(vrComposeTransforms(root, screen), p);
  };

  float scale = Mount::ConsoleGlassWidth / Mount::WorldGlassWidth;
  CHECK_NEAR(root.scale, scale, 1e-6);

  VrVector3 centre = Mount::WorldGlassCentre;
  checkVector(onConsole(centre), Mount::ConsoleGlassCentre, 1e-4);

  // The viewer's right (+Y), up (+Z) and towards the viewer (+X) on the
  // world monitor are +X, +Z and -Y on the console's
  checkVector(onConsole(centre + VrVector3 { 0.0f, 10.0f, 0.0f }) - Mount::ConsoleGlassCentre, { 10.0f * scale, 0.0f, 0.0f }, 1e-4);
  checkVector(onConsole(centre + VrVector3 { 0.0f, 0.0f, 10.0f }) - Mount::ConsoleGlassCentre, { 0.0f, 0.0f, 10.0f * scale }, 1e-4);
  checkVector(onConsole(centre + VrVector3 { 10.0f, 0.0f, 0.0f }) - Mount::ConsoleGlassCentre, { 0.0f, -10.0f * scale, 0.0f }, 1e-4);

  // The glass is as wide as the console's
  VrVector3 left  = onConsole(centre - VrVector3 { 0.0f, 0.5f * Mount::WorldGlassWidth, 0.0f });
  VrVector3 right = onConsole(centre + VrVector3 { 0.0f, 0.5f * Mount::WorldGlassWidth, 0.0f });
  CHECK_NEAR(vrLength(right - left), Mount::ConsoleGlassWidth, 1e-4);
}


TEST_CASE(terminal_plate_lies_on_the_key_tops) {
  // Corner vertices of the key tops in terminal01.nif, with their texels
  checkVector(VrTerminalKeyboardPlate::point(248.3661f, 295.0482f), { -2.86965f, 16.86250f, 4.16388f }, 0.01);
  checkVector(VrTerminalKeyboardPlate::point(46.0794f, 378.8532f),  { 11.11136f, -16.88709f, 2.39440f }, 0.01);
  checkVector(VrTerminalKeyboardPlate::point(248.3825f, 378.8673f), { 11.11136f, 16.86250f, 2.39440f }, 0.01);
}


TEST_CASE(terminal_plate_placement_finds_the_key_under_a_laser) {
  // A keyboard turned and scaled in the menu's space, shown somewhere in the room
  VrGameScaledTransform keyboard;
  keyboard.rotate    = rotationAbout({ 0.2f, 1.0f, 0.4f }, 1.9f);
  keyboard.translate = { 18.0f, -3.0f, 1.5f };
  keyboard.scale     = 0.2f;

  VrPose anchor;
  anchor.orientation = vrQuaternionFromAxisAngle({ 0.0f, 1.0f, 0.0f }, 0.4f * Pi);
  anchor.position    = { 0.3f, 1.6f, -0.7f };

  VrVector3 target = { 20.0f, 0.38f, 0.0f };
  float unitsPerMetre = 9.6f;

  VrPanelPlacement plate = VrTerminalKeyboardPlate::placement(keyboard, anchor, target, unitsPerMetre);

  const VrTexelRect& rect = VrTerminalKeyboard::Plate;
  CHECK_NEAR(plate.width,  rect.width()  * 0.166829f * 0.2f / unitsPerMetre, 1e-4);
  CHECK_NEAR(plate.height, rect.height() * 0.168052f * 0.2f / unitsPerMetre, 1e-4);

  // The panel faces out of the key tops, whose names are on their +Z side
  VrVector3 outside = vrComputeMenuSceneDirectionInRoom(anchor, keyboard.rotate * VrVector3 { 0.125561f, 0.0f, 0.992083f });
  checkVector(vrRotate(plate.pose.orientation, { 0.0f, 0.0f, 1.0f }), outside * (1.0f / vrLength(outside)), 1e-3);

  for (char c : { 'q', 'm', '5' }) {
    int32_t index = keyIndex(c);
    uint32_t count = 0;
    const VrTexelRect& key = VrTerminalKeyboard::keys(count)[index].rect;

    VrVector3 onKey = vrComputeMenuScenePointInRoom(anchor, target, unitsPerMetre, vrApplyTransform(keyboard,
      VrTerminalKeyboardPlate::point(0.5f * (key.left + key.right), 0.5f * (key.top + key.bottom))));

    // A laser half a metre in front of the key tops, pointing at the key
    VrVector3 normal = vrRotate(plate.pose.orientation, { 0.0f, 0.0f, 1.0f });
    VrPose ray;
    ray.orientation = plate.pose.orientation;
    ray.position    = onKey + normal * 0.5f;

    VrPanelHit hit = vrIntersectPanel(ray, plate);
    CHECK(hit.hit);
    CHECK_NEAR(hit.distance, 0.5f, 1e-3);
    CHECK(VrTerminalKeyboard::findKey(hit.uv) == index);

    // From under the keyboard, nothing is hit
    ray.position = onKey - normal * 0.5f;
    ray.orientation = plate.pose.orientation * vrQuaternionFromAxisAngle({ 0.0f, 1.0f, 0.0f }, Pi);
    CHECK(!vrIntersectPanel(ray, plate).hit);
  }
}
