#include <cfloat>
#include <cstring>

#include "../../util/log/log.h"
#include "../../util/util_string.h"

#include "vr_game_addresses.h"
#include "vr_game_nodes.h"
#include "vr_magazine.h"
#include "vr_reload_animation.h"

namespace dxvk {

  namespace {

    // Process virtual function, __thiscall without arguments, returns the
    // equipped weapon's inventory entry
    using GetWeaponInfoFn = uint8_t* (__fastcall*)(uint8_t* process, void* unused);

    // ItemChange::HasModEffectActive, __thiscall with the effect, returns a
    // bool in AL
    using HasModEffectFn = uint32_t (__fastcall*)(uint8_t* entry, void* unused, uint32_t effect);

    // TESObjectWEAP::GetReloadAnim, __thiscall with a bool
    using GetReloadAnimFn = uint32_t (__fastcall*)(const uint8_t* weapon, void* unused, uint32_t extended);

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

    // TESSound::GetFilename, __thiscall without arguments
    using GetFilenameFn = const char* (__fastcall*)(const uint8_t* sound, void* unused);

    // TESSound::ConvertLoopFlag, __cdecl with the sound's flags
    using ConvertLoopFlagFn = uint32_t (__cdecl*)(uint32_t flags);

    // BSAudio::GetSoundHandleByFilename, __thiscall with four stack arguments
    using GetSoundFn = void* (__fastcall*)(void* audio, void* unused,
      void* handle, const char* file, uint32_t flags, const uint8_t* sound);

    // BSSoundHandle functions, __thiscall
    using SetPositionFn  = uint32_t (__fastcall*)(void* handle, void* unused, float x, float y, float z);
    using SetFloatFn     = uint32_t (__fastcall*)(void* handle, void* unused, float value);
    using PlayFn         = uint32_t (__fastcall*)(void* handle, void* unused, uint32_t loop);

    /// NiQuatTransform: each part is -FLT_MAX where the track has none
    struct QuatTransform {
      float translate[3] = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
      float rotate[4]    = { -FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX };
      float scale        = -FLT_MAX;
    };

    /// BSSoundHandle: the sound's ID, -1 without one, and its state
    struct SoundHandle {
      int32_t  id      = -1;
      uint8_t  assume  = 0;
      uint32_t state   = 0;
    };

    static_assert(sizeof(SoundHandle) == 12);

    // How many times per second of the animation the magazine is sampled
    constexpr float SamplesPerSecond = 60.0f;

    // How far from the magazine's times its sounds may be, in seconds of
    // the animation
    constexpr float SoundWindow = 0.4f;

    // The bones below the left hand are a few levels deep
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

    /**
     * \brief A node's transform in an ancestor's frame
     */
    struct Pose {
      VrGameRotation rotate;
      VrVector3      translate;
      float          scale = 1.0f;
    };

    Pose compose(const Pose& parent, const Pose& child) {
      Pose result;
      result.rotate    = parent.rotate * child.rotate;
      result.translate = parent.translate + (parent.rotate * child.translate) * parent.scale;
      result.scale     = parent.scale * child.scale;
      return result;
    }

    /**
     * \brief The tracks of a sequence, by the name of the object they move
     */
    class SequenceTracks {

    public:

