#include <algorithm>
#include <cfloat>
#include <cstring>

#include "../../util/log/log.h"
#include "../../util/util_string.h"

#include "vr_game_addresses.h"
#include "vr_game_nodes.h"
#include "vr_sound_keys.h"
#include "vr_throw_animation.h"

namespace dxvk {

  namespace {

    // How many times per second of the animation the hand is sampled
    constexpr float SamplesPerSecond = 60.0f;

    // How long before the hand acts on the part its sound may start, in
    // seconds of the animation: the hand's own sounds before it are the
    // preparation
    constexpr float ActSoundLead = 0.1f;

    // The parts below the lighter and the button are a few levels deep
    constexpr uint32_t MaxPartDepth = 6;

    // The weapon's attack animation when it has none of its own
    constexpr uint32_t NoAttackAnimation = 0xFF;

    // How far from the left hand bone the lighter's meshes may be for the
    // hand to hold it, in game units: the bone sits at the wrist
    constexpr float MaxLighterFromHand = 0.12f * VrGameUnitsPerMetre;

    // How often the lighter's distance to the hand is logged, in seconds
    constexpr float LighterLogStep = 0.1f;

    // NiInterpolator::IsFloatValueSupported, __thiscall without arguments
    using IsFloatFn = uint32_t (__fastcall*)(uint8_t* interpolator, void* unused);

    // NiInterpolator::UpdateFloat, __thiscall with three stack arguments
    using UpdateFloatFn = uint32_t (__fastcall*)(uint8_t* interpolator, void* unused,
      float time, uint8_t* target, float* value);

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
      const char* name = object ? readField<const char*>(object, VrGame::ObjectName) : nullptr;
      return name ? name : "";
    }

    /**
     * \brief The centre of a part's meshes in its own frame
     */
    VrVector3 meshCentre(const uint8_t* part) {
      float scale = readField<float>(part, VrGame::ObjectWorldScale);
      auto bound  = readField<const uint8_t*>(part, VrGame::ObjectWorldBound);

      if (!bound || scale <= 0.0f || readField<float>(bound, sizeof(VrVector3)) <= 0.0f)
        return VrVector3();

      return vrTranspose(readRotation(part, VrGame::ObjectWorldRotation))
        * (readVector(bound, 0) - readVector(part, VrGame::ObjectWorldTranslation)) * (1.0f / scale);
    }

    /**
     * \brief Where a point of a node is in the root's frame at a time
     */
    bool pointAt(const VrSequenceTracks& tracks, uint8_t* node, const uint8_t* root,
        const VrVector3& local, float time, VrVector3& point) {
      VrSequencePose pose;

      if (!tracks.toAncestor(node, root, time, pose))
        return false;

      point = pose.translate + pose.rotate * local * pose.scale;
      return true;
    }

    /**
     * \brief The time the animation lets go of the weapon
     *
     * Its Release key, or its Hit key, or its end. Every key is logged.
     */
    float findRelease(const uint8_t* sequence, float end) {
      auto textKeys  = readField<const uint8_t*>(sequence, VrGame::SequenceTextKeys);
      auto keys      = textKeys ? readField<const uint8_t*>(textKeys, VrGame::TextKeyArray) : nullptr;
      uint32_t count = textKeys ? readField<uint32_t>(textKeys, VrGame::TextKeyCount) : 0;

      float release = -1.0f;
      float hit     = -1.0f;
      std::string list;

      for (uint32_t i = 0; keys && i < count; i++) {
        const uint8_t* key = keys + i * VrGame::TextKeySize;
        const char* text = readField<const char*>(key, VrGame::TextKeyText);
        float time = readField<float>(key, 0);

        if (release < 0.0f && vrIsReleaseKey(text))
          release = time;

        if (hit < 0.0f && vrIsHitKey(text))
          hit = time;

        if (text)
          list += str::format(list.empty() ? "" : ", ", "'", std::string(text, std::strcspn(text, "\r")), "' at ", time, " s");
      }

      Logger::info(str::format("VR: Throw animation keys: ", list.empty() ? std::string("none") : list));
      return release >= 0.0f ? release : (hit >= 0.0f ? hit : end);
    }

