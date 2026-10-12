#include "../../util/log/log.h"
#include "../../util/util_string.h"

#include "vr_game_addresses.h"
#include "vr_game_memory.h"
#include "vr_weapon_drawn.h"

namespace dxvk {

  namespace {

    using GetGroupActionFn = uint32_t (__fastcall*)(uint8_t* animData, void* unused, int32_t section);
    using SetWantDrawnFn   = void (__fastcall*)(uint8_t* actor, void* unused, bool drawn);
    using GetWantDrawnFn   = bool (__fastcall*)(uint8_t* actor, void* unused);
    using SitStateFn       = int32_t (__fastcall*)(uint8_t* actor, void* unused);
    using ProcessIntFn     = int32_t (__fastcall*)(uint8_t* process, void* unused);
    using ProcessBoolFn    = bool (__fastcall*)(uint8_t* process, void* unused);
    using GetAnimationFn   = uint8_t* (__fastcall*)(uint8_t* actor, void* unused);

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

    uint8_t* process = actor ? field<uint8_t*>(actor, VrGame::ActorProcess) : nullptr;

    if (!process)
      return;

    logFurnitureState(actor, process);

    if (reinterpret_cast<GetWantDrawnFn>(VrGame::GetWantWeaponDrawn)(actor, nullptr))
      return;

    // The game holsters the weapon on purpose in these cases
    if (field<uint8_t>(actor, VrGame::PlayerDisabledControls) & VrGame::ControlFighting)
      return;

    if (field<uint32_t>(actor, VrGame::ActorLifeState) != 0)
      return;

    // The game holsters the weapon before it starts sitting down, while the
    // sit state is still 0
    if (field<uint8_t*>(process, VrGame::ProcessFurniture))
      return;

    if (virtualFunction<SitStateFn>(actor, VrGame::GetSitStateSlot)(actor, nullptr) != 0)
      return;

    reinterpret_cast<SetWantDrawnFn>(VrGame::SetWantWeaponDrawn)(actor, nullptr, true);

    if (!m_loggedDraw) {
      Logger::info("VR: The player's weapon is drawn again");
      m_loggedDraw = true;
    }
  }


  void VrWeaponDrawn::logFurnitureState(uint8_t* actor, uint8_t* process) {
    // Diagnostic for sitting with the weapon kept drawn: logs each change of
    // the states MiddleHighProcess::SitOrSleep waits on
    uint8_t* furniture = field<uint8_t*>(process, VrGame::ProcessFurniture);
    uint8_t  sitState  = field<uint8_t>(process, VrGame::ProcessSitSleepState);

    if (!furniture && !sitState) {
      m_furnitureState.clear();
      return;
    }

    uint8_t* animData = virtualFunction<GetAnimationFn>(actor, VrGame::ActorGetAnimationSlot)(actor, nullptr);

    uint32_t weaponGroup  = animData ? field<uint16_t>(animData, VrGame::AnimDataSectionGroups + VrGame::AnimSectionWeapon * sizeof(uint16_t)) : 0xFFFFu;
    uint32_t weaponAction = animData ? reinterpret_cast<GetGroupActionFn>(VrGame::AnimGetGroupAction)(animData, nullptr, VrGame::AnimSectionWeapon) : 0xFFFFu;

    std::string state = str::format(
      "furniture ", furniture ? "yes" : "no",
      ", sit state ", uint32_t(sitState),
      ", anim action ", virtualFunction<ProcessIntFn>(process, VrGame::ProcessAnimActionSlot)(process, nullptr),
      ", weapon drawn ", uint32_t(virtualFunction<ProcessBoolFn>(process, VrGame::ProcessIsWeaponOutSlot)(process, nullptr)),
      ", want drawn ", uint32_t(reinterpret_cast<GetWantDrawnFn>(VrGame::GetWantWeaponDrawn)(actor, nullptr)),
      ", alert ", uint32_t(virtualFunction<ProcessBoolFn>(process, VrGame::ProcessIsAlertSlot)(process, nullptr)),
      ", weapon group 0x", std::hex, weaponGroup, std::dec,
      ", weapon group action ", weaponAction);

    if (state != m_furnitureState) {
      Logger::info(str::format("VR: Furniture: ", state));
      m_furnitureState = std::move(state);
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

    // Sitting and sleeping wait for the actor's holster action to end, which
    // only happens once the holster sequence has really finished
    uint8_t* process = field<uint8_t*>(player(), VrGame::ActorProcess);

    if (group == VrGame::AnimGroupUnequip && process && field<uint8_t*>(process, VrGame::ProcessFurniture)) {
      if (!g_weaponDrawn->m_loggedFurniture) {
        Logger::info("VR: The weapon is holstered with its animation before using furniture");
        g_weaponDrawn->m_loggedFurniture = true;
      }

      return action;
    }

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
