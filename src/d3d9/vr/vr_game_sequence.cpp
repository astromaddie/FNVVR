#include <cfloat>
#include <cstring>

#include "vr_game_addresses.h"
#include "vr_game_nodes.h"
#include "vr_game_sequence.h"

namespace dxvk {

  namespace {

    // Process virtual function, __thiscall without arguments, returns the
    // equipped weapon's inventory entry
    using GetWeaponInfoFn = uint8_t* (__fastcall*)(uint8_t* process, void* unused);

    // Actor::GetAnimGroup, __thiscall with four stack arguments, returns
    // the 16-bit group in AX
    using GetAnimGroupFn = uint32_t (__fastcall*)(uint8_t* actor, void* unused,
      uint32_t group, void* weaponEntry, uint32_t checkHandToHand, void* animation);

    // NiTPointerMap::GetAt, __thiscall with the key and the result's address
    using MapGetAtFn = uint32_t (__fastcall*)(uint8_t* map, void* unused, uint32_t key, uint8_t** value);

    // AnimSequenceBase::GetSequenceToPlay, __thiscall with an index
    using GetSequenceFn = uint8_t* (__fastcall*)(uint8_t* base, void* unused, int32_t index);

    // NiInterpolator::UpdateTransform, __thiscall with three stack arguments
    using UpdateTransformFn = uint32_t (__fastcall*)(uint8_t* interpolator, void* unused,
      float time, uint8_t* target, void* value);

    // NiInterpolator::IsTransformValueSupported, __thiscall without arguments
    using IsTransformFn = uint32_t (__fastcall*)(uint8_t* interpolator, void* unused);

    /// NiQuatTransform: each part is -FLT_MAX where the track has none
    struct QuatTransform {
      float translate[3] = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
      float rotate[4]    = { -FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX };
      float scale        = -FLT_MAX;
    };

    // The bones below a hand are a few levels deep
    constexpr uint32_t MaxHandDepth = 8;

    template<typename T>
    T readField(const uint8_t* base, uintptr_t offset) {
      return *reinterpret_cast<const T*>(base + offset);
    }

    VrVector3 readVector(const uint8_t* object, uintptr_t offset) {
      VrVector3 v;
      std::memcpy(&v, object + offset, sizeof(v));
      return v;
    }

    VrGameRotation readRotation(const uint8_t* object, uintptr_t offset) {
      VrGameRotation r;
      std::memcpy(r.m, object + offset, sizeof(r.m));
      return r;
    }

    const char* objectName(const uint8_t* object) {
      return readField<const char*>(object, VrGame::ObjectName);
    }

  }


  VrSequencePose vrComposeSequencePose(const VrSequencePose& parent, const VrSequencePose& child) {
    VrSequencePose result;
    result.rotate    = parent.rotate * child.rotate;
    result.translate = parent.translate + (parent.rotate * child.translate) * parent.scale;
    result.scale     = parent.scale * child.scale;
    return result;
  }


  VrSequenceTracks::VrSequenceTracks(const uint8_t* sequence) {
    uint32_t count  = readField<uint32_t>(sequence, VrGame::SequenceArraySize);
    auto interps    = readField<const uint8_t*>(sequence, VrGame::SequenceInterpolators);
    auto tags       = readField<const uint8_t*>(sequence, VrGame::SequenceIdTags);

    for (uint32_t i = 0; interps && tags && i < count; i++) {
      auto interpolator = readField<uint8_t*>(interps + i * VrGame::SequenceInterpolatorSize, 0);
      auto name         = readField<const char*>(tags + i * VrGame::SequenceIdTagSize, 0);

      if (!interpolator || !name)
        continue;

      auto vtable = readField<const uint8_t*>(interpolator, 0);
      auto isTransform = reinterpret_cast<IsTransformFn>(readField<uintptr_t>(vtable, VrGame::InterpolatorIsTransformSlot));

      if (isTransform(interpolator, nullptr) & 0xFF)
        m_tracks.push_back({ name, interpolator });
    }
  }