    /**
     * \brief The parts below a node the sequence moves, at a time
     *
     * \param [in] self Whether the node itself counts
     */
    std::vector<VrPartPose> readParts(const VrSequenceTracks& tracks, uint8_t* node, bool self, float time) {
      std::vector<VrPartPose> parts;

      vrVisitGameObjects(reinterpret_cast<uintptr_t>(node), MaxPartDepth,
        [&tracks, &parts, node, self, time] (uintptr_t object, uint32_t) {
          auto part = reinterpret_cast<uint8_t*>(object);

          if ((part == node && !self) || !tracks.moves(part))
            return;

          VrSequencePose pose = tracks.local(part, time);
          parts.push_back({ objectName(part), { pose.rotate, pose.translate }, pose.scale });
        });

      return parts;
    }

    /**
     * \brief The time the animation holds the lighter nearest the left
     *    hand bone, up to the release
     *
     * The distance is logged along the animation.
     */
    float findLighterHold(const VrSequenceTracks& tracks, const VrThrowAnimationNodes& nodes,
        float begin, float release, float& distance) {
      VrVector3 centre = meshCentre(nodes.lighter);
      float hold = begin;
      float nextLog = begin;
      std::string curve;

      distance = FLT_MAX;

      for (uint32_t i = 0; begin + float(i) / SamplesPerSecond <= release; i++) {
        float time = begin + float(i) / SamplesPerSecond;
        VrVector3 hand, lighter;

        if (!pointAt(tracks, nodes.leftHand, nodes.root, VrVector3(), time, hand)
         || !pointAt(tracks, nodes.lighter, nodes.root, centre, time, lighter))
          break;

        float d = vrLength(lighter - hand);

        if (d < distance) {
          distance = d;
          hold     = time;
        }

        if (time >= nextLog) {
          curve  += str::format(curve.empty() ? "" : ", ", int(d / VrGameUnitsPerMetre * 100.0f + 0.5f));
          nextLog = time + LighterLogStep;
        }
      }

      Logger::info(str::format("VR: Throw animation lighter", tracks.moves(nodes.lighter) ? " (animated)" : " (not animated)",
        " to the left hand bone, cm every ", LighterLogStep, " s: ", curve.empty() ? std::string("none") : curve));
      return hold;
    }

    /**
     * \brief The most particles per second the sequence has the emitter
     *    of a particle system give, 0 without such a track
     *
     * Its float tracks on the particle system, the emitter's birth rate,
     * sampled from \p begin to \p end. Each is logged.
     */
    float readParticleRate(const uint8_t* sequence, uint8_t* system, float begin, float end) {
      uint32_t count = readField<uint32_t>(sequence, VrGame::SequenceArraySize);
      auto interps   = readField<const uint8_t*>(sequence, VrGame::SequenceInterpolators);
      auto tags      = readField<const uint8_t*>(sequence, VrGame::SequenceIdTags);
      const char* name = objectName(system);

      float rate = 0.0f;

      for (uint32_t i = 0; interps && tags && i < count; i++) {
        auto interpolator = readField<uint8_t*>(interps + i * VrGame::SequenceInterpolatorSize, 0);
        auto target       = readField<const char*>(tags + i * VrGame::SequenceIdTagSize, 0);
        auto controller   = readField<const char*>(tags + i * VrGame::SequenceIdTagSize, VrGame::SequenceIdTagControllerType);

        if (!interpolator || !target || std::strcmp(target, name))
          continue;

        auto vtable    = readField<const uint8_t*>(interpolator, 0);
        auto isFloat   = reinterpret_cast<IsFloatFn>(readField<uintptr_t>(vtable, VrGame::InterpolatorIsFloatSlot));
        auto readFloat = reinterpret_cast<UpdateFloatFn>(readField<uintptr_t>(vtable, VrGame::InterpolatorUpdateFloatSlot));

        if (!(isFloat(interpolator, nullptr) & 0xFF))
          continue;

        float highest = 0.0f;
        float first   = -1.0f;

        for (uint32_t j = 0; begin + float(j) / SamplesPerSecond <= end; j++) {
          float time  = begin + float(j) / SamplesPerSecond;
          float value = 0.0f;

          if (!(readFloat(interpolator, nullptr, time, system, &value) & 0xFF))
            continue;

          if (value > 0.0f && first < 0.0f)
            first = time;

          highest = std::max(highest, value);
        }

        Logger::info(str::format("VR: Throw animation particles of '", name, "' (", controller ? controller : "",
          "): up to ", highest, " per second", first >= 0.0f ? str::format(" from ", first, " s") : std::string()));
        rate = std::max(rate, highest);
      }

      return rate;
    }

