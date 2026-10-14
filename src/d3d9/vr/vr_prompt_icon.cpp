#include "vr_controller_pad.h"
#include "vr_game_addresses.h"
#include "vr_game_memory.h"
#include "vr_prompt_icon.h"

namespace dxvk {

  namespace {

    using TextToTraitFn  = uint32_t (__cdecl*)(const char* name);
    using TileGetFloatFn = float (__thiscall*)(uintptr_t tile, uint32_t trait);
    using TileSetFloatFn = void (__thiscall*)(uintptr_t tile, uint32_t trait, float value, bool propagate);

    template<typename T>
    T readField(uintptr_t base, uintptr_t offset) {
      return *reinterpret_cast<const T*>(base + offset);
    }

    const uint8_t* gameBindings() {
      uintptr_t input = readField<uintptr_t>(VrGame::InputGlobals, 0);
      return input ? reinterpret_cast<const uint8_t*>(input + VrGame::InputControllerBindings) : nullptr;
    }

  }


  bool VrPromptIcon::initialize() {
    m_available = VrGameMemory::matches(VrGame::HudPromptIconSite,
        VrGame::HudPromptIconCode, sizeof(VrGame::HudPromptIconCode))
      && VrGameMemory::matches(VrGame::TileTextToTrait,
        VrGame::TileTextToTraitPrologue, sizeof(VrGame::TileTextToTraitPrologue))
      && VrGameMemory::matches(VrGame::TileGetFloat,
        VrGame::TileGetFloatPrologue, sizeof(VrGame::TileGetFloatPrologue))
      && VrGameMemory::matches(VrGame::TileSetFloat,
        VrGame::TileSetFloatPrologue, sizeof(VrGame::TileSetFloatPrologue))
      && VrGameMemory::readable(VrGame::HudMainMenu, sizeof(uintptr_t))
      && VrGameMemory::readable(VrGame::InputGlobals, sizeof(uintptr_t));
    return m_available;
  }


  void VrPromptIcon::show(VrHand hand) {
    if (!m_available)
      return;

    uintptr_t hud  = readField<uintptr_t>(VrGame::HudMainMenu, 0);
    uintptr_t tile = hud ? readField<uintptr_t>(hud, VrGame::HudPromptButtonTile) : 0;

    if (!tile)
      return;

    // The trait is created when the HUD's menu file is read
    if (!m_trait) {
      uint32_t trait = reinterpret_cast<TextToTraitFn>(VrGame::TileTextToTrait)(VrGame::PromptIconTraitName);

      if (trait == VrGame::TileTraitUnknown)
        return;

      m_trait = trait;
    }

    uint8_t input = VrGamepadInput::X;

    if (hand == VrHand::Right) {
      const uint8_t* bindings = gameBindings();

      if (!bindings)
        return;

      input = bindings[VrGameControl::Activate];
    }

    uint32_t icon = vrButtonIconForInput(input);

    if (!icon)
      return;

    if (reinterpret_cast<TileGetFloatFn>(VrGame::TileGetFloat)(tile, m_trait) != float(icon))
      reinterpret_cast<TileSetFloatFn>(VrGame::TileSetFloat)(tile, m_trait, float(icon), true);
  }

}
