#include <cstdint>
#include <utility>

#include "../../util/log/log.h"

#include "vr_game_addresses.h"
#include "vr_game_memory.h"
#include "vr_options_menu.h"

namespace dxvk {

  namespace {

    // The page's own bit in the options' masks. The game uses 0x01 to 0x40
    // for its pages and the main menu, and a few higher bits for its
    // confirmation lists.
    constexpr uint32_t PageMask = 0x4000;

    // UserOption types: 0 toggles and wraps around from the last value
    constexpr int32_t  ToggleType   = 0;
    constexpr uint32_t ToggleValues = 2;

    // Space between the arrows of a toggle, the same as the game's toggles
    // with long value names
    constexpr uint32_t ArrowSpacing = 0x55;

    // The game keeps these pointers and never frees them
    constexpr char PageText[]       = "VR";
    constexpr char TurnText[]       = "Turning";
    constexpr char TurnSnapText[]   = "Snap";
    constexpr char TurnSmoothText[] = "Smooth";

    using AppendFn   = int   (__fastcall*)(void* array, void* unused, void** option);
    using HeapFn     = void* (__cdecl*)(size_t size);
    using OptionCtorFn = void* (__thiscall*)(void* self, const char* text, void (__cdecl* callback)(), uint32_t mask);
    using UserOptionCallback = void (__cdecl*)(void* option);
    using UserOptionCtorFn = void* (__cdecl*)(void* self, const char* text, UserOptionCallback callback,
      uint32_t mask, int32_t type, uint32_t count, uint32_t spacing, const char* name0, const char* name1);
    using PopulateFn = void (__thiscall*)(void* menu, uint32_t mask);
    using SetEnabledFn = void (__thiscall*)(void* list, bool enabled);
    using GetFloatFn = float (__thiscall*)(void* tile, int32_t trait);
    using SetFloatFn = void (__thiscall*)(void* tile, int32_t trait, float value, bool propagate);
    using AddTraitFn = int32_t (__cdecl*)(const char* name, int32_t id);

    VrOptions                    g_options;
    VrOptionsMenu::ChangedFn     g_onChanged;

    uint8_t* startMenu() {
      return *reinterpret_cast<uint8_t* const*>(VrGame::StartMenuGlobal);
    }


    // The entry in the Settings list: the same steps as the game's entries
    // for its own pages, with the page's mask
    void __cdecl choosePage() {
      uint8_t* menu = startMenu();

      if (!menu)
        return;

      reinterpret_cast<SetEnabledFn>(VrGame::SetListEnabled)(menu + VrGame::StartMenuSettingsList, false);
      reinterpret_cast<PopulateFn>(VrGame::PopulateOptions)(menu, PageMask);

      // Showing a page ends by placing it with a value per page, which the
      // game only knows for its own pages
      void* tile = *reinterpret_cast<void**>(menu + VrGame::StartMenuTile);

      if (!tile)
        return;

      float center = reinterpret_cast<GetFloatFn>(VrGame::TileGetFloat)(tile, int32_t(VrGame::PageCenterTraitGameplay));
      int32_t trait = reinterpret_cast<AddTraitFn>(VrGame::AddUserTrait)(VrGame::CenterXTraitName, -1);

      if (trait != INT32_MIN)
        reinterpret_cast<SetFloatFn>(VrGame::TileSetFloat)(tile, trait, center, true);
    }


    // Called by the game with the option twice: when the options are
    // initialised from their settings, and when the player changes one
    void __cdecl onTurnChanged(void* option) {
      uint8_t* menu = startMenu();
      uint8_t* object = static_cast<uint8_t*>(option);
      uint32_t& value        = *reinterpret_cast<uint32_t*>(object + VrGame::UserOptionValue);
      uint32_t& defaultValue = *reinterpret_cast<uint32_t*>(object + VrGame::UserOptionDefaultValue);

      bool initialised = menu
        && (*reinterpret_cast<const uint32_t*>(menu + VrGame::StartMenuFlags) & VrGame::StartMenuOptionsInitialised);

      if (!initialised) {
        value        = g_options.smoothTurn ? 1u : 0u;
        defaultValue = 0u;
        return;
      }

      bool smooth = value != 0;

      if (smooth == g_options.smoothTurn)
        return;

      g_options.smoothTurn = smooth;

      if (g_onChanged)
        g_onChanged(g_options);
    }


