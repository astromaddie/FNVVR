#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace dxvk {

  /// Number of weapon types the game knows (TESObjectWEAP data, type byte)
  constexpr uint32_t VrWeaponTypeCount = 14;

  /**
   * \brief Returns the game's name of a weapon type
   *
   * The names are the game's own (the GECK's), from HandToHandMelee (0)
   * to OneHandThrown (13), see findings/developer-mode.md.
   *
   * \param [in] type Weapon type
   * \returns The name, or \c nullptr if the type is not a weapon type
   */
  const char* vrWeaponTypeName(uint32_t type);

  /**
   * \brief Reads a list of weapon type names
   *
   * Names are separated by commas or spaces and are not case sensitive.
   *
   * \param [in] list Names, such as \c "OneHandMelee,TwoHandMelee"
   * \param [out] unknown Words of the list that are not weapon type names
   * \returns Mask with bit \c 1 << type set for each type in the list
   */
  uint32_t vrParseWeaponTypes(const std::string& list, std::vector<std::string>& unknown);

}