      explicit SequenceTracks(const uint8_t* sequence) {
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

      /**
       * \brief A node's transform in its parent at a time
       *
       * Where the sequence has no track for it, or its track has no
       * value, the node's current transform is used.
       *
       * \param [out] rotated Whether the sequence gave the rotation
       */
      Pose local(uint8_t* node, float time, bool* rotated = nullptr) const {
        Pose pose;
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

      /**
       * \brief A node's transform in one of its ancestors' frame at a time
       */
      bool toAncestor(uint8_t* node, const uint8_t* ancestor, float time, Pose& pose) const {
        pose = Pose();

        for (; node && node != ancestor; node = readField<uint8_t*>(node, VrGame::ObjectParent))
          pose = compose(local(node, time), pose);

        return node == ancestor;
      }

    private:

      struct Track {
        const char* name;
        uint8_t*    interpolator;
      };

      std::vector<Track> m_tracks;

      uint8_t* find(const char* name) const {
        if (!name)
          return nullptr;

        for (const Track& track : m_tracks) {
          if (!std::strcmp(track.name, name))
            return track.interpolator;
        }

        return nullptr;
      }

    };


    uint8_t* weaponEntry(uint8_t* actor) {
      uint8_t* process = readField<uint8_t*>(actor, VrGame::ActorProcess);

      if (!process)
        return nullptr;

      auto vtable = readField<const uint8_t*>(process, 0);
      auto getWeaponInfo = reinterpret_cast<GetWeaponInfoFn>(readField<uintptr_t>(vtable, VrGame::ProcessWeaponInfoSlot));
      return getWeaponInfo(process, nullptr);
    }


    /**
     * \brief The first-person reload sequence the game would play
     */
    const uint8_t* findReloadSequence(uint8_t* player) {
      uint8_t* entry = weaponEntry(player);
      auto weapon    = entry ? readField<const uint8_t*>(entry, VrGame::InventoryEntryForm) : nullptr;
      auto animation = readField<uint8_t*>(player, VrGame::PlayerFirstPersonAnimData);

      if (!weapon || !animation)
        return nullptr;

      uint32_t extended = reinterpret_cast<HasModEffectFn>(VrGame::ItemHasModEffect)(
        entry, nullptr, VrGame::ModEffectClipSize) & 0xFF;
      uint32_t reload = reinterpret_cast<GetReloadAnimFn>(VrGame::WeaponGetReloadAnim)(
        weapon, nullptr, extended) & 0xFF;
      uint32_t group = reinterpret_cast<GetAnimGroupFn>(VrGame::ActorGetAnimGroup)(
        player, nullptr, reload, nullptr, 0, animation) & 0xFFFF;

      // The AnimData has no such group: the game would not play one either
      if ((group & 0xFF) != reload)
        return nullptr;

      auto map = readField<uint8_t*>(animation, VrGame::AnimDataSequences);
      uint8_t* base = nullptr;

      if (!map || !(reinterpret_cast<MapGetAtFn>(VrGame::AnimSequenceMapGetAt)(map, nullptr, group, &base) & 0xFF) || !base)
        return nullptr;

      auto vtable = readField<const uint8_t*>(base, 0);
      auto getSequence = reinterpret_cast<GetSequenceFn>(readField<uintptr_t>(vtable, VrGame::AnimSequenceToPlaySlot));
      return getSequence(base, nullptr, 0);
    }


    const char* soundFile(const uint8_t* sound) {
      const char* file = sound ? reinterpret_cast<GetFilenameFn>(VrGame::SoundGetFilename)(sound, nullptr) : nullptr;
      return file ? file : "";
    }


    void readSounds(const uint8_t* sequence, const VrMagazineTiming& timing, VrReloadAnimation& animation) {
      auto group = readField<const uint8_t*>(sequence, VrGame::AnimSequenceGroup);

      if (!group)
        return;

      uint32_t count = readField<uint32_t>(group, VrGame::AnimGroupSoundCount);
      auto sounds    = readField<const uint8_t*>(group, VrGame::AnimGroupSounds);

      std::vector<float> times;
      std::vector<const uint8_t*> entries;

      for (uint32_t i = 0; sounds && i < count; i++) {
        const uint8_t* entry = sounds + i * VrGame::AnimGroupSoundSize;

        // Entries without a sound are footsteps and other events
        if (readField<const uint8_t*>(entry, VrGame::AnimGroupSoundForm)) {
          times.push_back(readField<float>(entry, 0));
          entries.push_back(entry);
        }
      }

      int32_t out = vrFindNearestSound(times.data(), times.size(), timing.out, SoundWindow, -1);
      int32_t in  = vrFindNearestSound(times.data(), times.size(), timing.in,  SoundWindow, out);

      auto toSound = [&entries] (int32_t index) {
        VrReloadSound sound;

        if (index >= 0) {
          sound.sound  = readField<const uint8_t*>(entries[index], VrGame::AnimGroupSoundForm);
          sound.volume = float(readField<uint8_t>(entries[index], VrGame::AnimGroupSoundVolume)) / 255.0f;
          sound.pitch  = readField<float>(entries[index], VrGame::AnimGroupSoundPitch);
        }

        return sound;
      };

      animation.out = toSound(out);
      animation.in  = toSound(in);

      for (size_t i = 0; i < entries.size(); i++) {
        Logger::info(str::format("VR: Reload animation sound at ", times[i], " s: '",
          soundFile(readField<const uint8_t*>(entries[i], VrGame::AnimGroupSoundForm)), "'",
          int32_t(i) == out ? ", the magazine going out" : "",
          int32_t(i) == in  ? ", the magazine going in" : ""));
      }
    }

  }


