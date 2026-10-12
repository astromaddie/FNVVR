#include "../../util/log/log.h"

#include "vr_game_addresses.h"
#include "vr_game_memory.h"
#include "vr_weapon_drawn.h"

namespace dxvk {

  namespace {

    using GetGroupActionFn = uint32_t (__fastcall*)(uint8_t* animData, void* unused, int32_t section);
    using SetWantDrawnFn   = void (__fastcall*)(uint8_t* actor, void* unused, bool drawn);
    using GetWantDrawnFn   = bool (__fastcall*)(uint8_t* actor, void* unused);
    using SitStateFn       = int32_t (__fastcall*)(uint8_t* actor, void* unused);

    VrWeaponDrawn* g_weaponDrawn = nullptr;

    template<typename T>
    T field(const uint8_t* object, uintptr_t offset) {
      return *reinterpret_cast<const T*>(object + offset);
    }

    template<typename Fn>
    Fn virtualFunction(const uint8_t* object, uintptr_t slot) {
      auto vtable = field<const uint8_t*>(object, 0);
      return reinterpret_cast<Fn>(field<uintptr_t>(vtable, slot));
    }

    uint8_t* player() {
      return *reinterpret_cast<uint8_t* const*>(VrGame::Player);
    }

  }


  VrWeaponDrawn::~VrWeaponDrawn() {
    // The patched calls stay in place and fall through to the game's functions
    g_weaponDrawn = nullptr;
  }


  std::unique_ptr<VrWeaponDrawn> VrWeaponDrawn::install() {
    if (g_weaponDrawn)
      return nullptr;

    // Everything is checked before anything is patched, so that the game
    // never keeps only part of the behaviour
    bool found = VrGameMemory::callsTo(VrGame::EquipWaitCallSite, VrGame::AnimGetGroupAction)
      && VrGameMemory::callsTo(VrGame::HolsterInputCallSite, VrGame::SetWantWeaponDrawn)
      && VrGameMemory::matches(VrGame::SetWantWeaponDrawn,
        VrGame::SetWantWeaponDrawnPrologue, sizeof(VrGame::SetWantWeaponDrawnPrologue))
      && VrGameMemory::matches(VrGame::GetWantWeaponDrawn,
        VrGame::GetWantWeaponDrawnPrologue, sizeof(VrGame::GetWantWeaponDrawnPrologue));

    found = found
      && VrGameMemory::redirectCall(VrGame::EquipWaitCallSite, VrGame::AnimGetGroupAction,
        reinterpret_cast<const void*>(&VrWeaponDrawn::equipWaitHook))
      && VrGameMemory::redirectCall(VrGame::HolsterInputCallSite, VrGame::SetWantWeaponDrawn,
        reinterpret_cast<const void*>(&VrWeaponDrawn::holsterInputHook));

    if (!found)
      return nullptr;

    std::unique_ptr<VrWeaponDrawn> weaponDrawn(new VrWeaponDrawn());
    g_weaponDrawn = weaponDrawn.get();
    return weaponDrawn;
  }


  void VrWeaponDrawn::update() {
    uint8_t* actor = player();

    if (!actor || !field<uint8_t*>(actor, VrGame::ActorProcess))
      return;

    if (reinterpret_cast<GetWantDrawnFn>(VrGame::GetWantWeaponDrawn)(actor, nullptr))
      return;

    // The game holsters the weapon on purpose in these cases
    if (field<uint8_t>(actor, VrGame::PlayerDisabledControls) & VrGame::ControlFighting)
      return;

    if (field<uint32_t>(actor, VrGame::ActorLifeState) != 0)
      return;

    if (virtualFunction<SitStateFn>(actor, VrGame::GetSitStateSlot)(actor, nullptr) != 0)
      return;

    reinterpret_cast<SetWantDrawnFn>(VrGame::SetWantWeaponDrawn)(actor, nullptr, true);

    if (!m_loggedDraw) {
      Logger::info("VR: The player's weapon is drawn again");
      m_loggedDraw = true;
    }
  }


  uint32_t __fastcall VrWeaponDrawn::equipWaitHook(
          uint8_t*              animData,
          void*                 unused,
          int32_t               section) {
    uint32_t action = reinterpret_cast<GetGroupActionFn>(VrGame::AnimGetGroupAction)(animData, nullptr, section);

    if (!g_weaponDrawn || !animData || section != VrGame::AnimSectionWeapon
     || action >= VrGame::AnimActionStop || field<uint8_t*>(animData, VrGame::AnimDataActor) != player())
      return action;

    uintptr_t slot = section * sizeof(void*);

    if (!field<uint8_t*>(animData, VrGame::AnimDataSectionSequences + slot))
      return action;

    uint8_t group = uint8_t(field<uint16_t>(animData, VrGame::AnimDataSectionGroups + section * sizeof(uint16_t)));

    if (group != VrGame::AnimGroupEquip && group != VrGame::AnimGroupUnequip)
      return action;

    if (!g_weaponDrawn->m_loggedEquip) {
      Logger::info("VR: The weapon is drawn and holstered without animation");
      g_weaponDrawn->m_loggedEquip = true;
    }

    return VrGame::AnimActionStop;
  }


  void __fastcall VrWeaponDrawn::holsterInputHook(
          uint8_t*              actor,
          void*                 unused,
          bool                  drawn) {
    if (g_weaponDrawn && !drawn) {
      if (!g_weaponDrawn->m_loggedHolster) {
        Logger::info("VR: Holding the reload control does not holster the weapon");
        g_weaponDrawn->m_loggedHolster = true;
      }

      return;
    }

    reinterpret_cast<SetWantDrawnFn>(VrGame::SetWantWeaponDrawn)(actor, nullptr, drawn);
  }

}
