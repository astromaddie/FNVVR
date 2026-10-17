#include "../../util/log/log.h"
#include "../../util/util_string.h"

#include "vr_game_addresses.h"
#include "vr_game_memory.h"
#include "vr_manual_reload.h"

namespace dxvk {

  namespace {

    // PlayerCharacter::ReloadWeaponNV, __thiscall with four stack
    // arguments, returns a bool in AL
    using ReloadWeaponFn = uint32_t (__fastcall*)(uint8_t* player, void* unused,
      uint8_t* weapon, uint32_t animation, uint32_t useMod, uint32_t fromInventory);

    // Actor::GetAnimGroup, __thiscall with four stack arguments, returns
    // the 16-bit group in AX
    using GetAnimGroupFn = uint32_t (__fastcall*)(uint8_t* actor, void* unused,
      uint32_t group, void* weaponEntry, uint32_t checkHandToHand, void* animation);

    // Process virtual function, __thiscall without arguments, returns the
    // clip's inventory entry
    using GetCurrentAmmoFn = uint8_t* (__fastcall*)(uint8_t* process, void* unused);

    // Frames during which the clip of a reload is watched: the equipment
    // queue fills it a frame or so after the reload
    constexpr uint32_t ClipWatchFrames = 4;

    // Clip count logged when the player has no clip
    constexpr int32_t NoClip = -1;

    VrManualReload* g_manualReload = nullptr;

    template<typename T>
    T readField(const uint8_t* base, uintptr_t offset) {
      return *reinterpret_cast<const T*>(base + offset);
    }

    uint8_t* player() {
      return *reinterpret_cast<uint8_t* const*>(VrGame::Player);
    }

    int32_t clipCount(uint8_t* actor) {
      uint8_t* process = actor ? readField<uint8_t*>(actor, VrGame::ActorProcess) : nullptr;

      if (!process)
        return NoClip;

      auto vtable  = readField<const uint8_t*>(process, 0);
      auto getAmmo = reinterpret_cast<GetCurrentAmmoFn>(readField<uintptr_t>(vtable, VrGame::ProcessCurrentAmmoSlot));
      uint8_t* entry = getAmmo(process, nullptr);

      return entry ? readField<int32_t>(entry, VrGame::InventoryEntryCount) : NoClip;
    }

    const char* callerName(uintptr_t returnAddress) {
      switch (returnAddress) {
        case VrGame::ReloadReturnControl:   return "reload control";
        case VrGame::ReloadReturnEmptyClip: return "empty clip";
        default:                            return "game";
      }
    }

    const char* weaponName(const uint8_t* weapon) {
      const char* name = weapon ? readField<const char*>(weapon, VrGame::WeaponFullName) : nullptr;
      return name ? name : "";
    }

  }


  VrManualReload::~VrManualReload() {
    // The table entry and the call stay in place and fall through to the
    // game's functions
    g_manualReload = nullptr;
  }


  std::unique_ptr<VrManualReload> VrManualReload::install() {
    if (g_manualReload)
      return nullptr;

    if (!VrGameMemory::redirectVirtual(VrGame::PlayerReloadWeaponSlot, VrGame::PlayerReloadWeapon,
        reinterpret_cast<const void*>(&VrManualReload::reloadHook)))
      return nullptr;

    if (!VrGameMemory::redirectCall(VrGame::JamAnimGroupCallSite, VrGame::ActorGetAnimGroup,
        reinterpret_cast<const void*>(&VrManualReload::jamGroupHook)))
      Logger::warn("VR: The jam animation call was not found, the player's jams are not logged");

    std::unique_ptr<VrManualReload> reload(new VrManualReload());
    g_manualReload = reload.get();
    return reload;
  }


  void VrManualReload::update() {
    int32_t clip = clipCount(player());

    std::lock_guard<dxvk::mutex> lock(m_mutex);

    if (!m_watchFrames)
      return;

    m_watchFrames -= 1;

    if (clip != m_watchedClip) {
      Logger::info(str::format("VR: Clip after the reload: ", clip, ", ",
        ClipWatchFrames - 1 - m_watchFrames, " frame(s) later"));
      m_watchedClip = clip;
    }
  }


  void VrManualReload::watchClip(int32_t clip) {
    std::lock_guard<dxvk::mutex> lock(m_mutex);
    m_watchFrames = ClipWatchFrames;
    m_watchedClip = clip;
  }


  uint32_t __fastcall VrManualReload::reloadHook(
          uint8_t*              player,
          void*                 unused,
          uint8_t*              weapon,
          uint32_t              animation,
          uint32_t              useMod,
          uint32_t              fromInventory) {
    uintptr_t caller = VR_RETURN_ADDRESS();
    int32_t   before = clipCount(player);

    uint32_t result = reinterpret_cast<ReloadWeaponFn>(VrGame::PlayerReloadWeapon)(
      player, nullptr, weapon, animation, useMod, fromInventory);

    int32_t after = clipCount(player);

    Logger::info(str::format("VR: Reload of '", weaponName(weapon), "' from the ", callerName(caller),
      " (return address 0x", std::hex, caller, std::dec, "), animation ", animation,
      ", mod ", useMod & 0xFF, ", from inventory ", fromInventory & 0xFF,
      ": ", (result & 0xFF) ? "started" : "refused", ", clip ", before, " before, ", after, " after"));

    if (g_manualReload)
      g_manualReload->watchClip(after);

    return result;
  }


  uint32_t __fastcall VrManualReload::jamGroupHook(
          uint8_t*              actor,
          void*                 unused,
          uint32_t              group,
          void*                 weaponEntry,
          uint32_t              checkHandToHand,
          void*                 animation) {
    uint32_t found = reinterpret_cast<GetAnimGroupFn>(VrGame::ActorGetAnimGroup)(
      actor, nullptr, group, weaponEntry, checkHandToHand, animation);

    if (actor == player()) {
      Logger::info(str::format("VR: The player's weapon jammed (group 0x", std::hex, group,
        ", animation group 0x", found & 0xFFFF, std::dec, "), clip ", clipCount(actor)));
    }

    return found;
  }

}
