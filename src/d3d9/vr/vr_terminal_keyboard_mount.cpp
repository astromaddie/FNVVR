#include <cmath>

#include "vr_math.h"
#include "vr_terminal_keyboard_mount.h"

namespace dxvk {

  namespace {

    VrVector3 cross(const VrVector3& a, const VrVector3& b) {
      return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
    }

    VrVector3 normalize(const VrVector3& v) {
      float length = vrLength(v);
      return length > 0.0f ? v * (1.0f / length) : v;
    }

    // The world terminal's monitor faces +X, the console's monitor -Y,
    // both with +Z up: the world's +X, +Y and +Z are the console's -Y,
    // +X and +Z
    VrGameRotation worldToConsole() {
      VrGameRotation r;
      r.m[0][0] = 0.0f;  r.m[0][1] = 1.0f;  r.m[0][2] = 0.0f;
      r.m[1][0] = -1.0f; r.m[1][1] = 0.0f;  r.m[1][2] = 0.0f;
      r.m[2][0] = 0.0f;  r.m[2][1] = 0.0f;  r.m[2][2] = 1.0f;
      return r;
    }

  }


  VrVector3 vrApplyTransform(const VrGameScaledTransform& transform, const VrVector3& point) {
    return transform.rotate * point * transform.scale + transform.translate;
  }


  VrGameScaledTransform vrComposeTransforms(const VrGameScaledTransform& parent, const VrGameScaledTransform& child) {
    VrGameScaledTransform result;
    result.rotate    = parent.rotate * child.rotate;
    result.scale     = parent.scale * child.scale;
    result.translate = vrApplyTransform(parent, child.translate);
    return result;
  }


  VrGameScaledTransform vrInverseTransform(const VrGameScaledTransform& transform) {
    VrGameScaledTransform result;
    result.rotate    = vrTranspose(transform.rotate);
    result.scale     = 1.0f / transform.scale;
    result.translate = result.rotate * transform.translate * -result.scale;
    return result;
  }


  VrGameScaledTransform VrTerminalKeyboardMount::computeRoot(const VrGameScaledTransform& screenLocal) {
    // The world monitor's glass centre lands on the console's, scaled to
    // the console's glass width
    VrGameScaledTransform mount;
    mount.rotate    = worldToConsole();
    mount.scale     = ConsoleGlassWidth / WorldGlassWidth;
    mount.translate = ConsoleGlassCentre - mount.rotate * WorldGlassCentre * mount.scale;

    return vrComposeTransforms(mount, vrInverseTransform(screenLocal));
  }


  VrVector3 VrTerminalKeyboardPlate::point(float u, float v) {
    return Origin + TexelRight * (u - OriginU) + TexelDown * (v - OriginV);
  }


  VrPanelPlacement VrTerminalKeyboardPlate::placement(
    const VrGameScaledTransform& keyboardWorld,
    const VrPose&                anchor,
    const VrVector3&             target,
          float                  unitsPerMetre) {
    const VrTexelRect& plate = VrTerminalKeyboard::Plate;

    VrVector3 centre = vrApplyTransform(keyboardWorld,
      point(0.5f * (plate.left + plate.right), 0.5f * (plate.top + plate.bottom)));

    VrVector3 right = vrComputeMenuSceneDirectionInRoom(anchor, keyboardWorld.rotate * TexelRight);
    VrVector3 down  = vrComputeMenuSceneDirectionInRoom(anchor, keyboardWorld.rotate * TexelDown);

    // The panel's +X runs along the texture's rows, its +Y up the texture,
    // and its +Z out of the key tops
    VrVector3 x = normalize(right);
    VrVector3 y = normalize(x * vrDot(down, x) - down);
    VrVector3 z = cross(x, y);

    VrGameRotation axes;
    const VrVector3* columns[3] = { &x, &y, &z };

    for (uint32_t j = 0; j < 3; j++) {
      axes.m[0][j] = columns[j]->x;
      axes.m[1][j] = columns[j]->y;
      axes.m[2][j] = columns[j]->z;
    }

    float metresPerUnit = keyboardWorld.scale / unitsPerMetre;

    VrPanelPlacement result;
    result.pose.orientation = vrQuaternionFromGameRotation(axes);
    result.pose.position    = vrComputeMenuScenePointInRoom(anchor, target, unitsPerMetre, centre);
    result.width            = plate.width()  * vrLength(TexelRight) * metresPerUnit;
    result.height           = plate.height() * vrLength(TexelDown)  * metresPerUnit;
    return result;
  }

}
