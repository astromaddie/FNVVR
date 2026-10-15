#include <cstring>

#include "../../util/log/log.h"
#include "../../util/util_string.h"

#include "vr_game_addresses.h"
#include "vr_game_memory.h"
#include "vr_game_nodes.h"
#include "vr_math.h"
#include "vr_melee_swing.h"

namespace dxvk {

  namespace {

    // Actor::CombatHit, see VrGame::CombatHit
    using CombatHitFn = void (__fastcall*)(void* attacker, void* unused,
      void* target, uint32_t power, void* projectile, uint32_t meleeEffect);

    // TESObjectREFR::Get3D and IsActor, __thiscall without arguments
    using Get3DFn   = uintptr_t (__fastcall*)(uintptr_t reference, void* unused);
    using IsActorFn = uint32_t (__fastcall*)(uintptr_t reference, void* unused);

    // Positions the blade passes through between two frames
    constexpr uint32_t SweepSteps = 4;

    // Radius of the capsule around each bone, about a limb's
    constexpr float BoneRadius = 0.10f * VrGameUnitsPerMetre;

    // Bones further apart are not a limb, and an actor further than this
    // from the blade cannot be reached, in game units
    constexpr float MaxBoneLength = 1.5f * VrGameUnitsPerMetre;
    constexpr float MaxActorDistance = 4.0f * VrGameUnitsPerMetre;

    // The skeleton's deepest bones (fingers) are about twenty levels down
    constexpr uint32_t MaxSkeletonDepth = 32;

    // More entries than this means the list was not read correctly
    constexpr uint32_t MaxProcessActors = 4096;

    // Bones are named like this in the skeletons of characters and of most
    // creatures. Other nodes of an actor's model (armour, attached weapons)
    // are placed at the actor's origin and would join it to every bone.
    constexpr char BonePrefix[] = "Bip01";

    constexpr uint32_t MaxLoggedHits = 50;

    VrMeleeSwing* g_meleeSwing = nullptr;

    template<typename T>
    T readField(uintptr_t base, uintptr_t offset) {
      return *reinterpret_cast<const T*>(base + offset);
    }

    VrVector3 readVector(uintptr_t base, uintptr_t offset) {
      VrVector3 v;
      std::memcpy(&v, reinterpret_cast<const void*>(base + offset), sizeof(v));
      return v;
    }

    bool isBone(uintptr_t object) {
      auto name = readField<const char*>(object, VrGame::ObjectName);
      return name && !std::strncmp(name, BonePrefix, sizeof(BonePrefix) - 1);
    }

    /**
     * \brief Calls a function for each bone and its parent bone
     */
    template<typename Fn>
    bool forEachBone(uintptr_t node, uint32_t depth, const Fn& fn) {
      auto children = readField<const uintptr_t*>(node, VrGame::NodeChildren);
      auto count    = readField<uint16_t>(node, VrGame::NodeChildCount);
      bool parentIsBone = isBone(node);

      for (uint32_t i = 0; children && i < count; i++) {
        uintptr_t child = children[i] ? vrGameAsNode(children[i]) : 0;

        if (!child)
          continue;

        if (parentIsBone && isBone(child) && fn(node, child))
          return true;

        if (depth && forEachBone(child, depth - 1, fn))
          return true;
      }

      return false;
    }

  }


  VrMeleeSwing::~VrMeleeSwing() {
    g_meleeSwing = nullptr;
  }


  std::unique_ptr<VrMeleeSwing> VrMeleeSwing::install(const VrSwingConfig& config) {
    if (g_meleeSwing)
      return nullptr;

    bool found = VrGameMemory::matches(VrGame::CombatHit, VrGame::CombatHitPrologue, sizeof(VrGame::CombatHitPrologue))
      && VrGameMemory::readable(VrGame::ProcessActors, sizeof(uintptr_t))
      && VrGameMemory::readable(VrGame::ProcessHighEnd, sizeof(uint32_t));

    if (!found)
      return nullptr;

    Logger::info(str::format("VR: Melee weapons hit when swung faster than ", config.hitSpeed,
      " m/s, power attacks from ", config.powerSpeed, " m/s"));

    std::unique_ptr<VrMeleeSwing> swing(new VrMeleeSwing(config));
    g_meleeSwing = swing.get();
    return swing;
  }