    void appendPage(void* optionArray) {
      auto append = reinterpret_cast<AppendFn>(VrGame::OptionArrayAppend);
      auto allocate = reinterpret_cast<HeapFn>(VrGame::HeapAllocate);

      void* optionMemory = allocate(VrGame::OptionSize);
      void* userMemory   = allocate(VrGame::UserOptionSize);

      if (!optionMemory || !userMemory)
        return;

      void* option = reinterpret_cast<OptionCtorFn>(VrGame::OptionConstruct)(
        optionMemory, PageText, &choosePage, VrGame::OptionMaskSettingsList);

      void* turn = reinterpret_cast<UserOptionCtorFn>(VrGame::UserOptionConstruct)(
        userMemory, TurnText, &onTurnChanged, PageMask, ToggleType, ToggleValues, ArrowSpacing,
        TurnSnapText, TurnSmoothText);

      append(optionArray, nullptr, &option);
      append(reinterpret_cast<void*>(VrGame::UserOptionArray), nullptr, &turn);
    }


    // Replaces the call that adds the Controls entry to the Settings list:
    // adds it, then the VR entry and its options
    int __fastcall appendControlsEntry(void* optionArray, void* unused, void** option) {
      int index = reinterpret_cast<AppendFn>(VrGame::OptionArrayAppend)(optionArray, nullptr, option);
      appendPage(optionArray);
      return index;
    }


    bool supported() {
      return VrGameMemory::matches(VrGame::OptionConstruct,
          VrGame::OptionConstructPrologue, sizeof(VrGame::OptionConstructPrologue))
        && VrGameMemory::matches(VrGame::UserOptionConstruct,
          VrGame::UserOptionConstructPrologue, sizeof(VrGame::UserOptionConstructPrologue))
        && VrGameMemory::matches(VrGame::PopulateOptions,
          VrGame::PopulateOptionsPrologue, sizeof(VrGame::PopulateOptionsPrologue))
        && VrGameMemory::matches(VrGame::OptionArrayAppend,
          VrGame::OptionArrayAppendPrologue, sizeof(VrGame::OptionArrayAppendPrologue))
        && VrGameMemory::matches(VrGame::SetListEnabled,
          VrGame::SetListEnabledPrologue, sizeof(VrGame::SetListEnabledPrologue))
        && VrGameMemory::matches(VrGame::TileGetFloat,
          VrGame::TileGetFloatPrologue, sizeof(VrGame::TileGetFloatPrologue))
        && VrGameMemory::matches(VrGame::TileSetFloat,
          VrGame::TileSetFloatPrologue, sizeof(VrGame::TileSetFloatPrologue))
        && VrGameMemory::matches(VrGame::AddUserTrait,
          VrGame::AddUserTraitPrologue, sizeof(VrGame::AddUserTraitPrologue))
        && VrGameMemory::matches(VrGame::HeapAllocate,
          VrGame::HeapAllocatePrologue, sizeof(VrGame::HeapAllocatePrologue))
        && VrGameMemory::readable(VrGame::StartMenuGlobal, sizeof(uintptr_t));
    }

  }


  bool VrOptionsMenu::install(const VrOptions& options, ChangedFn onChanged) {
    if (!supported()) {
      Logger::warn("VR: The options page is not available, the executable is not the supported version");
      return false;
    }

    g_options   = options;
    g_onChanged = std::move(onChanged);

    if (!VrGameMemory::redirectCall(VrGame::ControlsOptionAppendCallSite,
        VrGame::OptionArrayAppend, reinterpret_cast<const void*>(&appendControlsEntry))) {
      Logger::warn("VR: The options page is not available, the settings list was not hooked");
      return false;
    }

    Logger::info("VR: Added the VR page to the Settings menu");
    return true;
  }

}