  VrSequencePose VrSequenceTracks::local(uint8_t* node, float time, bool* rotated) const {
    VrSequencePose pose;
    pose.rotate    = readRotation(node, VrGame::ObjectLocalRotation);
    pose.translate = readVector(node, VrGame::NodeLocalTranslate);
    pose.scale     = readField<float>(node, VrGame::ObjectLocalScale);

    if (rotated)
      *rotated = false;

    uint8_t* interpolator = find(objectName(node));

    if (!interpolator)
      return pose;

    QuatTransform value;

    auto vtable = readField<const uint8_t*>(interpolator, 0);
    auto update = reinterpret_cast<UpdateTransformFn>(readField<uintptr_t>(vtable, VrGame::InterpolatorUpdateTransformSlot));

    if (!(update(interpolator, nullptr, time, node, &value) & 0xFF))
      return pose;

    if (value.translate[0] != -FLT_MAX)
      pose.translate = { value.translate[0], value.translate[1], value.translate[2] };

    if (value.rotate[1] != -FLT_MAX) {
      VrQuaternion q;
      q.w = value.rotate[0];
      q.x = value.rotate[1];
      q.y = value.rotate[2];
      q.z = value.rotate[3];
      pose.rotate = vrGameRotationFromQuaternion(vrNormalize(q));

      if (rotated)
        *rotated = true;
    }

    if (value.scale != -FLT_MAX)
      pose.scale = value.scale;

    return pose;
  }


  bool VrSequenceTracks::toAncestor(uint8_t* node, const uint8_t* ancestor, float time, VrSequencePose& pose) const {
    pose = VrSequencePose();

    for (; node && node != ancestor; node = readField<uint8_t*>(node, VrGame::ObjectParent))
      pose = vrComposeSequencePose(local(node, time), pose);

    return node == ancestor;
  }


  bool VrSequenceTracks::moves(const uint8_t* node) const {
    return node && find(objectName(node));
  }


  uint8_t* VrSequenceTracks::find(const char* name) const {
    if (!name)
      return nullptr;

    for (const Track& track : m_tracks) {
      if (!std::strcmp(track.name, name))
        return track.interpolator;
    }

    return nullptr;
  }


  uint8_t* vrActorWeaponEntry(uint8_t* actor) {
    uint8_t* process = readField<uint8_t*>(actor, VrGame::ActorProcess);

    if (!process)
      return nullptr;

    auto vtable = readField<const uint8_t*>(process, 0);
    auto getWeaponInfo = reinterpret_cast<GetWeaponInfoFn>(readField<uintptr_t>(vtable, VrGame::ProcessWeaponInfoSlot));
    return getWeaponInfo(process, nullptr);
  }


  const uint8_t* vrFindPlayerSequence(uint8_t* player, uint32_t group) {
    auto animation = readField<uint8_t*>(player, VrGame::PlayerFirstPersonAnimData);

    if (!animation)
      return nullptr;

    uint32_t full = reinterpret_cast<GetAnimGroupFn>(VrGame::ActorGetAnimGroup)(
      player, nullptr, group, nullptr, 0, animation) & 0xFFFF;

    // The AnimData has no such group: the game would not play one either
    if ((full & 0xFF) != group)
      return nullptr;

    auto map = readField<uint8_t*>(animation, VrGame::AnimDataSequences);
    uint8_t* base = nullptr;

    if (!map || !(reinterpret_cast<MapGetAtFn>(VrGame::AnimSequenceMapGetAt)(map, nullptr, full, &base) & 0xFF) || !base)
      return nullptr;

    auto vtable = readField<const uint8_t*>(base, 0);
    auto getSequence = reinterpret_cast<GetSequenceFn>(readField<uintptr_t>(vtable, VrGame::AnimSequenceToPlaySlot));
    return getSequence(base, nullptr, 0);
  }


  std::vector<VrBonePose> vrReadSequenceFingers(const VrSequenceTracks& tracks, uint8_t* hand, float time) {
    std::vector<VrBonePose> fingers;

    vrVisitGameObjects(reinterpret_cast<uintptr_t>(hand), MaxHandDepth,
      [&tracks, &fingers, hand, time] (uintptr_t object, uint32_t) {
        auto bone = reinterpret_cast<uint8_t*>(object);
        auto name = objectName(bone);
        bool rotated = false;

        if (bone == hand || !name)
          return;

        VrSequencePose pose = tracks.local(bone, time, &rotated);

        if (rotated)
          fingers.push_back({ name, pose.rotate });
      });

    return fingers;
  }

}