  VrMeleeHit VrMeleeSwing::update(
    const VrGameCameraPose*     hand,
    const VrBlade*              blade,
    const VrGameCameraPose&     head,
    const VrGameCameraPose&     body,
          float                 dt) {
    VrMeleeHit result;

    auto player = *reinterpret_cast<uint8_t* const*>(VrGame::Player);

    if (!hand || !blade || !player) {
      m_hasBlade = false;
      m_swing.reset();
      m_guard.reset();
      return result;
    }

    // Where the blade was on the previous frame, moved and turned with the
    // body since
    VrSegment previous = {
      vrPointFromHand(body, m_blade.a),
      vrPointFromHand(body, m_blade.b) };
    bool hadBlade = m_hasBlade;

    VrSegment current = vrBladeInWorld(*hand, *blade);

    m_blade    = { vrPointToHand(body, current.a), vrPointToHand(body, current.b) };
    m_hasBlade = true;

    bool wasGuarding = m_guard.guarding();

    m_swing.update(m_blade.b, dt);
    m_guard.update(vrIsGuardPose(head, current), m_swing.swinging(), dt);

    if (m_guard.guarding() && !wasGuarding && m_loggedGuards < MaxLoggedHits) {
      Logger::info("VR: The melee weapon is held as a guard, the game's block control is held");
      m_loggedGuards++;
    }

    if (!hadBlade || !m_swing.swinging())
      return result;

    result.power = m_swing.powerful();
    result.count = hitActors(player, previous, current, result.power);

    if (result.count && m_loggedHits < MaxLoggedHits) {
      Logger::info(str::format("VR: Melee swing at ", m_swing.speed(), " m/s hit ", result.count,
        result.count > 1 ? " actors" : " actor", result.power ? " with a power attack" : ""));
      m_loggedHits++;
    }

    return result;
  }


  uint32_t VrMeleeSwing::hitActors(uint8_t* player, const VrSegment& from, const VrSegment& to, bool power) {
    auto actors = *reinterpret_cast<const uintptr_t* const*>(VrGame::ProcessActors);
    auto end    = *reinterpret_cast<const uint32_t*>(VrGame::ProcessHighEnd);

    if (!actors || end > MaxProcessActors)
      return 0;

    auto combatHit = reinterpret_cast<CombatHitFn>(VrGame::CombatHit);
    uint32_t count = 0;

    for (uint32_t i = 0; i < end; i++) {
      uintptr_t actor = actors[i];

      if (!actor || actor == reinterpret_cast<uintptr_t>(player) || m_swing.hasHit(reinterpret_cast<void*>(actor)))
        continue;

      auto vtable = readField<uintptr_t>(actor, 0);

      if (!(reinterpret_cast<IsActorFn>(readField<uintptr_t>(vtable, VrGame::RefIsActorSlot))(actor, nullptr) & 0xFF))
        continue;

      if (!touches(actor, from, to))
        continue;

      m_swing.addHit(reinterpret_cast<void*>(actor));
      combatHit(player, nullptr, reinterpret_cast<void*>(actor), power ? 1 : 0, nullptr, 0);
      count++;
    }

    return count;
  }


  bool VrMeleeSwing::touches(uintptr_t actor, const VrSegment& from, const VrSegment& to) const {
    VrVector3 location = readVector(actor, VrGame::RefLocation);
    VrVector3 middle   = (to.a + to.b) * 0.5f;

    if (vrLength(middle - location) > MaxActorDistance)
      return false;

    auto vtable = readField<uintptr_t>(actor, 0);
    uintptr_t root = reinterpret_cast<Get3DFn>(readField<uintptr_t>(vtable, VrGame::RefGet3DSlot))(actor, nullptr);
    uintptr_t node = root ? vrGameAsNode(root) : 0;

    if (!node)
      return false;

    return forEachBone(node, MaxSkeletonDepth, [&] (uintptr_t parent, uintptr_t child) {
      VrSegment bone = {
        readVector(parent, VrGame::ObjectWorldTranslation),
        readVector(child,  VrGame::ObjectWorldTranslation) };

      if (vrLength(bone.b - bone.a) > MaxBoneLength)
        return false;

      return vrSweptSegmentDistance(from, to, bone, SweepSteps) <= BoneRadius;
    });
  }

}
