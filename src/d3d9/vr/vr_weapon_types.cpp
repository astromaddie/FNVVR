#include <algorithm>
#include <cctype>

#include "vr_weapon_types.h"

namespace dxvk {

  namespace {

    constexpr const char* WeaponTypeNames[VrWeaponTypeCount] = {
      "HandToHandMelee",
      "OneHandMelee",
      "TwoHandMelee",
      "OneHandPistol",
      "OneHandPistolEnergy",
      "TwoHandRifle",
      "TwoHandAutomatic",
      "TwoHandRifleEnergy",
      "TwoHandHandle",
      "TwoHandLauncher",
      "OneHandGrenade",
      "OneHandMine",
      "OneHandLunchboxMine",
      "OneHandThrown",
    };


    bool isSeparator(char c) {
      return c == ',' || std::isspace(static_cast<unsigned char>(c));
    }


    bool equalsIgnoringCase(const std::string& a, const char* b) {
      size_t i = 0;

      for (; i < a.size() && b[i]; i++) {
        if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i])))
          return false;
      }

      return i == a.size() && !b[i];
    }

  }


  const char* vrWeaponTypeName(uint32_t type) {
    return type < VrWeaponTypeCount ? WeaponTypeNames[type] : nullptr;
  }


  uint32_t vrParseWeaponTypes(const std::string& list, std::vector<std::string>& unknown) {
    uint32_t mask = 0;
    size_t   n    = 0;

    while (n < list.size()) {
      while (n < list.size() && isSeparator(list[n]))
        n++;

      size_t begin = n;

      while (n < list.size() && !isSeparator(list[n]))
        n++;

      if (begin == n)
        continue;

      std::string word = list.substr(begin, n - begin);

      auto name = std::find_if(std::begin(WeaponTypeNames), std::end(WeaponTypeNames),
        [&word] (const char* candidate) { return equalsIgnoringCase(word, candidate); });

      if (name != std::end(WeaponTypeNames))
        mask |= 1u << uint32_t(name - std::begin(WeaponTypeNames));
      else
        unknown.push_back(std::move(word));
    }

    return mask;
  }

}
