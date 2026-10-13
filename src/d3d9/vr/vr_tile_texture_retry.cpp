#include <windows.h>

#include "../../util/log/log.h"
#include "../../util/util_string.h"

#include "vr_game_addresses.h"
#include "vr_game_memory.h"
#include "vr_tile_texture_retry.h"

namespace dxvk {

  namespace {

    using GetTypeFn        = uint32_t  (__fastcall*)(uintptr_t tile, void* unused);
    using GetValueFn       = uintptr_t (__fastcall*)(uintptr_t tile, void* unused, uint32_t trait);
    using GetPropertyFn    = uintptr_t (__fastcall*)(uintptr_t object, void* unused, int32_t type);
    using AddNeedsUpdateFn = void      (__fastcall*)(uintptr_t tile, void* unused, uint32_t flags);

    constexpr uint32_t MaxDepth  = 32;
    constexpr uint32_t MaxLogged = 20;

    template<typename T>
    T readField(uintptr_t base, uintptr_t offset) {
      return *reinterpret_cast<const T*>(base + offset);
    }

    uintptr_t imageTexture(uintptr_t node) {
      if (!readField<uint16_t>(node, VrGame::NodeChildCount))
        return 0;

      uintptr_t geometry = readField<uintptr_t>(readField<uintptr_t>(node, VrGame::NodeChildren), 0);

      if (!geometry)
        return 0;

      uintptr_t shader = reinterpret_cast<GetPropertyFn>(VrGame::ObjectGetProperty)(
        geometry, nullptr, VrGame::PropertyTypeShade);

      return shader ? readField<uintptr_t>(shader, VrGame::TileShaderTexture) : 0;
    }

  }


  bool VrTileTextureRetry::initialize() {
    m_available = VrGameMemory::matches(VrGame::TileGetValue,
        VrGame::TileGetValuePrologue, sizeof(VrGame::TileGetValuePrologue))
      && VrGameMemory::matches(VrGame::TileAddNeedsUpdate,
        VrGame::TileAddNeedsUpdatePrologue, sizeof(VrGame::TileAddNeedsUpdatePrologue))
      && VrGameMemory::matches(VrGame::ObjectGetProperty,
        VrGame::ObjectGetPropertyPrologue, sizeof(VrGame::ObjectGetPropertyPrologue))
      && VrGameMemory::matches(VrGame::MenuTileReadSite,
        VrGame::MenuTileRead, sizeof(VrGame::MenuTileRead));

    if (!m_available)
      Logger::info("VR: The menu tile functions were not found, menu images left without a texture are not reloaded");

    return m_available;
  }


  void VrTileTextureRetry::update() {
    if (!m_available)
      return;

    auto lock = reinterpret_cast<LPCRITICAL_SECTION>(VrGame::TileUpdateLock);
    EnterCriticalSection(lock);

    uintptr_t tiles = readField<uintptr_t>(VrGame::MenuTiles, 0);
    uint32_t  menus = readField<uint16_t>(VrGame::MenuTileCount, 0);

    m_stillMissing.clear();

    for (uint32_t i = 0; tiles && i < menus; i++) {
      uintptr_t menu = readField<uintptr_t>(tiles, i * sizeof(uintptr_t));

      if (menu)
        visit(menu, 0);
    }

    std::swap(m_requested, m_stillMissing);

    LeaveCriticalSection(lock);
  }


  void VrTileTextureRetry::visit(uintptr_t tile, uint32_t depth) {
    uintptr_t node = readField<uintptr_t>(tile, VrGame::TileNode);

    // A hidden tile hides its children too
    if (depth > MaxDepth || !node
     || (readField<uint32_t>(node, VrGame::ObjectFlags) & VrGame::ObjectFlagAppCulled))
      return;

    auto     getType = reinterpret_cast<GetTypeFn>(readField<uintptr_t>(readField<uintptr_t>(tile, 0), VrGame::TileGetTypeSlot));
    uint32_t flags   = readField<uint32_t>(tile, VrGame::TileFlags);

    if (getType(tile, nullptr) == VrGame::TileTypeImage
     && !(flags & VrGame::TileUpdateTexture)
     && reinterpret_cast<GetValueFn>(VrGame::TileGetValue)(tile, nullptr, VrGame::TraitTextureAtlas)
     && !imageTexture(node)) {
      if (!m_requested.count(tile)) {
        reinterpret_cast<AddNeedsUpdateFn>(VrGame::TileAddNeedsUpdate)(tile, nullptr, VrGame::TileUpdateTexture);

        if (m_logged < MaxLogged) {
          const char* name = readField<const char*>(tile, VrGame::TileName);
          Logger::info(str::format("VR: The atlas image '", name ? name : "", "' is shown without a texture, its texture is requested again"));
          m_logged++;
        }
      }

      m_stillMissing.insert(tile);
    }

    for (uintptr_t entry = readField<uintptr_t>(tile, VrGame::TileChildren); entry; entry = readField<uintptr_t>(entry, 0)) {
      uintptr_t child = readField<uintptr_t>(entry, 8);

      if (child)
        visit(child, depth + 1);
    }
  }

}
