#include <algorithm>

#include "../../util/log/log.h"
#include "../../util/util_string.h"

#include "vr_developer_mode.h"
#include "vr_game_addresses.h"
#include "vr_game_memory.h"

namespace dxvk {

  namespace {

    using SetGodModeFn      = void     (__cdecl*)(bool enabled);
    using ContainerFn       = void     (__fastcall*)(uint8_t* container, void* unused);
    using AddObjectFn       = void*    (__fastcall*)(uint8_t* container, void* unused,
                                         uint8_t* object, int32_t count, void* extra);
    using AddToReferenceFn  = void     (__fastcall*)(uint8_t* container, void* unused,
                                         uint8_t* reference, bool silent);
    using GetChangesFn      = uint8_t* (__cdecl*)(uint8_t* reference);
    using ObjectCountFn     = int32_t  (__fastcall*)(uint8_t* changes, void* unused, uint8_t* object);
    using GetAmmoFn         = uint8_t* (__fastcall*)(uint8_t* ammoForm, void* unused);

    bool g_installed = false;

    template<typename T>
    T field(const uint8_t* object, uintptr_t offset) {
      return *reinterpret_cast<const T*>(object + offset);
    }

    uint8_t* player() {
      return *reinterpret_cast<uint8_t* const*>(VrGame::Player);
    }

  }


  VrDeveloperMode::VrDeveloperMode(uint32_t weaponTypes)
  : m_weaponTypes(weaponTypes) { }


  VrDeveloperMode::~VrDeveloperMode() {
    g_installed = false;
  }


  std::unique_ptr<VrDeveloperMode> VrDeveloperMode::install(uint32_t weaponTypes) {
    if (g_installed)
      return nullptr;

    bool found = VrGameMemory::matches(VrGame::SetGodMode,
        VrGame::SetGodModeCode, sizeof(VrGame::SetGodModeCode))
      && VrGameMemory::matches(VrGame::ContainerConstruct,
        VrGame::ContainerConstructPrologue, sizeof(VrGame::ContainerConstructPrologue))
      && VrGameMemory::matches(VrGame::ContainerDestruct,
        VrGame::ContainerConstructPrologue, sizeof(VrGame::ContainerConstructPrologue))
      && VrGameMemory::matches(VrGame::ContainerAddObject,
        VrGame::ContainerAddObjectPrologue, sizeof(VrGame::ContainerAddObjectPrologue))
      && VrGameMemory::matches(VrGame::ContainerAddToReference,
        VrGame::ContainerAddToReferencePrologue, sizeof(VrGame::ContainerAddToReferencePrologue))
      && VrGameMemory::matches(VrGame::GetInventoryChanges,
        VrGame::GetInventoryChangesPrologue, sizeof(VrGame::GetInventoryChangesPrologue))
      && VrGameMemory::matches(VrGame::InventoryObjectCount,
        VrGame::InventoryObjectCountPrologue, sizeof(VrGame::InventoryObjectCountPrologue))
      && VrGameMemory::matches(VrGame::AmmoFormGetAmmo,
        VrGame::AmmoFormGetAmmoPrologue, sizeof(VrGame::AmmoFormGetAmmoPrologue))
      && VrGameMemory::readable(VrGame::DataHandler, sizeof(uintptr_t));

    if (!found)
      return nullptr;

    g_installed = true;
    return std::unique_ptr<VrDeveloperMode>(new VrDeveloperMode(weaponTypes));
  }


  void VrDeveloperMode::update(VrGameStateKind state) {
    if (state == VrGameStateKind::Loading || state == VrGameStateKind::MainMenu) {
      m_pending = true;
      return;
    }

    if (!m_pending || state != VrGameStateKind::InGame)
      return;

    uint8_t* actor = player();

    if (!actor || !field<uint8_t*>(actor, VrGame::ActorProcess))
      return;

    m_pending = false;

    enableGodMode();
    addWeaponsAndAmmo();
  }