  bool vrReadReloadAnimation(
          uint8_t*              player,
          uint8_t*              root,
          uint8_t*              weapon,
          uint8_t*              magazine,
          uint8_t*              leftHand,
          VrReloadAnimation&    animation) {
    const uint8_t* sequence = findReloadSequence(player);

    if (!sequence) {
      Logger::info("VR: The weapon has no first-person reload animation");
      return false;
    }

    auto sequenceName = readField<const char*>(sequence, VrGame::ObjectName);
    float begin = readField<float>(sequence, VrGame::SequenceBeginKeyTime);
    float end   = readField<float>(sequence, VrGame::SequenceEndKeyTime);

    SequenceTracks tracks(sequence);

    // The magazine in the weapon's frame through the animation, from where
    // it is at the start
    std::vector<VrMagazineSample> samples;
    Pose rest;

    for (uint32_t i = 0; begin + float(i) / SamplesPerSecond <= end; i++) {
      float time = begin + float(i) / SamplesPerSecond;
      Pose pose;

      if (!tracks.toAncestor(magazine, weapon, time, pose))
        return false;

      if (!i)
        rest = pose;

      samples.push_back({ time, (pose.translate - rest.translate) * (1.0f / VrGameUnitsPerMetre) });
    }

    VrMagazineTiming timing;

    if (!vrFindMagazineTiming(samples.data(), samples.size(), timing)) {
      Logger::info(str::format("VR: The reload animation '", sequenceName ? sequenceName : "",
        "' does not take the magazine out and put it back"));
      return false;
    }

    // The magazine in the left hand's frame while the hand holds it
    Pose hand, held;

    if (!tracks.toAncestor(leftHand, root, timing.grip, hand)
     || !tracks.toAncestor(magazine, root, timing.grip, held)
     || hand.scale <= 0.0f)
      return false;

    VrGameRotation toHand = vrTranspose(hand.rotate);
    animation.magazineInHand.rotate    = toHand * held.rotate;
    animation.magazineInHand.translate = (toHand * (held.translate - hand.translate)) * (1.0f / hand.scale);
    animation.axis = timing.axis;

    // The fingers the animation moves
    animation.fingers.clear();

    vrVisitGameObjects(reinterpret_cast<uintptr_t>(leftHand), MaxHandDepth,
      [&tracks, &timing, &animation, leftHand] (uintptr_t object, uint32_t) {
        auto bone = reinterpret_cast<uint8_t*>(object);
        auto name = objectName(bone);
        bool rotated = false;

        if (bone == leftHand || !name)
          return;

        Pose pose = tracks.local(bone, timing.grip, &rotated);

        if (rotated)
          animation.fingers.push_back({ name, pose.rotate });
      });

    readSounds(sequence, timing, animation);

    Logger::info(str::format("VR: Reload animation '", sequenceName ? sequenceName : "", "' from ", begin, " to ", end,
      " s: the magazine goes out at ", timing.out, " s and in at ", timing.in, " s, along ",
      timing.axis.x, ", ", timing.axis.y, ", ", timing.axis.z, " in the weapon; the left hand holds it at ", timing.grip,
      " s, ", animation.magazineInHand.translate.x, ", ", animation.magazineInHand.translate.y, ", ",
      animation.magazineInHand.translate.z, " units from the hand bone, with ", animation.fingers.size(), " finger bone(s)"));
    return true;
  }


  void vrPlayReloadSound(
    const VrReloadSound&        sound,
    const VrVector3&            position) {
    auto audio  = *reinterpret_cast<void* const*>(VrGame::AudioSingleton);
    auto player = *reinterpret_cast<uint8_t* const*>(VrGame::Player);

    if (!sound.sound || !audio || !player)
      return;

    uint32_t soundFlags = readField<uint32_t>(sound.sound, VrGame::SoundFormFlags);
    uint32_t loop = reinterpret_cast<ConvertLoopFlagFn>(VrGame::SoundConvertLoopFlag)(soundFlags);

    uint32_t flags = loop
      | ((soundFlags & VrGame::SoundForm2D) ? VrGame::SoundFlag2D : VrGame::SoundFlag3D)
      | VrGame::SoundFlagOneShot | VrGame::SoundFlagFirstPerson
      | VrGame::SoundFlagIgnoreTimescale | VrGame::SoundFlagAnimationDriven;

    // As fast as the reload animation would play
    auto animation = readField<const uint8_t*>(player, VrGame::PlayerFirstPersonAnimData);
    float speed = animation ? readField<float>(animation, VrGame::AnimDataReloadSpeed) : 1.0f;

    SoundHandle handle;
    reinterpret_cast<GetSoundFn>(VrGame::AudioGetSoundByFilename)(audio, nullptr,
      &handle, soundFile(sound.sound), flags, sound.sound);

    reinterpret_cast<SetPositionFn>(VrGame::SoundHandleSetPosition)(&handle, nullptr, position.x, position.y, position.z);
    reinterpret_cast<SetFloatFn>(VrGame::SoundHandleSetVolume)(&handle, nullptr, sound.volume);
    reinterpret_cast<SetFloatFn>(VrGame::SoundHandleSetFrequency)(&handle, nullptr, sound.pitch * speed);
    reinterpret_cast<PlayFn>(VrGame::SoundHandlePlay)(&handle, nullptr, loop != 0);
  }

}
