#include <cstring>

#include "vr_game_addresses.h"
#include "vr_game_nodes.h"

namespace dxvk {

  namespace {

    using GetAsNodeFn = uintptr_t (__fastcall*)(uintptr_t object, void* unused);

    // TESObjectREFR::Get3D, __thiscall without arguments
    using Get3DFn = uintptr_t (__fastcall*)(uintptr_t reference, void* unused);

    // The head bone is about ten levels below a character's root
    constexpr uint32_t MaxSkeletonDepth = 16;

    template<typename T>
    T readField(uintptr_t base, uintptr_t offset) {
      return *reinterpret_cast<const T*>(base + offset);
    }

  }


  uintptr_t vrGameAsNode(uintptr_t object) {
    auto vtable = readField<uintptr_t>(object, 0);
    return reinterpret_cast<GetAsNodeFn>(readField<uintptr_t>(vtable, VrGame::ObjectGetAsNodeSlot))(object, nullptr);
  }


  uintptr_t vrFindGameObject(uintptr_t root, const char* name, uint32_t maxDepth) {
    if (!root)
      return 0;

    auto objectName = readField<const char*>(root, VrGame::ObjectName);

    if (objectName && !std::strcmp(objectName, name))
      return root;

    uintptr_t node = vrGameAsNode(root);

    if (!node || !maxDepth)
      return 0;

    auto children = readField<const uintptr_t*>(node, VrGame::NodeChildren);
    auto count    = readField<uint16_t>(node, VrGame::NodeChildCount);

    for (uint32_t i = 0; children && i < count; i++) {
      if (uintptr_t found = vrFindGameObject(children[i], name, maxDepth - 1))
        return found;
    }

    return 0;
  }


  void vrVisitGameObjects(uintptr_t root, uint32_t maxDepth,
    const std::function<void (uintptr_t object, uint32_t depth)>& visit) {
    std::function<void (uintptr_t, uint32_t)> walk = [&] (uintptr_t object, uint32_t depth) {
      visit(object, depth);

      uintptr_t node = vrGameAsNode(object);

      if (!node || depth == maxDepth)
        return;

      auto children = readField<const uintptr_t*>(node, VrGame::NodeChildren);
      auto count    = readField<uint16_t>(node, VrGame::NodeChildCount);

      for (uint32_t i = 0; children && i < count; i++) {
        if (children[i])
          walk(children[i], depth + 1);
      }
    };

    if (root)
      walk(root, 0);
  }


  bool vrFindActorHead(uintptr_t actor, VrVector3& position) {
    if (!actor)
      return false;

    auto vtable = readField<uintptr_t>(actor, 0);
    uintptr_t root = reinterpret_cast<Get3DFn>(readField<uintptr_t>(vtable, VrGame::RefGet3DSlot))(actor, nullptr);

    uintptr_t head = vrFindGameObject(root, VrGame::HeadNodeName, MaxSkeletonDepth);

    if (!head)
      head = vrFindGameObject(root, VrGame::SpeakerNodeName, MaxSkeletonDepth);

    if (!head)
      return false;

    std::memcpy(&position, reinterpret_cast<const void*>(head + VrGame::ObjectWorldTranslation), sizeof(position));
    return true;
  }

}
