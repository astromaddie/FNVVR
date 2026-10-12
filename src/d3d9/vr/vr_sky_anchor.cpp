#include <cstdint>
#include <cstring>

#include "vr_game_addresses.h"
#include "vr_game_memory.h"
#include "vr_sky_anchor.h"

namespace dxvk {

  namespace {

    // Main::DrawWorld_Init is __thiscall without arguments and returns the
    // sun to draw, or null
    using DrawWorldInitFn = void* (__fastcall*)(void* main, void* unused);

    // NiAVObject::Update is __thiscall with one stack argument
    using ObjectUpdateFn = void (__fastcall*)(uint8_t* object, void* unused, void* updateData);

    // The game builds the update data for these roots with a time of zero
    // and every flag cleared, which is all zeroes. Larger than the game's
    // structure, so that nothing past its end is read.
    struct ZeroUpdateData {
      uint8_t bytes[24] = { };
    };

    VrSkyAnchor* g_skyAnchor = nullptr;

    void moveRoot(uintptr_t root, const VrVector3& position) {
      uint8_t* object = *reinterpret_cast<uint8_t**>(root);

      if (!object)
        return;

      float translate[3] = { position.x, position.y, position.z };
      std::memcpy(object + VrGame::ObjectLocalTranslation, translate, sizeof(translate));

      ZeroUpdateData updateData;
      reinterpret_cast<ObjectUpdateFn>(VrGame::ObjectUpdate)(object, nullptr, &updateData);
    }

    // The game turns the sun glare with the camera (RIGID_FACE_CAMERA), so
    // its rays rolled with the head. Facing the eye's position instead keeps
    // its orientation fixed in the world.
    void faceGlareToEye(void* sun) {
      if (!sun)
        return;

      uint8_t* glare = *reinterpret_cast<uint8_t**>(
        reinterpret_cast<uint8_t*>(sun) + VrGame::SunGlareNode);

      if (!glare)
        return;

      uint16_t& flags = *reinterpret_cast<uint16_t*>(glare + VrGame::BillboardFlags);
      flags = uint16_t((flags & ~VrGame::BillboardModeMask) | VrGame::BillboardAlwaysFaceCenter);
    }

  }


  VrSkyAnchor::~VrSkyAnchor() {
    // The patched calls stay in place and fall through to the game's function
    g_skyAnchor = nullptr;
  }


  std::unique_ptr<VrSkyAnchor> VrSkyAnchor::install() {
    if (g_skyAnchor)
      return nullptr;

    bool patched = VrGameMemory::readable(VrGame::SkyRoot, sizeof(void*))
      && VrGameMemory::readable(VrGame::WeatherRoot, sizeof(void*));

    for (uintptr_t site : VrGame::DrawWorldInitCallSites) {
      patched = patched && VrGameMemory::redirectCall(site, VrGame::DrawWorldInit,
        reinterpret_cast<const void*>(&VrSkyAnchor::drawWorldInit));
    }

    if (!patched)
      return nullptr;

    std::unique_ptr<VrSkyAnchor> anchor(new VrSkyAnchor());
    g_skyAnchor = anchor.get();
    return anchor;
  }


  void* __fastcall VrSkyAnchor::drawWorldInit(
          void*                 main,
          void*                 unused) {
    void* sun = reinterpret_cast<DrawWorldInitFn>(VrGame::DrawWorldInit)(main, nullptr);

    if (g_skyAnchor) {
      faceGlareToEye(sun);
      g_skyAnchor->moveRoots();
    }

    return sun;
  }


  void VrSkyAnchor::moveRoots() const {
    if (!m_eyePosition)
      return;

    moveRoot(VrGame::SkyRoot, *m_eyePosition);
    moveRoot(VrGame::WeatherRoot, *m_eyePosition);
  }

}
