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

    // Hit limb, see findings/melee.md §10 and VrGame::SetTargetedLimb
    using SetTargetedLimbFn   = void (__fastcall*)(uintptr_t hitData, void* unused, void* projectile);
    using GetActorBaseFn      = uintptr_t (__fastcall*)(uintptr_t actor, void* unused);
    using GetBodyPartDataFn   = uintptr_t (__fastcall*)(uintptr_t actorBase, void* unused);
    using GetDamageLocationFn = int32_t (__fastcall*)(uintptr_t actor, void* unused, uintptr_t node);
    using GetDamageNodeFn     = uintptr_t (__fastcall*)(uintptr_t process, void* unused, int32_t limb);
    using GetBodyPartFn       = uintptr_t (__fastcall*)(uintptr_t bodyPartData, void* unused, int32_t limb);
    using GetPartTypeFn       = int32_t (__fastcall*)(uintptr_t bodyPart, void* unused);
    using CalcLimbDamageFn    = float (__cdecl*)(uintptr_t actorValues, uintptr_t bodyPart, float damage);
    using GetLimbDamageMultFn = float (__fastcall*)(uintptr_t weapon, void* unused);
    using SetBonusMultFn      = void (__fastcall*)(uintptr_t hitData, void* unused, float mult);

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

    // A damage node further than this from the contact is not the limb hit
    constexpr float MaxDamageNodeDistance = 1.5f * VrGameUnitsPerMetre;

    constexpr uint32_t MaxLoggedHits = 50;

    // BGSBodyPart::LIMB_ENUM, torso to brain
    constexpr const char* LimbNames[] = {
      "torso", "head 1", "head 2", "left arm 1", "left arm 2", "right arm 1", "right arm 2",
      "left leg 1", "left leg 2", "left leg 3", "right leg 1", "right leg 2", "right leg 3", "brain" };

    static_assert(sizeof(LimbNames) / sizeof(LimbNames[0]) == size_t(VrGame::LimbWeapon),
      "One name per body part before the weapon");

    VrMeleeSwing* g_meleeSwing = nullptr;

    template<typename T>
    T readField(uintptr_t base, uintptr_t offset) {
      return *reinterpret_cast<const T*>(base + offset);
    }

    template<typename T>
    void writeField(uintptr_t base, uintptr_t offset, const T& value) {
      *reinterpret_cast<T*>(base + offset) = value;
    }

    VrVector3 readVector(uintptr_t base, uintptr_t offset) {
      VrVector3 v;
      std::memcpy(&v, reinterpret_cast<const void*>(base + offset), sizeof(v));
      return v;
    }

    void writeVector(uintptr_t base, uintptr_t offset, const VrVector3& v) {
      std::memcpy(reinterpret_cast<void*>(base + offset), &v, sizeof(v));
    }

    template<typename Fn>
    Fn virtualFunction(uintptr_t object, uintptr_t slot) {
      return reinterpret_cast<Fn>(readField<uintptr_t>(readField<uintptr_t>(object, 0), slot));
    }

    const char* nodeName(uintptr_t object) {
      auto name = readField<const char*>(object, VrGame::ObjectName);
      return name ? name : "(unnamed)";
    }

    /**
     * \brief Checks the functions the hit limb uses by their calls in SetTargetedLimb
     */
    bool findLimbFunctions() {
      struct Call {
        uintptr_t site;
        uintptr_t target;
      };

      const Call calls[] = {
        { VrGame::ActorGetBaseCallSite,             VrGame::ActorGetBase },
        { VrGame::ActorGetDamageLocationCallSite,   VrGame::ActorGetDamageLocation },
        { VrGame::BodyPartDataGetPartCallSite,      VrGame::BodyPartDataGetPart },
        { VrGame::BodyPartGetTypeCallSite,          VrGame::BodyPartGetType },
        { VrGame::CalcTargetedLimbDamageCallSite,   VrGame::CalcTargetedLimbDamage },
        { VrGame::WeaponGetLimbDamageMultCallSite,  VrGame::WeaponGetLimbDamageMult },
        { VrGame::HitDataSetBonusMultCallSite,      VrGame::HitDataSetBonusMult },
      };

      for (const Call& call : calls) {
        if (!VrGameMemory::callsTo(call.site, call.target))
          return false;
      }

      return true;
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

    // Without the limb hook, the game picks the limb along the view
    bool limbFound = findLimbFunctions()
      && VrGameMemory::redirectCall(VrGame::SetTargetedLimbCallSite, VrGame::SetTargetedLimb,
           reinterpret_cast<const void*>(&VrMeleeSwing::setTargetedLimbHook));

    Logger::info(str::format("VR: Melee weapons hit when swung faster than ", config.hitSpeed,
      " m/s, power attacks from ", config.powerSpeed, " m/s"));

    if (!limbFound)
      Logger::warn("VR: The limb of melee hits could not be patched, the game picks it along the view");

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

      Contact contact;

      if (!findContact(actor, from, to, contact))
        continue;

      m_swing.addHit(reinterpret_cast<void*>(actor));

      // The game asks for the limb while it computes the hit, see setTargetedLimb
      m_contact = contact;
      m_dealing = true;
      combatHit(player, nullptr, reinterpret_cast<void*>(actor), power ? 1 : 0, nullptr, 0);
      m_dealing = false;

      count++;
    }

    return count;
  }


  bool VrMeleeSwing::findContact(uintptr_t actor, const VrSegment& from, const VrSegment& to, Contact& contact) const {
    VrVector3 location = readVector(actor, VrGame::RefLocation);
    VrVector3 middle   = (to.a + to.b) * 0.5f;

    if (vrLength(middle - location) > MaxActorDistance)
      return false;

    auto vtable = readField<uintptr_t>(actor, 0);
    uintptr_t root = reinterpret_cast<Get3DFn>(readField<uintptr_t>(vtable, VrGame::RefGet3DSlot))(actor, nullptr);
    uintptr_t node = root ? vrGameAsNode(root) : 0;

    if (!node)
      return false;

    bool found = false;

    // The bone the blade reaches first in the sweep, the nearest one if
    // several are reached at once
    forEachBone(node, MaxSkeletonDepth, [&] (uintptr_t parent, uintptr_t child) {
      VrSegment bone = {
        readVector(parent, VrGame::ObjectWorldTranslation),
        readVector(child,  VrGame::ObjectWorldTranslation) };

      if (vrLength(bone.b - bone.a) > MaxBoneLength)
        return false;

      VrSweepContact sweep = vrSweptSegmentContact(from, to, bone, SweepSteps, BoneRadius);

      if (!sweep.touches)
        return false;

      if (found && (sweep.time > contact.time
                || (sweep.time == contact.time && sweep.distance >= contact.distance)))
        return false;

      // The capsule from a bone to its child is the bone's limb
      contact.actor    = actor;
      contact.bone     = parent;
      contact.time     = sweep.time;
      contact.distance = sweep.distance;
      contact.hit      = vrComputeBladeHit(sweep, BoneRadius);
      found = true;
      return false;
    });

    return found;
  }


  void __fastcall VrMeleeSwing::setTargetedLimbHook(uintptr_t hitData, void* unused, void* projectile) {
    if (g_meleeSwing && g_meleeSwing->setTargetedLimb(hitData, projectile))
      return;

    reinterpret_cast<SetTargetedLimbFn>(VrGame::SetTargetedLimb)(hitData, nullptr, projectile);
  }


  bool VrMeleeSwing::setTargetedLimb(uintptr_t hitData, void* projectile) {
    auto player = *reinterpret_cast<const uintptr_t*>(VrGame::Player);

    // Only the hit dealt by hitActors, not the game's own hits
    if (!m_dealing || projectile
     || readField<uintptr_t>(hitData, VrGame::HitAggressor) != player
     || readField<uintptr_t>(hitData, VrGame::HitTarget) != m_contact.actor
     || readField<uintptr_t>(hitData, VrGame::HitVatsCommand))
      return false;

    uintptr_t actor = m_contact.actor;
    uintptr_t actorBase = reinterpret_cast<GetActorBaseFn>(VrGame::ActorGetBase)(actor, nullptr);

    if (!actorBase)
      return false;

    uintptr_t bodyParts = virtualFunction<GetBodyPartDataFn>(actorBase,
      VrGame::ActorBaseBodyPartDataSlot)(actorBase, nullptr);

    if (!bodyParts)
      return false;

    int32_t limb = limbOfContact(bodyParts);

    if (limb < 0)
      return false;

    auto getBodyPart = reinterpret_cast<GetBodyPartFn>(VrGame::BodyPartDataGetPart);
    uintptr_t bodyPart = getBodyPart(bodyParts, nullptr, limb);

    if (!bodyPart)
      return false;

    // What the game's routine does once it has picked a body part, see
    // findings/melee.md §10.1. The target is never the player, so the
    // player's limb damage multiplier does not apply.
    int32_t partType = reinterpret_cast<GetPartTypeFn>(VrGame::BodyPartGetType)(bodyPart, nullptr);

    writeField<int32_t>(hitData, VrGame::HitDamageLimb, partType);
    writeVector(hitData, VrGame::HitLocation, m_contact.hit.location);
    writeVector(hitData, VrGame::HitDirection, m_contact.hit.direction);

    float limbDamage = reinterpret_cast<CalcLimbDamageFn>(VrGame::CalcTargetedLimbDamage)(
      actor + VrGame::ActorValueOwnerBase, bodyPart, readField<float>(hitData, VrGame::HitHealthDamage));

    uintptr_t weapon = readField<uintptr_t>(hitData, VrGame::HitWeapon);

    if (weapon)
      limbDamage *= reinterpret_cast<GetLimbDamageMultFn>(VrGame::WeaponGetLimbDamageMult)(weapon, nullptr);

    writeField<float>(hitData, VrGame::HitLimbDamage, limbDamage);

    // The game ignores the body part's damage multiplier for melee weapons
    reinterpret_cast<SetBonusMultFn>(VrGame::HitDataSetBonusMult)(hitData, nullptr, 1.0f);

    if (m_loggedLimbs < MaxLoggedHits) {
      const char* limbName = partType >= 0 && partType < VrGame::LimbWeapon ? LimbNames[partType] : "another part";

      Logger::info(str::format("VR: The blade hit the ", limbName, " at bone ", nodeName(m_contact.bone),
        ", limb damage ", limbDamage));
      m_loggedLimbs++;
    }

    return true;
  }


  int32_t VrMeleeSwing::limbOfContact(uintptr_t bodyParts) const {
    uintptr_t actor = m_contact.actor;
    auto getBodyPart = reinterpret_cast<GetBodyPartFn>(VrGame::BodyPartDataGetPart);

    // The game's own mapping walks up from the bone to a body part's node
    int32_t limb = reinterpret_cast<GetDamageLocationFn>(VrGame::ActorGetDamageLocation)(
      actor, nullptr, m_contact.bone);

    if (limb >= 0 && limb < VrGame::LimbWeapon && getBodyPart(bodyParts, nullptr, limb))
      return limb;

    // Bones above no body part's node, such as the pelvis and the lower
    // spine under the torso's node: the nearest body part's node instead
    uintptr_t process = readField<uintptr_t>(actor, VrGame::ActorProcess);

    if (!process)
      return -1;

    auto getDamageNode = virtualFunction<GetDamageNodeFn>(process, VrGame::ProcessDamageNodeSlot);

    int32_t nearest = -1;
    float nearestDistance = MaxDamageNodeDistance;

    for (int32_t i = 0; i < VrGame::LimbWeapon; i++) {
      uintptr_t node = getDamageNode(process, nullptr, i);

      if (!node || !getBodyPart(bodyParts, nullptr, i))
        continue;

      float distance = vrLength(readVector(node, VrGame::ObjectWorldTranslation) - m_contact.hit.location);

      if (distance < nearestDistance) {
        nearest = i;
        nearestDistance = distance;
      }
    }

    return nearest;
  }

}
