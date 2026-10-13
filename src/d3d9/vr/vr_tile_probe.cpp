#include <windows.h>

#include <cstring>

#include "../../util/log/log.h"
#include "../../util/util_string.h"

#include "vr_game_addresses.h"
#include "vr_game_memory.h"
#include "vr_tile_probe.h"

namespace dxvk {

  namespace {

    using GetValueFn    = uintptr_t (__fastcall*)(uintptr_t tile, void* unused, uint32_t trait);
    using GetPropertyFn = uintptr_t (__fastcall*)(uintptr_t object, void* unused, int32_t type);

    constexpr uint32_t MaxLines = 2000;
    constexpr uint32_t MaxDepth = 32;

    template<typename T>
    T readField(uintptr_t base, uintptr_t offset) {
      return *reinterpret_cast<const T*>(base + offset);
    }

    uintptr_t getValue(uintptr_t tile, uint32_t trait) {
      return reinterpret_cast<GetValueFn>(VrGame::TileGetValue)(tile, nullptr, trait);
    }

    float getNumber(uintptr_t tile, uint32_t trait) {
      uintptr_t value = getValue(tile, trait);
      return value ? readField<float>(value, 8) : -1.0f;
    }

    const char* getString(uintptr_t tile, uint32_t trait) {
      uintptr_t value = getValue(tile, trait);
      return value ? readField<const char*>(value, 0xC) : nullptr;
    }

    // Name of the first app-culled node from the tile's node up, or "none"
    std::string culledChain(uintptr_t node) {
      uint32_t level = 0;

      for (; node; node = readField<uintptr_t>(node, VrGame::ObjectParent), level++) {
        if (readField<uint32_t>(node, VrGame::ObjectFlags) & VrGame::ObjectFlagAppCulled) {
          const char* name = readField<const char*>(node, VrGame::ObjectName);
          return str::format("level ", level, " (", name ? name : "?", ")");
        }
      }

      return "none";
    }

  }


  bool VrTileProbe::initialize() {
    m_available = VrGameMemory::matches(VrGame::TileGetValue,
        VrGame::TileGetValuePrologue, sizeof(VrGame::TileGetValuePrologue))
      && VrGameMemory::matches(VrGame::ObjectGetProperty,
        VrGame::ObjectGetPropertyPrologue, sizeof(VrGame::ObjectGetPropertyPrologue))
      && VrGameMemory::matches(VrGame::MenuTileReadSite,
        VrGame::MenuTileRead, sizeof(VrGame::MenuTileRead));

    Logger::info(str::format("VR: Button icon probe ", m_available ? "enabled" : "disabled, unexpected executable"));
    return m_available;
  }


  void VrTileProbe::sample() {
    if (!m_available || m_lines >= MaxLines)
      return;

    auto lock = reinterpret_cast<LPCRITICAL_SECTION>(VrGame::TileUpdateLock);
    EnterCriticalSection(lock);

    if (!m_loggedStart) {
      Logger::info("VR: Button icon probe started");
      m_loggedStart = true;
    }

    uintptr_t tiles = readField<uintptr_t>(VrGame::MenuTiles, 0);
    uint32_t  menus = readField<uint16_t>(VrGame::MenuTileCount, 0);

    for (uint32_t i = 0; tiles && i < menus; i++) {
      uintptr_t menu = readField<uintptr_t>(tiles, i * sizeof(uintptr_t));

      if (menu)
        walk(menu, VrGame::FirstMenuId + i, 0);
    }

    LeaveCriticalSection(lock);
  }


  void VrTileProbe::walk(uintptr_t tile, uint32_t menuId, uint32_t depth) {
    if (depth > MaxDepth)
      return;

    const char* name = readField<const char*>(tile, VrGame::TileName);

    if (name && !std::strcmp(name, "xbox_button"))
      probeIcon(tile, menuId);

    for (uintptr_t node = readField<uintptr_t>(tile, VrGame::TileChildren); node; node = readField<uintptr_t>(node, 0)) {
      uintptr_t child = readField<uintptr_t>(node, 8);

      if (child)
        walk(child, menuId, depth + 1);
    }
  }


  void VrTileProbe::probeIcon(uintptr_t tile, uint32_t menuId) {
    uintptr_t node    = readField<uintptr_t>(tile, VrGame::TileNode);
    uint32_t  flags   = readField<uint32_t>(tile, VrGame::TileFlags);
    uintptr_t texture = 0;

    if (node && readField<uint16_t>(node, VrGame::NodeChildCount)) {
      uintptr_t geometry = readField<uintptr_t>(readField<uintptr_t>(node, VrGame::NodeChildren), 0);
      uintptr_t shader   = geometry
        ? reinterpret_cast<GetPropertyFn>(VrGame::ObjectGetProperty)(geometry, nullptr, 3)
        : 0;

      if (shader)
        texture = readField<uintptr_t>(shader, VrGame::TileShaderTexture);
    }

    const char* filename = getString(tile, VrGame::TraitFilename);

    std::string state = str::format("file ", filename && *filename ? filename : "(none)",
      " flags 0x", std::hex, flags, std::dec,
      " visible ", getNumber(tile, VrGame::TraitVisible),
      " menuVisible ", getNumber(tile, VrGame::TraitMenuVisible),
      " culled ", culledChain(node),
      " texture ", texture ? "set" : "NONE");

    auto entry = m_states.find(tile);

    if (entry != m_states.end() && entry->second == state)
      return;

    m_states[tile] = state;

    Logger::info(str::format("VR: Icon menu 0x", std::hex, menuId, " tile 0x", tile, std::dec, ": ", state,
      " hibernateCheck ", uint32_t(readField<uint8_t>(VrGame::TileNeedsHibernateCheck, 0))));

    m_lines++;
  }

}
