#pragma once

#include <cstdint>

#include "vr_magazine.h"

namespace dxvk {

  /**
   * \brief Counts the objects below the first-person weapon node
   *
   * The weapon's model is attached below the node once the game has
   * loaded it, so the count tells when the parts can be read.
   *
   * \param [in] weapon The first-person weapon node (NiNode*)
   */
  uint32_t vrCountWeaponObjects(const uint8_t* weapon);

  /**
   * \brief Logs the moving parts of the weapon in the hand
   *
   * The reload animations move named parts of the weapon's model, whose
   * names start with "##" (magazine, slide, bolt, cylinder...), see
   * findings/reload.md §6. Each one is logged with its position along the
   * weapon node's axes, in centimetres, and the centre and radius of its
   * bound, with the projectile node for the barrel's direction.
   *
   * \param [in] weapon The first-person weapon node (NiNode*)
   * \param [in] form The equipped weapon (TESObjectWEAP*), or \c nullptr
   */
  void vrLogWeaponParts(const uint8_t* weapon, const uint8_t* form);

  /**
   * \brief Logs every object of the thrown weapon in the hand
   *
   * Grenades have no "##" parts: the names of all their objects tell
   * where a pin, fuse or button may be, see findings/throw.md. Each is
   * logged with its depth, whether it is culled, and the centre and
   * radius of its bound along the weapon node's axes, in centimetres.
   *
   * \param [in] weapon The first-person weapon node (NiNode*)
   * \param [in] form The equipped weapon (TESObjectWEAP*), or \c nullptr
   */
  void vrLogModelObjects(const uint8_t* weapon, const uint8_t* form);

  /**
   * \brief Finds a part of the weapon in the hand
   *
   * The part is told by its name, see vrClassifyWeaponPart. Only a part
   * with meshes counts (the 9mm pistol has an empty second magazine node),
   * and a node is preferred to its geometry, the nearest to the weapon
   * node first.
   *
   * \param [in] weapon The first-person weapon node (NiNode*)
   * \param [in] kind What the part is for
   * \returns The part (NiAVObject*), or \c nullptr if there is none
   */
  uint8_t* vrFindWeaponPart(uint8_t* weapon, VrWeaponPartKind kind);

}