  void VrDeveloperMode::enableGodMode() {
    reinterpret_cast<SetGodModeFn>(VrGame::SetGodMode)(true);
    Logger::info("VR: Developer mode: god mode is on");
  }


  void VrDeveloperMode::addWeaponsAndAmmo() {
    const uint8_t* dataHandler = *reinterpret_cast<const uint8_t* const*>(VrGame::DataHandler);
    const uint8_t* objects = dataHandler ? field<const uint8_t*>(dataHandler, VrGame::DataHandlerObjectList) : nullptr;

    if (!objects) {
      Logger::warn("VR: Developer mode: the game's object list is empty, no weapon or ammunition was added");
      return;
    }

    uint8_t* actor   = player();
    uint8_t* changes = reinterpret_cast<GetChangesFn>(VrGame::GetInventoryChanges)(actor);

    alignas(uint32_t) uint8_t container[VrGame::ContainerSize];
    reinterpret_cast<ContainerFn>(VrGame::ContainerConstruct)(container, nullptr);

    std::vector<uint8_t*> ammoAdded;

    uint32_t added       = 0;
    uint32_t owned       = 0;
    uint32_t notPlayable = 0;

    for (uint8_t* object = field<uint8_t*>(objects, VrGame::ObjectListRoot); object;
         object = field<uint8_t*>(object, VrGame::ObjectNext)) {
      if (field<uint8_t>(object, VrGame::FormType) != VrGame::FormTypeWeapon
       || (field<uint32_t>(object, VrGame::FormFlags) & VrGame::FormFlagDeleted))
        continue;

      if (field<uint8_t>(object, VrGame::WeaponFlags) & VrGame::WeaponFlagNotPlayable) {
        notPlayable += 1;
        continue;
      }

      uint32_t type = field<uint8_t>(object, VrGame::WeaponAnimationType);

      if (changes && reinterpret_cast<ObjectCountFn>(VrGame::InventoryObjectCount)(changes, nullptr, object) != 0) {
        owned += 1;
      } else if (type < 32 && (m_weaponTypes & (1u << type))) {
        reinterpret_cast<AddObjectFn>(VrGame::ContainerAddObject)(container, nullptr, object, 1, nullptr);
        added += 1;
      } else {
        continue;
      }

      addAmmo(container, changes, object, ammoAdded);
    }

    if (added || !ammoAdded.empty())
      reinterpret_cast<AddToReferenceFn>(VrGame::ContainerAddToReference)(container, nullptr, actor, true);

    reinterpret_cast<ContainerFn>(VrGame::ContainerDestruct)(container, nullptr);

    Logger::info(str::format("VR: Developer mode: ", added, " weapons of the chosen types added to the inventory, ",
      owned, " weapons already there, ", notPlayable, " not playable left out, ",
      ammoAdded.size(), " kinds of ammunition added"));
  }


  void VrDeveloperMode::addAmmo(uint8_t* container, uint8_t* changes, uint8_t* weapon, std::vector<uint8_t*>& added) {
    uint8_t* ammo = reinterpret_cast<GetAmmoFn>(VrGame::AmmoFormGetAmmo)(weapon + VrGame::WeaponAmmoForm, nullptr);

    if (!ammo || field<uint8_t>(ammo, VrGame::FormType) != VrGame::FormTypeAmmo)
      return;

    if (std::find(added.begin(), added.end(), ammo) != added.end())
      return;

    if (changes && reinterpret_cast<ObjectCountFn>(VrGame::InventoryObjectCount)(changes, nullptr, ammo) != 0)
      return;

    int32_t count = std::max<int32_t>(field<uint8_t>(weapon, VrGame::WeaponClipRounds), 1);
    reinterpret_cast<AddObjectFn>(VrGame::ContainerAddObject)(container, nullptr, ammo, count, nullptr);

    added.push_back(ammo);
  }

}
