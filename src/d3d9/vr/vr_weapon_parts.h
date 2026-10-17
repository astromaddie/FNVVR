#pragma once

#include <cstdint>

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

}
