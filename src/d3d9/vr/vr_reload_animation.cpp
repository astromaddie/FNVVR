#include <algorithm>
#include <cfloat>
#include <cstring>

#include "../../util/log/log.h"
#include "../../util/util_string.h"

#include "vr_game_addresses.h"
#include "vr_game_nodes.h"
#include "vr_game_sound.h"
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

    /// NiQuatTransform: each part is -FLT_MAX where the track has none
    struct QuatTransform {
      float translate[3] = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
      float rotate[4]    = { -FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX };
      float scale        = -FLT_MAX;
    };

    // How many times per second of the animation the magazine is sampled
    constexpr float SamplesPerSecond = 60.0f;

    // How far from the magazine's times its sounds may be, in seconds of
    // the animation
    constexpr float SoundWindow = 0.4f;

    // The bones below the left hand are a few levels deep
    constexpr uint32_t MaxHandDepth = 8;

    // How near the left hand bone the rack part's centre must be for the
    // animation's hand to hold it, in metres
    constexpr float RackHeldReach = 0.15f;

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

    const char* partName(const uint8_t* part) {
      const char* name = objectName(part);
      return name ? name : "";
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


    /**
     * \brief The centre of a part's meshes in its own frame
     */
    VrVector3 meshCentre(const uint8_t* part) {
      float scale = readField<float>(part, VrGame::ObjectWorldScale);
      auto bound  = readField<const uint8_t*>(part, VrGame::ObjectWorldBound);

      // A part the game hides, such as a round, may have no bound
      if (!bound || scale <= 0.0f || readField<float>(bound, sizeof(VrVector3)) <= 0.0f)
        return VrVector3();

      return vrTranspose(readRotation(part, VrGame::ObjectWorldRotation))
        * (readVector(bound, 0) - readVector(part, VrGame::ObjectWorldTranslation)) * (1.0f / scale);
    }


    /**
     * \brief The bones below the left hand the sequence rotates, at a time
     */
    std::vector<VrBonePose> readFingers(const SequenceTracks& tracks, uint8_t* leftHand, float time) {
      std::vector<VrBonePose> fingers;

      vrVisitGameObjects(reinterpret_cast<uintptr_t>(leftHand), MaxHandDepth,
        [&tracks, &fingers, leftHand, time] (uintptr_t object, uint32_t) {
          auto bone = reinterpret_cast<uint8_t*>(object);
          auto name = objectName(bone);
          bool rotated = false;

          if (bone == leftHand || !name)
            return;

          Pose pose = tracks.local(bone, time, &rotated);

          if (rotated)
            fingers.push_back({ name, pose.rotate });
        });

      return fingers;
    }


    /**
     * \brief Reads how the sequence pulls the rack part back
     *
     * \param [out] from When it starts moving
     * \param [out] to When it is home again
     */
    bool readRack(
      const SequenceTracks&       tracks,
            uint8_t*              root,
            uint8_t*              weapon,
            uint8_t*              rack,
            uint8_t*              leftHand,
            float                 begin,
            float                 end,
            VrRackAnimation&      animation,
            float&                from,
            float&                to) {
      std::vector<VrMagazineSample> samples;
      std::vector<Pose> poses;

      for (uint32_t i = 0; begin + float(i) / SamplesPerSecond <= end; i++) {
        float t = begin + float(i) / SamplesPerSecond;
        Pose pose;

        if (!tracks.toAncestor(rack, weapon, t, pose))
          return false;

        VrVector3 offset = poses.empty() ? VrVector3() : (pose.translate - poses.front().translate);
        samples.push_back({ t, offset * (1.0f / VrGameUnitsPerMetre) });
        poses.push_back(pose);
      }

      VrRackMotion motion;

      if (!vrFindRackMotion(samples.data(), samples.size(), motion))
        return false;

      animation.axis   = motion.axis;
      animation.travel = motion.travel;
      animation.poses.clear();

      float along = 0.0f;

      for (size_t i = motion.start; i <= motion.back; i++) {
        along = std::max(along, vrDot(samples[i].offset, motion.axis));

        VrRackPose pose;
        pose.along          = along;
        pose.pose.rotate    = poses[i].rotate;
        pose.pose.translate = poses[i].translate;
        animation.poses.push_back(pose);
      }

      // The left hand holds the part all the way back if it is at its
      // centre then; the right hand may rack some weapons
      float time = samples[motion.back].time;
      from = samples[motion.start].time;
      to   = samples[motion.home].time;

      Pose hand, part;
      animation.held = false;

      if (tracks.toAncestor(leftHand, root, time, hand) && tracks.toAncestor(rack, root, time, part)
       && hand.scale > 0.0f) {
        VrVector3 middle = part.translate + part.rotate * meshCentre(rack) * part.scale;
        float distance   = vrLength(middle - hand.translate) / VrGameUnitsPerMetre;

        VrGameRotation toHand = vrTranspose(hand.rotate);
        animation.held                 = distance <= RackHeldReach;
        animation.partInHand.rotate    = toHand * part.rotate;
        animation.partInHand.translate = (toHand * (part.translate - hand.translate)) * (1.0f / hand.scale);
        animation.fingers              = animation.held ? readFingers(tracks, leftHand, time) : std::vector<VrBonePose>();

        Logger::info(str::format("VR: Reload animation racks '", partName(rack), "' ", motion.travel * 100.0f,
          " cm back along ", motion.axis.x, ", ", motion.axis.y, ", ", motion.axis.z, " in the weapon, from ",
          from, " to ", time, " s, home at ", to, " s; its centre is then ", distance * 100.0f,
          " cm from the left hand bone", animation.held ? ", which holds it" : ", which does not hold it"));
      }

      return true;
    }


    void readSounds(const uint8_t* sequence, const VrMagazineTiming& timing, float rackFrom, float rackTo, VrReloadAnimation& animation) {
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

      int32_t out = animation.round ? -1
        : vrFindNearestSound(times.data(), times.size(), timing.out, SoundWindow, -1);
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

      // The rack's sound is neither of the magazine's; it is played from
      // the time the part leaves its place to the time it is home again
      std::vector<float> others = times;

      for (int32_t index : { out, in }) {
        if (index >= 0)
          others[index] = FLT_MAX;
      }

      int32_t rack = animation.hasRack
        ? vrFindSoundDuring(others.data(), others.size(), rackFrom, rackTo, SoundWindow, -1)
        : -1;

      animation.out = toSound(out);
      animation.in  = toSound(in);

      if (animation.hasRack)
        animation.rack.sound = toSound(rack);

      for (size_t i = 0; i < entries.size(); i++) {
        Logger::info(str::format("VR: Reload animation sound at ", times[i], " s: '",
          vrGameSoundFile(readField<const uint8_t*>(entries[i], VrGame::AnimGroupSoundForm)), "'",
          int32_t(i) == out  ? ", the magazine going out" : "",
          int32_t(i) == in   ? (animation.round ? ", the round going in" : ", the magazine going in") : "",
          int32_t(i) == rack ? ", the weapon chambering" : ""));
      }
    }

  }


  bool vrReadReloadAnimation(
          uint8_t*              player,
          uint8_t*              root,
          uint8_t*              weapon,
          uint8_t*              magazine,
          bool                  round,
          uint8_t*              rack,
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
    std::vector<Pose> poses;

    for (uint32_t i = 0; begin + float(i) / SamplesPerSecond <= end; i++) {
      float time = begin + float(i) / SamplesPerSecond;
      Pose pose;

      if (!tracks.toAncestor(magazine, weapon, time, pose))
        return false;

      VrVector3 offset = poses.empty() ? VrVector3() : (pose.translate - poses.front().translate);
      samples.push_back({ time, offset * (1.0f / VrGameUnitsPerMetre) });
      poses.push_back(pose);
    }

    VrMagazineTiming timing;
    size_t place = 0;

    animation.round = round;

    if (round ? !vrFindRoundTiming(samples.data(), samples.size(), timing, place)
              : !vrFindMagazineTiming(samples.data(), samples.size(), timing)) {
      Logger::info(str::format("VR: The reload animation '", sequenceName ? sequenceName : "",
        round ? "' does not carry the round in" : "' does not take the magazine out and put it back"));
      return false;
    }

    if (round) {
      animation.place.rotate    = poses[place].rotate;
      animation.place.translate = poses[place].translate;
    }

    // The centre of the magazine's meshes in its own frame
    VrVector3 centre = meshCentre(magazine);

    // The magazine in the left hand's frame while it is out, to find when
    // the hand carries it
    std::vector<VrGameTransform> inHand;
    std::vector<float> inHandTimes;

    for (const VrMagazineSample& sample : samples) {
      Pose hand, held;

      if (sample.time < timing.out || sample.time >= timing.in
       || !tracks.toAncestor(leftHand, root, sample.time, hand)
       || !tracks.toAncestor(magazine, root, sample.time, held))
        continue;

      VrGameRotation toHand = vrTranspose(hand.rotate);
      VrVector3 middle = held.translate + held.rotate * centre * held.scale;

      VrGameTransform transform;
      transform.rotate    = toHand * held.rotate;
      transform.translate = toHand * (middle - hand.translate) * (1.0f / VrGameUnitsPerMetre);

      // What the hand does through the animation, for findings/reload.md
      if (!inHand.empty() && !(inHand.size() % 3)) {
        const VrGameTransform& last = inHand.back();
        Logger::info(str::format("VR: Reload animation at ", sample.time, " s: the ", round ? "round " : "magazine ",
          vrLength(sample.offset) * 100.0f, " cm out, its centre ", vrLength(transform.translate) * 100.0f,
          " cm from the left hand bone, moving ", vrLength(transform.translate - last.translate) * 100.0f,
          " cm and ", vrRotationAngle(last.rotate, transform.rotate) * 57.2957795f, " degrees in the hand"));
      }

      inHand.push_back(transform);
      inHandTimes.push_back(sample.time);
    }

    size_t carried = vrFindMagazineHold(inHand.data(), inHand.size());

    if (carried < inHand.size())
      timing.grip = inHandTimes[carried];

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
    animation.fingers = readFingers(tracks, leftHand, timing.grip);

    float rackFrom = 0.0f;
    float rackTo   = 0.0f;
    animation.hasRack = rack && readRack(tracks, root, weapon, rack, leftHand, begin, end, animation.rack, rackFrom, rackTo);

    if (rack && !animation.hasRack)
      Logger::info(str::format("VR: The reload animation does not rack '", partName(rack), "'"));

    readSounds(sequence, timing, rackFrom, rackTo, animation);

    Logger::info(str::format("VR: Reload animation '", sequenceName ? sequenceName : "", "' from ", begin, " to ", end,
      round ? " s: the round is carried in from " : " s: the magazine goes out at ", timing.out, " s and in at ", timing.in, " s, along ",
      timing.axis.x, ", ", timing.axis.y, ", ", timing.axis.z, " in the weapon; the left hand holds it at ", timing.grip,
      carried < inHand.size() ? " s, where it carries it" : " s, before it goes in (the hand never carries it)",
      ", ", animation.magazineInHand.translate.x, ", ", animation.magazineInHand.translate.y, ", ",
      animation.magazineInHand.translate.z, " units from the hand bone, with ", animation.fingers.size(), " finger bone(s)"));
    return true;
  }


  void vrPlayReloadSound(
    const VrReloadSound&        sound,
    const VrVector3&            position) {
    auto player = *reinterpret_cast<uint8_t* const*>(VrGame::Player);

    if (!sound.sound || !player)
      return;

    uint32_t soundFlags = readField<uint32_t>(sound.sound, VrGame::SoundFormFlags);

    uint32_t flags = ((soundFlags & VrGame::SoundForm2D) ? VrGame::SoundFlag2D : VrGame::SoundFlag3D)
      | VrGame::SoundFlagOneShot | VrGame::SoundFlagFirstPerson
      | VrGame::SoundFlagIgnoreTimescale | VrGame::SoundFlagAnimationDriven;

    // As fast as the reload animation would play
    auto animation = readField<const uint8_t*>(player, VrGame::PlayerFirstPersonAnimData);
    float speed = animation ? readField<float>(animation, VrGame::AnimDataReloadSpeed) : 1.0f;

    vrPlayGameSound(sound.sound, flags, position, sound.volume, sound.pitch * speed);
  }

}
