#include "../../util/log/log.h"
#include "../../util/util_string.h"

#include "vr_game_addresses.h"
#include "vr_game_memory.h"
#include "vr_xp_meter.h"

namespace dxvk {

  namespace {

    using TileGetFloatFn = float (__thiscall*)(uintptr_t tile, uint32_t trait);
    using TileSetFloatFn = void (__thiscall*)(uintptr_t tile, uint32_t trait, float value, bool propagate);

    template<typename T>
    T readField(uintptr_t base, uintptr_t offset) {
      return *reinterpret_cast<const T*>(base + offset);
    }

  }


  bool VrXpMeter::initialize() {
    m_available = VrGameMemory::matches(VrGame::HudXpMeterStoreSite,
        VrGame::HudXpMeterStore, sizeof(VrGame::HudXpMeterStore))
      && VrGameMemory::matches(VrGame::HudXpMeterBaseReadSite,
        VrGame::HudXpMeterBaseRead, sizeof(VrGame::HudXpMeterBaseRead))
      && VrGameMemory::matches(VrGame::HudMessagesStoreSite,
        VrGame::HudMessagesStore, sizeof(VrGame::HudMessagesStore))
      && VrGameMemory::matches(VrGame::TileGetFloat,
        VrGame::TileGetFloatPrologue, sizeof(VrGame::TileGetFloatPrologue))
      && VrGameMemory::matches(VrGame::TileSetFloat,
        VrGame::TileSetFloatPrologue, sizeof(VrGame::TileSetFloatPrologue))
      && VrGameMemory::readable(VrGame::HudMainMenu, sizeof(uintptr_t));
    return m_available;
  }


  bool VrXpMeter::place() {
    if (!m_available)
      return false;

    uintptr_t hud      = readField<uintptr_t>(VrGame::HudMainMenu, 0);
    uintptr_t meter    = hud ? readField<uintptr_t>(hud, VrGame::HudXpMeter) : 0;
    uintptr_t messages = hud ? readField<uintptr_t>(hud, VrGame::HudMessages) : 0;

    if (!meter || !messages)
      return false;

    auto getFloat = reinterpret_cast<TileGetFloatFn>(VrGame::TileGetFloat);
    auto setFloat = reinterpret_cast<TileSetFloatFn>(VrGame::TileSetFloat);

    // Only HUDMainMenu::Create sets the messages' height
    float target = getFloat(messages, VrGame::TileTraitY);
    float base   = getFloat(meter, VrGame::TileTraitXpMeterBaseY);

    if (base == target)
      return true;

    // The meter may be raised from its base by the HUD's visibility mode,
    // which stays the same
    float y = getFloat(meter, VrGame::TileTraitY);
    setFloat(meter, VrGame::TileTraitXpMeterBaseY, target, true);
    setFloat(meter, VrGame::TileTraitY, y - base + target, true);

    if (!m_loggedMove) {
      Logger::info(str::format("VR: The XP meter is moved from height ", base, " to ", target,
        ", the row of the HUD messages, to be shown in front of the head"));
      m_loggedMove = true;
    }

    return true;
  }

}
