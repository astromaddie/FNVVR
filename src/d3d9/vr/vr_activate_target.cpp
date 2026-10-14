#include <cstring>

#include "vr_activate_target.h"
#include "vr_game_addresses.h"
#include "vr_game_memory.h"

namespace dxvk {

  namespace {

    // Interface::GetActivateREFR is __cdecl without arguments
    using GetActivateRefFn = uintptr_t (__cdecl*)();

    // TESObjectREFR::Get3D, __thiscall without arguments
    using Get3DFn = uintptr_t (__fastcall*)(uintptr_t reference, void* unused);

    // A world bound is a centre followed by a radius
    constexpr uintptr_t BoundRadius = 0xC;

    template<typename T>
    T readField(uintptr_t base, uintptr_t offset) {
      return *reinterpret_cast<const T*>(base + offset);
    }

  }


  bool VrActivateTarget::initialize() {
    m_available = VrGameMemory::matches(VrGame::GetActivateRef,
      VrGame::GetActivateRefPrologue, sizeof(VrGame::GetActivateRefPrologue));
    return m_available;
  }


  bool VrActivateTarget::find(VrVector3& position) const {
    if (!m_available)
      return false;

    uintptr_t reference = reinterpret_cast<GetActivateRefFn>(VrGame::GetActivateRef)();

    if (!reference)
      return false;

    auto vtable = readField<uintptr_t>(reference, 0);
    uintptr_t root = reinterpret_cast<Get3DFn>(readField<uintptr_t>(vtable, VrGame::RefGet3DSlot))(reference, nullptr);

    if (!root)
      return false;

    // The model's origin is often at its base, such as an actor's feet,
    // while its bound is centred on it
    auto bound = readField<uintptr_t>(root, VrGame::ObjectWorldBound);

    if (bound && readField<float>(bound, BoundRadius) > 0.0f)
      std::memcpy(&position, reinterpret_cast<const void*>(bound), sizeof(position));
    else
      std::memcpy(&position, reinterpret_cast<const void*>(root + VrGame::ObjectWorldTranslation), sizeof(position));

    return true;
  }

}
