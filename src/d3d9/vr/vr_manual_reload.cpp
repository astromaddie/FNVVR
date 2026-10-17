#include "../../util/log/log.h"
#include "../../util/util_string.h"

#include "vr_game_addresses.h"
#include "vr_game_memory.h"
#include "vr_manual_reload.h"
#include "vr_melee.h"

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

    // Process virtual function, __thiscall without arguments, returns the
    // equipped weapon's inventory entry
    using GetWeaponInfoFn = uint8_t* (__fastcall*)(uint8_t* process, void* unused);

    // InventoryChanges::GetInventoryChanges, __cdecl with the reference
    using GetInventoryChangesFn = uint8_t* (__cdecl*)(uint8_t* reference);

    // InventoryChanges::GetObjectCount, __thiscall with the item
    using InventoryObjectCountFn = int32_t (__fastcall*)(uint8_t* changes, void* unused, const uint8_t* item);

    // ItemChange::HasModEffectActive, __thiscall with the effect, returns a
    // bool in AL
    using HasModEffectFn = uint32_t (__fastcall*)(uint8_t* entry, void* unused, uint32_t effect);

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

    uint8_t* currentAmmo(uint8_t* actor) {
      uint8_t* process = actor ? readField<uint8_t*>(actor, VrGame::ActorProcess) : nullptr;

      if (!process)
        return nullptr;

      auto vtable  = readField<const uint8_t*>(process, 0);
      auto getAmmo = reinterpret_cast<GetCurrentAmmoFn>(readField<uintptr_t>(vtable, VrGame::ProcessCurrentAmmoSlot));
      return getAmmo(process, nullptr);
    }

    uint8_t* weaponEntry(uint8_t* actor) {
      uint8_t* process = actor ? readField<uint8_t*>(actor, VrGame::ActorProcess) : nullptr;

      if (!process)
        return nullptr;

      auto vtable = readField<const uint8_t*>(process, 0);
      auto getWeaponInfo = reinterpret_cast<GetWeaponInfoFn>(readField<uintptr_t>(vtable, VrGame::ProcessWeaponInfoSlot));
      return getWeaponInfo(process, nullptr);
    }

    int32_t clipCount(uint8_t* actor) {
      uint8_t* entry = currentAmmo(actor);
      return entry ? readField<int32_t>(entry, VrGame::InventoryEntryCount) : NoClip;
    }

    /**
     * \brief Whether the inventory holds rounds of the clip's ammunition
     *
     * Without a clip entry, the game's reload decides.
     */
    bool holdsRounds(uint8_t* actor) {
      uint8_t* entry = currentAmmo(actor);
      auto ammo = entry ? readField<const uint8_t*>(entry, VrGame::InventoryEntryForm) : nullptr;

      uint8_t* changes = ammo ? reinterpret_cast<GetInventoryChangesFn>(VrGame::GetInventoryChanges)(actor) : nullptr;

      if (!changes)
        return true;

      return reinterpret_cast<InventoryObjectCountFn>(VrGame::InventoryObjectCount)(changes, nullptr, ammo) > 0;
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


  void VrManualReload::setEnabled(bool enabled) {
    if (enabled != m_enabled) {
      Logger::info(enabled
        ? "VR: Weapons with a detachable magazine are reloaded with the hands"
        : "VR: The weapons are reloaded as in the game");
    }

    m_enabled = enabled;
  }


  VrMagazineEvent VrManualReload::updateHands(const VrManualReloadInput& input) {
    uint8_t* character = player();
    uint8_t* entry     = weaponEntry(character);
    uint8_t* weapon    = entry ? readField<uint8_t*>(entry, VrGame::InventoryEntryForm) : nullptr;

    if (weapon != m_lastWeapon) {
      m_magazine.reset();
      m_lastWeapon = weapon;
    }

    // A weapon is reloaded by hand once its magazine was found in the
    // hand, and until another weapon is equipped, even in V.A.T.S. or
    // while it is not drawn in the hand. Looping weapons load round by
    // round, which comes later.
    if (weapon && input.well)
      m_wellWeapon = weapon;

    bool handled = m_enabled && weapon && weapon == m_wellWeapon
      && !readField<uint8_t>(weapon, VrGame::WeaponLoopingReload);

    m_handWeapon = handled ? weapon : nullptr;

    if (!handled) {
      m_magazine.reset();
      return VrMagazineEvent::None;
    }

    // The game filled the clip itself: equipping, V.A.T.S., an ammunition
    // swap, a pickup... The magazine is back in the weapon.
    if (m_magazine.state() != VrMagazineState::InWeapon && clipCount(character) > 0) {
      Logger::info(str::format("VR: The game filled the clip of '", weaponName(weapon),
        "', its magazine is back in the weapon"));
      m_magazine.reset();
    }

    VrMagazineInput magazine;
    magazine.active = input.head && input.rightHand && input.leftHand && input.well && !input.leftOnWeapon;
    magazine.grip   = input.grip;

    if (magazine.active) {
      magazine.hand       = input.leftHand->position;
      magazine.well       = vrPointFromHand(*input.rightHand, *input.well);
      magazine.head       = *input.head;
      magazine.roundsHeld = m_magazine.state() != VrMagazineState::Out || holdsRounds(character);
    }

    VrMagazineEvent event = m_magazine.update(magazine);

    switch (event) {
      case VrMagazineEvent::Ejected:
        ejectMagazine(character, weapon);
        break;

      case VrMagazineEvent::Inserted:
        insertMagazine(character, weapon);
        break;

      case VrMagazineEvent::Taken:
        Logger::info(str::format("VR: A new magazine for '", weaponName(weapon), "' is taken behind the back"));
        break;

      case VrMagazineEvent::NoRounds:
        Logger::info(str::format("VR: No rounds left for '", weaponName(weapon), "', no magazine is taken"));
        break;

      case VrMagazineEvent::Dropped:
        Logger::info(str::format("VR: The magazine of '", weaponName(weapon), "' is let go of"));
        break;

      case VrMagazineEvent::None:
        break;
    }

    return event;
  }


  void VrManualReload::ejectMagazine(uint8_t* player, const uint8_t* weapon) {
    // The rounds of the clip are still counted in the inventory: emptying
    // it loses none (findings/reload.md §1)
    uint8_t* clip = currentAmmo(player);
    int32_t  rounds = clip ? readField<int32_t>(clip, VrGame::InventoryEntryCount) : NoClip;

    if (clip)
      *reinterpret_cast<int32_t*>(clip + VrGame::InventoryEntryCount) = 0;

    Logger::info(str::format("VR: The magazine of '", weaponName(weapon), "' is taken out with ",
      rounds, " round(s), they stay in the inventory"));
  }


  void VrManualReload::insertMagazine(uint8_t* player, uint8_t* weapon) {
    // As the game's callers do, the extended magazine counts if it is installed
    uint8_t* entry = weaponEntry(player);
    uint32_t extended = entry
      ? reinterpret_cast<HasModEffectFn>(VrGame::ItemHasModEffect)(entry, nullptr, VrGame::ModEffectClipSize) & 0xFF
      : 0;

    // No animation: the clip is filled through the equipment queue at the
    // next frame, and the action points are spent as in the game
    uint32_t result = reinterpret_cast<ReloadWeaponFn>(VrGame::PlayerReloadWeapon)(
      player, nullptr, weapon, 0, extended, 0);

    Logger::info(str::format("VR: A magazine is pushed into '", weaponName(weapon), "' (extended magazine ",
      extended, "): ", (result & 0xFF) ? "reload started" : "refused, no rounds left"));

    watchClip(clipCount(player));
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

    // The weapons reloaded by hand are not reloaded by the control or when
    // a shot empties the clip: an empty clip clicks
    bool byHand = g_manualReload && weapon && weapon == g_manualReload->m_handWeapon.load()
      && (caller == VrGame::ReloadReturnControl || caller == VrGame::ReloadReturnEmptyClip);

    if (byHand) {
      Logger::info(str::format("VR: Reload of '", weaponName(weapon), "' from the ", callerName(caller),
        " refused, the weapon is reloaded with the hands, clip ", before));
      return 0;
    }

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
