#pragma once

#include <cstdint>
#include <functional>

#include "vr_types.h"

namespace dxvk {

  /**
   * \brief Returns an object as a node, or 0 if it has no children (geometry)
   *
   * \param [in] object NiAVObject* of the game
   */
  uintptr_t vrGameAsNode(uintptr_t object);

  /**
   * \brief Finds an object by name in a subtree of the game's scene graph
   *
   * \param [in] root NiAVObject* to search from, itself included
   * \param [in] name Name of the object
   * \param [in] maxDepth Depth below the root to search to
   * \returns The first object found depth first, or 0
   */
  uintptr_t vrFindGameObject(uintptr_t root, const char* name, uint32_t maxDepth);

  /**
   * \brief Visits every object of a subtree of the game's scene graph
   *
   * \param [in] root NiAVObject* to start from, itself included
   * \param [in] maxDepth Depth below the root to visit to
   * \param [in] visit Called depth first with each object and its depth
   *    below the root
   */
  void vrVisitGameObjects(uintptr_t root, uint32_t maxDepth,
    const std::function<void (uintptr_t object, uint32_t depth)>& visit);

  /**
   * \brief Finds where an actor's head is, the way the dialogue camera does
   *
   * \param [in] actor The game's actor (TESObjectREFR*)
   * \param [out] position World position of its head bone, in game units
   * \returns \c false if the actor is not loaded or has no head bone
   */
  bool vrFindActorHead(uintptr_t actor, VrVector3& position);

}
