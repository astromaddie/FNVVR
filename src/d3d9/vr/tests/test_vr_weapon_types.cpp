#include "test.h"

#include "../vr_weapon_types.h"

using namespace dxvk;


TEST_CASE(weapon_types_are_named_as_in_the_game) {
  CHECK(std::string(vrWeaponTypeName(0)) == "HandToHandMelee");
  CHECK(std::string(vrWeaponTypeName(1)) == "OneHandMelee");
  CHECK(std::string(vrWeaponTypeName(2)) == "TwoHandMelee");
  CHECK(std::string(vrWeaponTypeName(13)) == "OneHandThrown");
  CHECK(vrWeaponTypeName(VrWeaponTypeCount) == nullptr);
}


TEST_CASE(weapon_type_list_reads_commas_spaces_and_any_case) {
  std::vector<std::string> unknown;
  uint32_t mask = vrParseWeaponTypes(" onehandmelee,TwoHandMelee  ONEHANDPISTOL,,", unknown);

  CHECK(mask == ((1u << 1) | (1u << 2) | (1u << 3)));
  CHECK(unknown.empty());
}


TEST_CASE(weapon_type_list_reports_unknown_names) {
  std::vector<std::string> unknown;
  uint32_t mask = vrParseWeaponTypes("OneHandMelee,Sword,TwoHandMeleeX", unknown);

  CHECK(mask == (1u << 1));
  CHECK(unknown.size() == 2);
  CHECK(unknown.size() == 2 && unknown[0] == "Sword" && unknown[1] == "TwoHandMeleeX");
}


TEST_CASE(weapon_type_list_can_be_empty) {
  std::vector<std::string> unknown;
  CHECK(vrParseWeaponTypes("", unknown) == 0);
  CHECK(vrParseWeaponTypes(" , ", unknown) == 0);
  CHECK(unknown.empty());
}
