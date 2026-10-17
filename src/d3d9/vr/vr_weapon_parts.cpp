#include <cstring>

#include "../../util/log/log.h"
#include "../../util/util_string.h"

#include "vr_game_addresses.h"
#include "vr_game_nodes.h"
#include "vr_weapon_aim.h"
#include "vr_weapon_parts.h"

namespace dxvk {

  namespace {

    // The parts are a few levels below the weapon node
    constexpr uint32_t MaxPartDepth = 12;

    constexpr float CentimetresPerUnit = 100.0f / VrGameUnitsPerMetre;

    constexpr char PartPrefix[] = "##";

    template<typename T>
    T readField(const uint8_t* base, uintptr_t offset) {
      T value;
      std::memcpy(&value, base + offset, sizeof(value));
      return value;
    }

    VrVector3 readVector(const uint8_t* object, uintptr_t offset) {
      return readField<VrVector3>(object, offset);
    }

    VrGameRotation readRotation(const uint8_t* object, uintptr_t offset) {
      VrGameRotation r;
      std::memcpy(r.m, object + offset, sizeof(r.m));
      return r;
    }

    const char* objectName(uintptr_t object) {
      auto name = readField<const char*>(reinterpret_cast<const uint8_t*>(object), VrGame::ObjectName);
      return name ? name : "";
    }

    bool isPart(const char* name) {
      return !std::strncmp(name, PartPrefix, sizeof(PartPrefix) - 1)
          || !std::strcmp(name, VrGame::ProjectileNodeName);
    }

    std::string formatCm(const VrVector3& v) {
      return str::format(v.x * CentimetresPerUnit, ", ", v.y * CentimetresPerUnit, ", ", v.z * CentimetresPerUnit);
    }

  }


  uint32_t vrCountWeaponObjects(const uint8_t* weapon) {
    uint32_t count = 0;

    vrVisitGameObjects(reinterpret_cast<uintptr_t>(weapon), MaxPartDepth,
      [&count] (uintptr_t, uint32_t) { count++; });

    return count;
  }


  void vrLogWeaponParts(const uint8_t* weapon, const uint8_t* form) {
    const char* name  = form ? readField<const char*>(form, VrGame::WeaponFullName) : nullptr;
    const char* model = form ? readField<const char*>(form, VrGame::WeaponModelPath) : nullptr;
    uint32_t    clip  = form ? readField<uint8_t>(form, VrGame::WeaponClipRounds) : 0;

    Logger::info(str::format("VR: Weapon parts of '", name ? name : "", "' (", model ? model : "",
      ", clip ", clip, "), centimetres along the weapon node's axes:"));

    // Along the weapon node's axes, at the scale of the world
    VrGameRotation toWeapon = vrTranspose(readRotation(weapon, VrGame::ObjectWorldRotation));
    VrVector3      origin   = readVector(weapon, VrGame::ObjectWorldTranslation);

    vrVisitGameObjects(reinterpret_cast<uintptr_t>(weapon), MaxPartDepth,
      [&] (uintptr_t object, uint32_t depth) {
        const char* partName = objectName(object);

        if (!isPart(partName))
          return;

        auto part = reinterpret_cast<const uint8_t*>(object);
        VrGameRotation rotation = toWeapon * readRotation(part, VrGame::ObjectWorldRotation);

        std::string line = str::format("VR:   ", partName, vrGameAsNode(object) ? " (node" : " (geometry",
          ", depth ", depth, ") at ", formatCm(toWeapon * (readVector(part, VrGame::ObjectWorldTranslation) - origin)),
          ", x axis ", rotation.m[0][0], ", ", rotation.m[1][0], ", ", rotation.m[2][0],
          ", y axis ", rotation.m[0][1], ", ", rotation.m[1][1], ", ", rotation.m[2][1]);

        // The bound covers the part's meshes, the place a hand takes it by
        if (auto bound = readField<const uint8_t*>(part, VrGame::ObjectWorldBound)) {
          float radius = readField<float>(bound, sizeof(VrVector3));
          line += str::format(", bound centre ", formatCm(toWeapon * (readVector(bound, 0) - origin)),
            " radius ", radius * CentimetresPerUnit);
        }

        Logger::info(line);
      });
  }

}