    void readSounds(const uint8_t* sequence, float act, float release, VrThrowAnimation& animation) {
      auto group = readField<const uint8_t*>(sequence, VrGame::AnimSequenceGroup);

      if (!group)
        return;

      uint32_t count = readField<uint32_t>(group, VrGame::AnimGroupSoundCount);
      auto sounds    = readField<const uint8_t*>(group, VrGame::AnimGroupSounds);

      float prepareStart = -1.0f;
      float armStart     = -1.0f;

      for (uint32_t i = 0; sounds && i < count; i++) {
        const uint8_t* entry = sounds + i * VrGame::AnimGroupSoundSize;
        float time = readField<float>(entry, 0);

        VrThrowSound sound;
        sound.sound.sound  = readField<const uint8_t*>(entry, VrGame::AnimGroupSoundForm);
        sound.sound.volume = float(readField<uint8_t>(entry, VrGame::AnimGroupSoundVolume)) / 255.0f;
        sound.sound.pitch  = readField<float>(entry, VrGame::AnimGroupSoundPitch);
        sound.loops        = vrGameSoundLoops(sound.sound.sound);

        // Entries without a sound are footsteps and other events
        if (!sound.sound.sound)
          continue;

        const char* use = "after the release, not played";

        if (time <= release) {
          bool prepare = time < act - ActSoundLead;
          float& start = prepare ? prepareStart : armStart;

          // The keys are in time order
          if (start < 0.0f)
            start = time;

          sound.delay = time - start;
          (prepare ? animation.prepare : animation.arm).push_back(sound);
          use = prepare ? "played as the left hand gets to the part" : "played when it is armed";
        }

        Logger::info(str::format("VR: Throw animation sound at ", time, " s: '", vrGameSoundFile(sound.sound.sound), "'",
          sound.loops ? " (loops)" : "", ", ", use));
      }
    }

  }


  bool vrReadThrowAnimation(
          uint8_t*              player,
    const uint8_t*              weapon,
          VrThrowArming         arming,
    const VrThrowAnimationNodes& nodes,
          VrThrowAnimation&     animation) {
    animation = VrThrowAnimation();

    uint32_t group = readField<uint8_t>(weapon, VrGame::WeaponAttackAnimation);
    const uint8_t* sequence = group != NoAttackAnimation ? vrFindPlayerSequence(player, group) : nullptr;

    if (!sequence) {
      Logger::info(str::format("VR: The thrown weapon has no first-person attack animation (", group, ")"));
      return false;
    }

    float begin   = readField<float>(sequence, VrGame::SequenceBeginKeyTime);
    float end     = readField<float>(sequence, VrGame::SequenceEndKeyTime);
    float release = findRelease(sequence, end);

    VrSequenceTracks tracks(sequence);

    // When the left hand acts on the part: the flame nearest the fuse, or
    // the hand bone nearest the part
    uint8_t* actor = arming == VrThrowArming::Fuse ? (nodes.flame ? nodes.flame : nodes.lighter) : nodes.leftHand;
    float act      = release;
    float nearest  = FLT_MAX;

    if (actor && nodes.target && nodes.leftHand && nodes.root) {
      VrVector3 centre = meshCentre(nodes.target);

      for (uint32_t i = 0; begin + float(i) / SamplesPerSecond <= release; i++) {
        float time = begin + float(i) / SamplesPerSecond;
        VrVector3 from, to;

        if (!pointAt(tracks, actor, nodes.root, VrVector3(), time, from)
         || !pointAt(tracks, nodes.target, nodes.root, centre, time, to))
          break;

        float distance = vrLength(to - from);

        if (distance < nearest) {
          nearest = distance;
          act     = time;
        }
      }
    }

    // The lighter is held as the animation holds it when it is nearest the
    // left hand bone: the flame may reach the fuse while the hand is still
    // bringing it there
    float hold          = act;
    float lighterToHand = FLT_MAX;

    if (nodes.lighter && nodes.leftHand && nodes.root)
      hold = findLighterHold(tracks, nodes, begin, release, lighterToHand);

    if (nodes.leftHand)
      animation.fingers = vrReadSequenceFingers(tracks, nodes.leftHand, nodes.lighter ? hold : act);

    // The lighter in the hand, lit
    VrSequencePose hand, lighter;

    if (lighterToHand <= MaxLighterFromHand
     && tracks.toAncestor(nodes.leftHand, nodes.root, hold, hand)
     && tracks.toAncestor(nodes.lighter, nodes.root, hold, lighter)
     && hand.scale > 0.0f) {
      VrGameRotation toHand = vrTranspose(hand.rotate);
      animation.lighterInHand.rotate    = toHand * lighter.rotate;
      animation.lighterInHand.translate = (toHand * (lighter.translate - hand.translate)) * (1.0f / hand.scale);
      animation.hasLighter = true;
      animation.lighterLit = readParts(tracks, nodes.lighter, false, act);

      // Where it stands from: its meshes' centre towards its flame, or
      // its node's y axis without one
      VrVector3 centre = lighter.translate + lighter.rotate * meshCentre(nodes.lighter) * lighter.scale;
      VrVector3 up     = lighter.rotate * VrVector3 { 0.0f, 1.0f, 0.0f };
      VrVector3 flame;

      if (nodes.flame && pointAt(tracks, nodes.flame, nodes.root, meshCentre(nodes.flame), hold, flame)
       && vrLength(flame - centre) > 0.0f)
        up = (flame - centre) * (1.0f / vrLength(flame - centre));

      animation.lighterCentre = (toHand * (centre - hand.translate)) * (1.0f / hand.scale);
      animation.lighterUp     = toHand * up;
    }

    // A switch is left thrown
    if (arming == VrThrowArming::Button && nodes.button)
      animation.pressed = readParts(tracks, nodes.button, true, release);

    if (nodes.sparks)
      animation.sparkRate = readParticleRate(sequence, nodes.sparks, begin, end);

    readSounds(sequence, act, release, animation);

    auto name = readField<const char*>(sequence, VrGame::ObjectName);

    Logger::info(str::format("VR: Throw animation '", name ? name : "", "' (group ", group, ") from ", begin,
      " to ", end, " s, released at ", release, " s; the left hand acts on '", objectName(nodes.target), "' at ", act,
      nearest < FLT_MAX ? str::format(" s, ", nearest / VrGameUnitsPerMetre * 100.0f, " cm from ", actor == nodes.leftHand ? "the hand bone" : "the flame")
                        : std::string(" s (not found)"),
      ", with ", animation.fingers.size(), " finger bone(s)",
      animation.hasLighter ? str::format(", the lighter held at ", hold, " s, its meshes ", lighterToHand / VrGameUnitsPerMetre * 100.0f,
        " cm from the hand bone, with ", animation.lighterLit.size(), " part(s) moved lit")
      : nodes.lighter ? std::string(", the lighter never in the left hand") : std::string(),
      arming == VrThrowArming::Button ? str::format(", ", animation.pressed.size(), " part(s) moved once pressed") : std::string(),
      "; ", animation.prepare.size(), " sound(s) on the way, ", animation.arm.size(), " when armed"));
    return true;
  }

}
