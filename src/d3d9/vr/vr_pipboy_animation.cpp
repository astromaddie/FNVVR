#include <cmath>

#include "../../util/log/log.h"

#include "vr_game_addresses.h"
#include "vr_game_memory.h"
#include "vr_pipboy_animation.h"

namespace dxvk {

  namespace {

    using GetGroupActionFn = uint32_t (__fastcall*)(uint8_t* animData, void* unused, int32_t section);
    using IsPipBoyActiveFn = bool (__fastcall*)(uint8_t* player, void* unused);
    using ClearGroupFn     = void (__fastcall*)(uint8_t* animData, void* unused, int32_t section, float blend);

    VrPipBoyAnimation* g_pipBoyAnimation = nullptr;

    template<typename T>
    T& field(uint8_t* object, uintptr_t offset) {
      return *reinterpret_cast<T*>(object + offset);
    }

    uint8_t* sectionSequence(uint8_t* animData, int32_t section) {
      return field<uint8_t*>(animData, VrGame::AnimDataSectionSequences + section * sizeof(void*));
    }

    uint8_t sectionGroup(uint8_t* animData, int32_t section) {
      return uint8_t(field<uint16_t>(animData, VrGame::AnimDataSectionGroups + section * sizeof(uint16_t)));
    }

  }


  VrPipBoyAnimation::~VrPipBoyAnimation() {
    // The patched calls stay in place and fall through to the game's functions
    g_pipBoyAnimation = nullptr;
  }


  std::unique_ptr<VrPipBoyAnimation> VrPipBoyAnimation::install() {
    if (g_pipBoyAnimation)
      return nullptr;

    // Both sites are checked before either is patched, so that the game
    // never skips only one of the two animations
    bool found = VrGameMemory::callsTo(VrGame::PipBoyRaiseWaitCallSite, VrGame::AnimGetGroupAction)
      && VrGameMemory::callsTo(VrGame::PipBoyLowerWaitCallSite, VrGame::IsPipBoyActive)
      && VrGameMemory::matches(VrGame::AnimClearGroup,
        VrGame::AnimClearGroupPrologue, sizeof(VrGame::AnimClearGroupPrologue));

    found = found
      && VrGameMemory::redirectCall(VrGame::PipBoyRaiseWaitCallSite, VrGame::AnimGetGroupAction,
        reinterpret_cast<const void*>(&VrPipBoyAnimation::raiseWaitHook))
      && VrGameMemory::redirectCall(VrGame::PipBoyLowerWaitCallSite, VrGame::IsPipBoyActive,
        reinterpret_cast<const void*>(&VrPipBoyAnimation::lowerWaitHook));

    if (!found)
      return nullptr;

    std::unique_ptr<VrPipBoyAnimation> animation(new VrPipBoyAnimation());
    g_pipBoyAnimation = animation.get();
    return animation;
  }


  uint32_t __fastcall VrPipBoyAnimation::raiseWaitHook(
          uint8_t*              animData,
          void*                 unused,
          int32_t               section) {
    uint32_t action = reinterpret_cast<GetGroupActionFn>(VrGame::AnimGetGroupAction)(animData, nullptr, section);

    if (!g_pipBoyAnimation || !animData || section != VrGame::AnimSectionLeftArm
     || action == VrGame::AnimActionStop)
      return action;

    // The game computes the arm's final pose from the sequence's offset,
    // which is only set once the sequence has been updated
    uint8_t* sequence = sectionSequence(animData, section);

    if (!sequence || sectionGroup(animData, section) != VrGame::AnimGroupPipBoy)
      return action;

    float offset = field<float>(sequence, VrGame::SequenceOffset);

    if (offset == 0.0f || !std::isfinite(offset))
      return action;

    if (!g_pipBoyAnimation->m_loggedRaise) {
      Logger::info("VR: The Pip-Boy opens without the arm raise animation");
      g_pipBoyAnimation->m_loggedRaise = true;
    }

    return VrGame::AnimActionStop;
  }


  bool __fastcall VrPipBoyAnimation::lowerWaitHook(
          uint8_t*              player,
          void*                 unused) {
    uint8_t* animData = player ? field<uint8_t*>(player, VrGame::PlayerFirstPersonAnimData) : nullptr;

    if (g_pipBoyAnimation && animData && sectionSequence(animData, VrGame::AnimSectionLeftArm)) {
      uint8_t group = sectionGroup(animData, VrGame::AnimSectionLeftArm);

      if (group == VrGame::AnimGroupPipBoy || group == VrGame::AnimGroupPipBoyChild) {
        reinterpret_cast<ClearGroupFn>(VrGame::AnimClearGroup)(
          animData, nullptr, VrGame::AnimSectionLeftArm, 0.0f);

        if (!g_pipBoyAnimation->m_loggedLower) {
          Logger::info("VR: The Pip-Boy closes without the arm lower animation");
          g_pipBoyAnimation->m_loggedLower = true;
        }
      }
    }

    return reinterpret_cast<IsPipBoyActiveFn>(VrGame::IsPipBoyActive)(player, nullptr);
  }

}
