#include <algorithm>
#include <array>
#include <cstring>
#include <iterator>

#include "../../util/log/log.h"
#include "../../util/util_string.h"

#include "vr_game_addresses.h"
#include "vr_game_memory.h"
#include "vr_game_nodes.h"
#include "vr_weapon_hand.h"
#include "vr_weapon_parts.h"

namespace dxvk {

  namespace {

    // NiAVObject::Update is __thiscall with one stack argument
    using ObjectUpdateFn = void (__fastcall*)(uint8_t* object, void* unused, void* updateData);

    // Projectile::Create is __cdecl with 16 arguments and returns the projectile
    using ProjectileCreateFn = void* (__cdecl*)(
      void* projectile, void* shooter, void* target, void* weapon,
      float x, float y, float z, float heading, float pitch,
      uint32_t a9, uint32_t a10, uint32_t a11, uint32_t a12,
      float spreadHeading, float spreadPitch, uint32_t a15);

    // PlayerCharacter::ApplyAutoAimActor is __thiscall with 7 stack arguments
    using ApplyAutoAimFn = void (__fastcall*)(uint8_t* player, void* unused,
      void* projectile, void* releaseNode, float* heading, float* pitch,
      void* origin, float startHeading, float startPitch);

    // Process virtual function, __thiscall without arguments, returns a
    // bool in AL
    using IsWeaponOutFn = uint32_t (__fastcall*)(uint8_t* process, void* unused);

    // NiObject::GetAsNiGeometry, __thiscall without arguments
    using GetAsGeometryFn = uint8_t* (__fastcall*)(uint8_t* object, void* unused);

    // Process virtual function, __thiscall without arguments, returns the
    // equipped weapon's inventory entry
    using GetWeaponInfoFn = uint8_t* (__fastcall*)(uint8_t* process, void* unused);

    // AnimData::MorphOrBlendToSequence, __thiscall with three stack
    // arguments, returns the sequence
    using PlaySequenceFn = void* (__fastcall*)(uint8_t* animData, void* unused,
      void* sequence, uint32_t groupId, int32_t sequenceType);

    // The weapon node is a few levels below the skeleton's root, and the
    // muzzle a few levels below the weapon
    constexpr uint32_t MaxModelDepth = 16;

    // How close the other controller must come to where the game holds the
    // weapon with that hand for its grip to take the weapon
    constexpr float TwoHandedGripDistance = 0.15f * VrGameUnitsPerMetre;

    // How close the controllers must be for the other hand to support a
    // one-handed weapon
    constexpr float BracedGripDistance = 0.2f * VrGameUnitsPerMetre;

    // Animation types of the game's weapons, see findings/weapon.md
    constexpr int8_t WeaponTypeNone         = -1;
    constexpr int8_t WeaponTypeOneHandMelee = 1;
    constexpr int8_t WeaponTypeTwoHandMelee = 2;
    constexpr int8_t WeaponTypePistol       = 3;
    constexpr int8_t WeaponTypePistolEnergy = 4;
    constexpr int8_t WeaponTypeRifle        = 5;
    constexpr int8_t WeaponTypeLauncher     = 9;

    constexpr float Pi               = 3.14159265f;
    constexpr float DegreesPerRadian = 180.0f / Pi;

    VrWeaponHand* g_weaponHand = nullptr;

    template<typename T>
    T& field(uint8_t* base, uintptr_t offset) {
      return *reinterpret_cast<T*>(base + offset);
    }

    template<typename T>
    T readField(const uint8_t* base, uintptr_t offset) {
      return *reinterpret_cast<const T*>(base + offset);
    }

    uint8_t* player() {
      return *reinterpret_cast<uint8_t* const*>(VrGame::Player);
    }

    VrVector3 readVector(const uint8_t* object, uintptr_t offset) {
      VrVector3 v;
      std::memcpy(&v, object + offset, sizeof(v));
      return v;
    }

    void writeVector(uint8_t* object, uintptr_t offset, const VrVector3& v) {
      std::memcpy(object + offset, &v, sizeof(v));
    }

    VrGameRotation readRotation(const uint8_t* object, uintptr_t offset) {
      VrGameRotation r;
      std::memcpy(r.m, object + offset, sizeof(r.m));
      return r;
    }

    void writeRotation(uint8_t* object, uintptr_t offset, const VrGameRotation& r) {
      std::memcpy(object + offset, r.m, sizeof(r.m));
    }

    /**
     * \brief World transform of a node's parent, identity without a parent
     *
     * A node's world transform is its parent's applied to its local one:
     * rotation P.R * L.R, translation P.t + P.s * P.R * L.t.
     */
    struct ParentFrame {
      VrGameRotation rotate;
      VrVector3      translate;
      float          scale = 1.0f;

      VrVector3 toWorld(const VrVector3& local) const {
        return translate + (rotate * local) * scale;
      }

      VrVector3 toLocal(const VrVector3& world) const {
        return (vrTranspose(rotate) * (world - translate)) * (1.0f / scale);
      }
    };

    ParentFrame parentFrame(uint8_t* object) {
      ParentFrame frame;
      uint8_t* parent = field<uint8_t*>(object, VrGame::ObjectParent);

      if (parent) {
        frame.rotate    = readRotation(parent, VrGame::ObjectWorldRotation);
        frame.translate = readVector(parent, VrGame::ObjectWorldTranslation);
        frame.scale     = field<float>(parent, VrGame::ObjectWorldScale);
      }

      return frame;
    }

    void updateObject(uint8_t* object, void* updateData) {
      reinterpret_cast<ObjectUpdateFn>(VrGame::ObjectUpdate)(object, nullptr, updateData);
    }

    uint8_t* findObject(uint8_t* root, const char* name) {
      return reinterpret_cast<uint8_t*>(
        vrFindGameObject(reinterpret_cast<uintptr_t>(root), name, MaxModelDepth));
    }

    bool isAncestor(const uint8_t* ancestor, uint8_t* object) {
      for (; object; object = field<uint8_t*>(object, VrGame::ObjectParent)) {
        if (object == ancestor)
          return true;
      }

      return false;
    }

    /**
     * \brief Top bone of the arm holding a hand
     *
     * The highest ancestor of the hand that is not also an ancestor of the
     * other hand: the clavicle in the game's skeleton.
     */
    uint8_t* armBone(uint8_t* hand, uint8_t* otherHand) {
      uint8_t* bone = hand;

      while (uint8_t* parent = field<uint8_t*>(bone, VrGame::ObjectParent)) {
        if (isAncestor(parent, otherHand))
          break;

        bone = parent;
      }

      return bone;
    }

    /**
     * \brief Bones of one arm above the forearm
     *
     * The hand's meshes are skinned to the hand, the fingers and the
     * forearm. Only the meshes of the sleeves and the upper arms also use
     * the bones between the forearm and the clavicle.
     */
    struct UpperArm {
      uint8_t* top     = nullptr;
      uint8_t* forearm = nullptr;

      bool contains(uint8_t* bone) const {
        return top != forearm && isAncestor(top, bone) && !isAncestor(forearm, bone);
      }
    };

    uint8_t* asGeometry(uint8_t* object) {
      auto vtable = readField<const uint8_t*>(object, 0);
      auto getAsGeometry = reinterpret_cast<GetAsGeometryFn>(readField<uintptr_t>(vtable, VrGame::ObjectGetAsGeometrySlot));
      return getAsGeometry(object, nullptr);
    }

    bool isArmMesh(uint8_t* geometry, const UpperArm (&arms)[2]) {
      auto skin = field<uint8_t*>(geometry, VrGame::GeometrySkinInstance);

      if (!skin)
        return false;

      auto data  = field<uint8_t*>(skin, VrGame::SkinInstanceData);
      auto bones = field<uint8_t**>(skin, VrGame::SkinInstanceBones);

      if (!data || !bones)
        return false;

      uint32_t count = field<uint32_t>(data, VrGame::SkinDataBoneCount);

      for (uint32_t i = 0; i < count; i++) {
        if (bones[i] && (arms[0].contains(bones[i]) || arms[1].contains(bones[i])))
          return true;
      }

      return false;
    }

    /**
     * \brief Whether a mesh is skinned to a bone or to one below it
     */
    bool isSkinnedBelow(uint8_t* geometry, const uint8_t* bone) {
      auto skin = field<uint8_t*>(geometry, VrGame::GeometrySkinInstance);

      if (!skin)
        return false;

      auto data  = field<uint8_t*>(skin, VrGame::SkinInstanceData);
      auto bones = field<uint8_t**>(skin, VrGame::SkinInstanceBones);

      if (!data || !bones)
        return false;

      uint32_t count = field<uint32_t>(data, VrGame::SkinDataBoneCount);

      for (uint32_t i = 0; i < count; i++) {
        if (bones[i] && isAncestor(bone, bones[i]))
          return true;
      }

      return false;
    }

    template<typename Fn>
    void forEachObject(uint8_t* object, uint32_t depth, const Fn& fn) {
      fn(object);

      uintptr_t node = vrGameAsNode(reinterpret_cast<uintptr_t>(object));

      if (!node || !depth)
        return;

      auto children = readField<uint8_t* const*>(reinterpret_cast<const uint8_t*>(node), VrGame::NodeChildren);
      auto count    = readField<uint16_t>(reinterpret_cast<const uint8_t*>(node), VrGame::NodeChildCount);

      for (uint32_t i = 0; children && i < count; i++) {
        if (children[i])
          forEachObject(children[i], depth - 1, fn);
      }
    }

    /**
     * \brief Pose of the free left hand, relative to its controller
     *
     * Measured in game with the 9mm pistol (2026-10-11), on the hand the
     * VR layer then held as the mirror image of the right hand: the hand
     * bone's rotation and offset in the left controller's frame (forward,
     * up and right, as in vrCameraRotation, in game units), and the local
     * rotations of the bones below it. Every weapon gets this pose: some,
     * such as the Fat Man, hold the weapon with an unusual grip that a
     * mirror of the right hand would copy.
     *
     * The wrist comes from a second measurement with the 9mm pistol: the
     * hand's and the forearm twist bone's local rotations in the forearm,
     * which the Fat Man's animation bends and twists otherwise.
     */
    const VrGameTransform FreeHandPose = {
      {{ {  0.995157f, 0.0949835f, 0.0253174f },
         { 0.0288025f, -0.035501f, -0.998955f },
         { -0.0939852f,  0.994846f, -0.0380647f } }},
      { -8.81553f, -1.23918f, -1.42719f },
    };

    struct FreeHandBone {
      const char*    name;
      VrGameRotation rotate;
    };

    const VrGameRotation FreeHandWrist = {{
      {  0.992455f,  0.114828f, -0.0429711f },
      { 0.0934869f, -0.481991f,  0.871174f },
      { 0.0793239f, -0.868619f,  -0.48909f } }};

    const VrGameRotation FreeHandTwist = {{
      {       1.0f,       0.0f,       0.0f },
      {       0.0f,  0.850957f,  0.525236f },
      {       0.0f, -0.525236f,  0.850957f } }};

    const FreeHandBone FreeHandBones[] = {
      { VrGame::LeftHandNodeName, FreeHandWrist },
      { VrGame::LeftForeTwistNodeName, FreeHandTwist },
      { "Bip01 L Thumb1", {{ {  0.818252f,  0.384053f,  0.427746f },
                             {  0.366491f,  0.224751f, -0.902869f },
                             { -0.442886f,  0.895539f, 0.0431507f } }} },
      { "Bip01 L Thumb11", {{ {  0.967436f, -0.253117f,       0.0f },
                              {  0.253117f,  0.967436f,       0.0f },
                              {       0.0f,       0.0f,       1.0f } }} },
      { "Bip01 L Thumb12", {{ {  0.994304f, -0.106582f,       0.0f },
                              {  0.106582f,  0.994304f,       0.0f },
                              {       0.0f,       0.0f,       1.0f } }} },
      { "Bip01 L Hand01", {{ {  0.991822f, 0.0157553f,  0.126654f },
                             { -0.0121424f,  0.999498f, -0.0292477f },
                             { -0.127052f, 0.0274707f,  0.991516f } }} },
      { "Bip01 L Finger1", {{ {  0.997184f, 0.00861518f, -0.074502f },
                              { -0.0105104f,   0.99963f, -0.0250837f },
                              { 0.0742583f, 0.0257961f,  0.996905f } }} },
      { "Bip01 L Finger11", {{ {  0.985859f,  -0.16757f, -0.00119342f },
                               {  0.167571f,   0.98586f, 0.000681602f },
                               { 0.00106233f, -0.000871946f,  0.999999f } }} },
      { "Bip01 L Finger12", {{ {  0.999859f, 0.0156992f, 0.0060174f },
                               { -0.0157025f,  0.999877f, 0.000501849f },
                               { -0.00600878f, -0.000596266f,  0.999982f } }} },
      { "Bip01 L Hand02", {{ {  0.994522f,  0.104404f, 0.00510071f },
                             { -0.104171f,  0.985903f,  0.130934f },
                             { 0.00864123f, -0.130748f,  0.991378f } }} },
      { "Bip01 L Finger2", {{ {  0.988127f, -0.153207f, -0.0114936f },
                              {  0.152233f,  0.986443f, -0.0612823f },
                              { 0.0207266f, 0.0588051f,  0.998054f } }} },
      { "Bip01 L Finger21", {{ {   0.99929f, -0.0376753f, 0.000116667f },
                               { 0.0376753f,   0.99929f,       0.0f },
                               { -0.000116585f,       0.0f,       1.0f } }} },
      { "Bip01 L Finger22", {{ {  0.999258f, 0.0385054f,       0.0f },
                               { -0.0385054f,  0.999258f,       0.0f },
                               {       0.0f,       0.0f,       1.0f } }} },
      { "Bip01 L Hand03", {{ {  0.991125f, 0.0600862f,  -0.11858f },
                             { -0.0447177f,  0.990733f,  0.128255f },
                             {  0.125188f, -0.121814f,  0.984627f } }} },
      { "Bip01 L Finger3", {{ {  0.992671f, -0.120352f, 0.0109717f },
                              {  0.120378f,  0.992727f, -0.00169364f },
                              { -0.0106881f, 0.00300198f,  0.999938f } }} },
      { "Bip01 L Finger31", {{ {  0.998236f, -0.0586457f, 0.0092844f },
                               { 0.0586366f,  0.998279f, 0.00125062f },
                               { -0.00934176f, -0.000704007f,  0.999956f } }} },
      { "Bip01 L Finger32", {{ {  0.991462f,  0.130393f,       0.0f },
                               { -0.130393f,  0.991462f,       0.0f },
                               {       0.0f,       0.0f,       1.0f } }} },
      { "Bip01 L Hand04", {{ {  0.973566f, 0.0902254f, -0.209831f },
                             { -0.0499923f,  0.980572f,  0.189685f },
                             {  0.222868f, -0.174181f,  0.959162f } }} },
      { "Bip01 L Finger4", {{ {  0.992131f, -0.120628f, 0.0335495f },
                              {   0.12138f,  0.992377f, -0.0213405f },
                              { -0.0307195f, 0.0252448f,  0.999209f } }} },
      { "Bip01 L Finger41", {{ {  0.990757f, -0.135646f, -0.000347379f },
                               {  0.135646f,  0.990757f, 0.00097353f },
                               { 0.000212113f, -0.00101165f,  0.999999f } }} },
      { "Bip01 L Finger42", {{ {  0.999922f, -0.012493f, -6.19373e-05f },
                               {  0.012493f,  0.999922f,       0.0f },
                               { 6.22444e-05f,       0.0f,       1.0f } }} },
    };

  }


  VrWeaponHand::~VrWeaponHand() {
    // The patched calls stay in place and fall through to the game's functions
    g_weaponHand = nullptr;
  }


  std::unique_ptr<VrWeaponHand> VrWeaponHand::install() {
    if (g_weaponHand)
      return nullptr;

    bool found = VrGameMemory::matches(VrGame::FirstPersonZeroSite,
        VrGame::FirstPersonZero, sizeof(VrGame::FirstPersonZero))
      && VrGameMemory::matches(VrGame::FirstPersonCameraSite,
        VrGame::FirstPersonCamera, sizeof(VrGame::FirstPersonCamera))
      && VrGameMemory::callsTo(VrGame::ProjectileCreateCallSite, VrGame::ProjectileCreate);

    found = found && VrGameMemory::redirectCall(VrGame::FirstPersonUpdateCallSite,
      VrGame::ObjectUpdate, reinterpret_cast<const void*>(&VrWeaponHand::updateHook));

    // If this one fails, the Pip-Boy's screen is drawn where the game
    // animates the forearm, while the forearm is drawn on the controller
    if (found && !VrGameMemory::redirectCall(VrGame::PipBoyModelUpdateCallSite,
        VrGame::ObjectUpdate, reinterpret_cast<const void*>(&VrWeaponHand::pipBoyUpdateHook)))
      Logger::warn("VR: The Pip-Boy's model update was not found, its screen stays where the game animates it");

    // If this one fails, the weapon is drawn in the hand but shoots from
    // the game's muzzle, along the head
    if (found && !VrGameMemory::redirectCall(VrGame::ProjectileCreateCallSite,
        VrGame::ProjectileCreate, reinterpret_cast<const void*>(&VrWeaponHand::createProjectileHook)))
      Logger::warn("VR: The projectile call was not found, the weapon shoots along the head");

    // If this one fails, the hand's shots still bend towards where the head looks
    if (found && !VrGameMemory::redirectCall(VrGame::AutoAimCallSite,
        VrGame::ApplyAutoAim, reinterpret_cast<const void*>(&VrWeaponHand::autoAimHook)))
      Logger::warn("VR: The auto-aim call was not found, shots bend towards where the head looks");

    if (found) {
      bool redirected = true;

      for (uintptr_t site : VrGame::PlaySequenceCallSites) {
        redirected &= VrGameMemory::redirectCall(site,
          VrGame::PlaySequence, reinterpret_cast<const void*>(&VrWeaponHand::playSequenceHook));
      }

      if (!redirected)
        Logger::warn("VR: An animation call was not found, the left hand may reach for a pistol before aiming");
    }

    if (!found) {
      Logger::info("VR: The first-person model was not found, the weapon stays in front of the head");
      return nullptr;
    }

    std::unique_ptr<VrWeaponHand> weaponHand(new VrWeaponHand());
    g_weaponHand = weaponHand.get();
    return weaponHand;
  }


  void VrWeaponHand::beginFrame(
    const VrGameCameraPose&     gameCamera,
    const VrGameCameraPose*     hand,
    const VrGameCameraPose*     otherHand,
          bool                  grip,
          VrFirstPersonMode     mode,
          float                 dt) {
    if (mode != VrFirstPersonMode::Hands)
      hand = nullptr;

    m_frameTime         = dt;
    m_heldSettleUpdated = false;

    m_gameCamera   = gameCamera;
    m_hasHand      = hand != nullptr;
    m_hasOtherHand = otherHand != nullptr;
    m_grip         = grip;
    m_mode         = mode;

    if (hand)
      m_hand = *hand;
    else
      m_hasAim = false;

    if (m_snappedAim) {
      m_snapAim    = false;
      m_snappedAim = false;
    }

    // The grip lets go as soon as it is released, even if the model is not
    // drawn this frame
    if (!hand || !otherHand || !grip) {
      m_twoHanded = false;
      m_braced    = false;
    }

    if (otherHand)
      m_otherHand = *otherHand;
  }


  void VrWeaponHand::beforePlaceCamera(
          void*                 main,
    const VrVector3&            eyePosition) {
    m_main         = static_cast<uint8_t*>(main);
    m_eyePosition  = eyePosition;
    m_pipBoyPlaced = false;

    uint8_t* object = player();
    uint8_t* root   = object ? field<uint8_t*>(object, VrGame::PlayerFirstPersonRoot) : nullptr;

    if (!root)
      return;

    // PlaceCamera reads the root's world position to place its camera and
    // the lighting of the model. The previous eye left it where the model
    // was drawn.
    m_rootPosition = parentFrame(root).toWorld(readVector(root, VrGame::NodeLocalTranslate));
    writeVector(root, VrGame::ObjectWorldTranslation, m_rootPosition);
  }


  void __fastcall VrWeaponHand::updateHook(
          uint8_t*              object,
          void*                 unused,
          void*                 updateData) {
    updateObject(object, updateData);

    if (g_weaponHand && g_weaponHand->m_main)
      g_weaponHand->placeModel(object, updateData);
  }


  void __fastcall VrWeaponHand::pipBoyUpdateHook(
          uint8_t*              object,
          void*                 unused,
          void*                 updateData) {
    updateObject(object, updateData);

    // The update puts the forearm back where the game animates it, for the
    // Pip-Boy screen drawn after it. The model is at the same draw origin
    // as in PlaceCamera, so the eye's shift still applies.
    uint8_t* character = player();

    if (!g_weaponHand || !g_weaponHand->m_pipBoyPlaced || !character
     || object != field<uint8_t*>(character, VrGame::PlayerFirstPersonRoot))
      return;

    if (uint8_t* otherHand = findObject(object, VrGame::LeftHandNodeName))
      g_weaponHand->placeOtherHand(object, otherHand, g_weaponHand->m_pipBoyShift, updateData);
  }


  void* __cdecl VrWeaponHand::createProjectileHook(
          void* projectile, void* shooter, void* target, void* weapon,
          float x, float y, float z, float heading, float pitch,
          uint32_t a9, uint32_t a10, uint32_t a11, uint32_t a12,
          float spreadHeading, float spreadPitch, uint32_t a15) {
    uint8_t* object = player();
    bool handAimed = false;

    if (g_weaponHand && g_weaponHand->m_hasAim && object && shooter == object
     && !*reinterpret_cast<const uint32_t*>(VrGame::VatsCameraState)) {
      if (uint8_t* root = field<uint8_t*>(object, VrGame::PlayerFirstPersonRoot)) {
        VrVector3 muzzle = parentFrame(root).toWorld(readVector(root, VrGame::NodeLocalTranslate))
                         + g_weaponHand->m_muzzleOffset;
        VrHeadAngles aim = vrComputeAimAngles(g_weaponHand->m_aimDirection);

        x = muzzle.x;
        y = muzzle.y;
        z = muzzle.z;
        heading = aim.yaw + spreadHeading;
        pitch   = aim.pitch + spreadPitch;
        handAimed = true;
      }
    }

    // The projectile is initialised, and aimed by the auto-aim, before
    // Create returns
    if (g_weaponHand)
      g_weaponHand->m_handAimedShot = handAimed;

    void* result = reinterpret_cast<ProjectileCreateFn>(VrGame::ProjectileCreate)(
      projectile, shooter, target, weapon, x, y, z, heading, pitch,
      a9, a10, a11, a12, spreadHeading, spreadPitch, a15);

    if (g_weaponHand)
      g_weaponHand->m_handAimedShot = false;

    return result;
  }


  void __fastcall VrWeaponHand::autoAimHook(
          uint8_t*              player,
          void*                 unused,
          void*                 projectile,
          void*                 releaseNode,
          float*                heading,
          float*                pitch,
          void*                 origin,
          float                 startHeading,
          float                 startPitch) {
    // The game turns the player's projectiles towards the point under the
    // centre of its camera, up to a few degrees each way, then adds the gun
    // wobble and the spread again. With the camera on the head, that bends
    // the shots from where the weapon points towards where the head looks.
    // The weapon drawn in the hand already sways with the wobble, and the
    // spread is applied once when the shot is aimed.
    if (g_weaponHand && g_weaponHand->m_handAimedShot) {
      if (!g_weaponHand->m_loggedAutoAim) {
        Logger::info("VR: The game's auto-aim is skipped for shots aimed with the hand");
        g_weaponHand->m_loggedAutoAim = true;
      }
      return;
    }

    reinterpret_cast<ApplyAutoAimFn>(VrGame::ApplyAutoAim)(player, nullptr,
      projectile, releaseNode, heading, pitch, origin, startHeading, startPitch);
  }


  void* __fastcall VrWeaponHand::playSequenceHook(
          uint8_t*              animData,
          void*                 unused,
          void*                 sequence,
          uint32_t              groupId,
          int32_t               sequenceType) {
    auto playSequence = reinterpret_cast<PlaySequenceFn>(VrGame::PlaySequence);
    uint8_t* object = player();
    uint8_t  group  = uint8_t(groupId & 0xFF);

    bool snap = g_weaponHand && g_weaponHand->m_braced && g_weaponHand->m_snapAim
      && object && animData && animData == field<uint8_t*>(object, VrGame::PlayerFirstPersonAnimData)
      && group >= VrGame::AnimGroupAimIS && group <= VrGame::AnimGroupAimISDown;

    if (!snap)
      return playSequence(animData, nullptr, sequence, groupId, sequenceType);

    // The game's own way to start a sequence in its pose: the flag makes
    // the blend time zero. It is restored so that only this sequence skips
    // its blend. The aim's up and down variants start in the same game
    // update, so the brace stops snapping at the next frame.
    uint8_t& skipBlend = field<uint8_t>(animData, VrGame::AnimDataSkipBlend);
    uint8_t  previous  = skipBlend;

    skipBlend = 1;
    void* result = playSequence(animData, nullptr, sequence, groupId, sequenceType);
    skipBlend = previous;

    g_weaponHand->m_snappedAim = true;

    if (!g_weaponHand->m_loggedSnapAim) {
      Logger::info("VR: The aim animation starts without blending while the left hand supports the pistol");
      g_weaponHand->m_loggedSnapAim = true;
    }

    return result;
  }


  void VrWeaponHand::placeModel(uint8_t* root, void* updateData) {
    uint8_t* main = m_main;
    m_main = nullptr;

    uint8_t* object = player();

    if (!object || root != field<uint8_t*>(object, VrGame::PlayerFirstPersonRoot))
      return;

    // The game draws the model with the root's translation at zero, so
    // that its camera and lights sit near the origin. Positions in the
    // world are brought to that space by the same shift.
    ParentFrame parent = parentFrame(root);
    VrVector3 drawnRoot = readVector(root, VrGame::ObjectWorldTranslation);
    VrVector3 shift     = m_rootPosition - drawnRoot;

    if (!m_loggedFrame) {
      auto parentObject = field<uint8_t*>(root, VrGame::ObjectParent);
      auto parentName   = parentObject ? field<const char*>(parentObject, VrGame::ObjectName) : nullptr;

      Logger::info(str::format("VR: First-person model parent '", parentName ? parentName : "", "' at ",
        parent.translate.x, ", ", parent.translate.y, ", ", parent.translate.z, " scale ", parent.scale,
        ", drawn ", vrLength(shift), " units from the player"));
      m_loggedFrame = true;
    }

    // The size is written after the game's update, the next update shows it
    PipBoyNodes pipBoy = findPipBoy(root);
    measurePipBoyScreen(pipBoy);
    scalePipBoy(pipBoy);

    // The game places its camera at its own eye
    if (uint8_t* camera = field<uint8_t*>(main, VrGame::MainFirstPersonCamera))
      writeVector(camera, VrGame::CameraWorldTranslation, m_eyePosition - shift);

    if (m_mode == VrFirstPersonMode::Hidden) {
      m_hasAim    = false;
      m_hasBlade  = false;
      m_twoHanded = false;
      m_braced    = false;
      hideModel(root);
      return;
    }

    // The arms and the weapon are culled one by one from here on
    if (m_hidesModel)
      showArms(root);

    if (m_mode == VrFirstPersonMode::PipBoy) {
      m_hasAim    = false;
      m_hasBlade  = false;
      m_twoHanded = false;
      m_braced    = false;
      placePipBoyArm(root, shift, updateData);
      return;
    }

    uint8_t* weapon = m_hasHand && holdsWeapon(object)
      ? findObject(root, VrGame::WeaponNodeName) : nullptr;

    if (!weapon) {
      m_hasAim    = false;
      m_hasBlade  = false;
      m_twoHanded = false;
      m_braced    = false;
      showArms(root);
      return;
    }

    // Where the game animates the weapon and the other hand, before the
    // model is moved
    uint8_t* otherHand = findObject(root, VrGame::LeftHandNodeName);
    uint8_t* forearm   = otherHand ? field<uint8_t*>(otherHand, VrGame::ObjectParent) : nullptr;

    VrVector3 drawnWeapon = readVector(weapon, VrGame::ObjectWorldTranslation);
    VrVector3 drawnHand   = forearm ? readVector(otherHand, VrGame::ObjectWorldTranslation) : drawnWeapon;

    VrGameCameraPose hand = m_hand;
    hand.position = hand.position - shift;

    // A melee weapon is swung by the hand: it keeps the angle in the hand
    // that the game gives it in its idle pose, whatever the game animates
    // on it later, such as blocking. Until the game has settled a weapon
    // just drawn, it follows the game's animation as other weapons do.
    const uint8_t* form = weaponForm(object);
    bool melee = isMelee(form);
    VrGameRotation weaponRotation = readRotation(weapon, VrGame::ObjectWorldRotation);

    if (!melee)
      m_heldForm = nullptr;

    if (melee && (weapon != m_heldWeapon || form != m_heldForm)) {
      m_heldWeapon = weapon;
      m_heldForm   = form;
      m_heldSettle.reset();
      m_bladeMeasured = false;
    }

    if (melee && !m_heldSettleUpdated) {
      m_heldSettleUpdated = true;

      if (m_heldSettle.update(vrComputeHeldRotation(m_gameCamera, weaponRotation), m_frameTime)) {
        Logger::info(str::format("VR: The melee weapon is held still in the hand after ", m_heldSettle.waited(), " s",
          m_heldSettle.timedOut() ? ", the game kept animating it" : ", once the game settled it"));
      }
    }

    bool held = melee && m_heldSettle.settled();

    VrGameTransform rig = held
      ? vrComputeRigHoldingWeapon(weaponRotation, drawnWeapon, m_heldSettle.rotation(), hand)
      : vrComputeRigToHand(m_gameCamera, drawnWeapon, hand);

    // With both hands on a gun, it points along the line between them. A
    // two-handed melee weapon is swung by the right hand, the left hand
    // only rides on its handle.
    VrVector3 support = rig.rotate * (drawnHand - drawnWeapon);
    GripKind  grip    = weaponGrip(object);
    updateGrip(grip, forearm != nullptr, support);

    if (m_twoHanded && grip == GripKind::TwoHanded) {
      hand = vrComputeTwoHandedPose(hand, support, m_otherHand.position - m_hand.position);
      rig  = vrComputeRigToHand(m_gameCamera, drawnWeapon, hand);
    }

    // The new world transform of the root, brought back to its parent's frame
    VrGameRotation worldRotation = rig.rotate * readRotation(root, VrGame::ObjectWorldRotation);
    VrVector3 worldTranslation   = rig.rotate * drawnRoot + rig.translate;

    VrGameRotation rotation = readRotation(root, VrGame::ObjectLocalRotation);
    VrVector3 translation   = readVector(root, VrGame::NodeLocalTranslate);

    writeRotation(root, VrGame::ObjectLocalRotation, vrTranspose(parent.rotate) * worldRotation);
    writeVector(root, VrGame::NodeLocalTranslate, parent.toLocal(worldTranslation));
    updateObject(root, updateData);

    // PlaceCamera restores the translation itself
    writeRotation(root, VrGame::ObjectLocalRotation, rotation);
    writeVector(root, VrGame::NodeLocalTranslate, translation);

    if (forearm && m_hasOtherHand && !m_twoHanded && !m_braced)
      placeOtherHand(root, otherHand, shift, updateData);

    // Held still, the blade does not move in the hand
    if (held && !m_bladeMeasured)
      measureBlade(weapon, hand);

    m_hasBlade = held && m_bladeMeasured;
    m_bladeAutomatic = form && (readField<uint8_t>(form, VrGame::WeaponFlags) & VrGame::WeaponFlagAutomatic);

    hideArms(root, false);

    uint8_t* muzzle = findObject(weapon, VrGame::ProjectileNodeName);

    if (!muzzle)
      muzzle = findObject(weapon, VrGame::ProjectileNodeAltName);

    // Relative to the root, which the game places at the player. The shots
    // follow the barrel as it is drawn: the game animates the weapon at an
    // angle to its camera, so the barrel does not point along the hand.
    m_muzzleOffset = readVector(muzzle ? muzzle : weapon, VrGame::ObjectWorldTranslation) - drawnRoot;
    m_aimDirection = muzzle
      ? vrComputeBarrelDirection(readRotation(muzzle, VrGame::ObjectWorldRotation))
      : hand.forward;
    m_hasAim       = true;

    if (weapon != m_loggedBarrelWeapon) {
      VrHeadAngles barrelAngles = vrComputeAimAngles(m_aimDirection);
      VrHeadAngles handAngles   = vrComputeAimAngles(hand.forward);

      // Heading difference wrapped into [-pi, pi)
      float right = vrWrapHeading(barrelAngles.yaw - handAngles.yaw + Pi) - Pi;
      float up    = handAngles.pitch - barrelAngles.pitch;

      Logger::info(str::format("VR: Weapon barrel ", right * DegreesPerRadian, " degrees right and ",
        up * DegreesPerRadian, " degrees up of the hand",
        muzzle ? "" : " (no projectile node, the shots follow the hand)"));
      m_loggedBarrelWeapon = weapon;
    }

    // The weapon's model is attached below the node once it is loaded
    uint32_t objects = vrCountWeaponObjects(weapon);

    if (form != m_loggedPartsForm || objects != m_loggedPartsObjects) {
      vrLogWeaponParts(weapon, form);
      m_loggedPartsForm    = form;
      m_loggedPartsObjects = objects;
    }

    if (!m_loggedActive) {
      Logger::info("VR: The weapon is in the right hand");
      m_loggedActive = true;
    }
  }


  void VrWeaponHand::placePipBoyArm(uint8_t* root, const VrVector3& shift, void* updateData) {
    // The game raises the forearm in front of its camera, which follows the
    // head: the forearm follows the left controller instead, and the
    // Pip-Boy with it
    uint8_t* otherHand = findObject(root, VrGame::LeftHandNodeName);

    if (otherHand && m_hasOtherHand && field<uint8_t*>(otherHand, VrGame::ObjectParent)) {
      placeOtherHand(root, otherHand, shift, updateData);
      m_pipBoyShift  = shift;
      m_pipBoyPlaced = true;

      if (!m_loggedPipBoy) {
        Logger::info("VR: The Pip-Boy is on the left forearm, which follows the left controller");
        m_loggedPipBoy = true;
      }
    }

    // The game animates the right hand in front of its camera, so it is
    // hidden with the weapon until it has a pose of its own
    hideArms(root, true);
  }


  VrWeaponHand::PipBoyNodes VrWeaponHand::findPipBoy(uint8_t* root) {
    PipBoyNodes nodes;
    nodes.hand    = findObject(root, VrGame::LeftHandNodeName);
    nodes.forearm = nodes.hand ? field<uint8_t*>(nodes.hand, VrGame::ObjectParent) : nullptr;
    nodes.twist   = nodes.forearm ? findObject(nodes.forearm, VrGame::LeftForeTwistNodeName) : nullptr;
    nodes.screen  = nodes.twist ? findObject(nodes.twist, VrGame::PipBoyScreenNodeName) : nullptr;

    if (nodes.twist && !nodes.screen)
      nodes.screen = findObject(nodes.twist, VrGame::PipBoyScreenNodeAltName);

    if (!nodes.screen || field<uint8_t*>(nodes.twist, VrGame::ObjectParent) != nodes.forearm) {
      nodes.screen = nullptr;
      return nodes;
    }

    // The model's root is the screen's ancestor attached to the twist bone
    uint8_t* device = nodes.screen;

    while (device && field<uint8_t*>(device, VrGame::ObjectParent) != nodes.twist)
      device = field<uint8_t*>(device, VrGame::ObjectParent);

    nodes.device = device;
    return nodes;
  }


  void VrWeaponHand::measurePipBoyScreen(const PipBoyNodes& nodes) {
    // Offsets between the bones and from the twist bone to the screen do
    // not depend on the pose the game animates, only the rotations that
    // the free pose replaces do
    if (!nodes.screen) {
      if (!m_loggedScreen)
        Logger::warn("VR: The Pip-Boy screen was not found on the left forearm, looking at it does not open the Pip-Boy");

      m_hasPipBoyScreen = false;
      m_loggedScreen    = true;
      return;
    }

    uint8_t* hand    = nodes.hand;
    uint8_t* forearm = nodes.forearm;
    uint8_t* twist   = nodes.twist;
    uint8_t* screen  = nodes.screen;

    VrGameRotation forearmRotation = readRotation(forearm, VrGame::ObjectWorldRotation);
    VrVector3 forearmPosition      = readVector(forearm, VrGame::ObjectWorldTranslation);
    VrGameRotation twistRotation   = readRotation(twist, VrGame::ObjectWorldRotation);
    VrVector3 twistPosition        = readVector(twist, VrGame::ObjectWorldTranslation);

    // The screen geometry's origin need not be its centre, its bound's is
    VrVector3 centre = readVector(screen, VrGame::ObjectWorldTranslation);

    if (auto bound = field<const uint8_t*>(screen, VrGame::ObjectWorldBound))
      centre = readVector(bound, 0);

    VrVector3 wristOffset = vrTranspose(forearmRotation) * (readVector(hand, VrGame::ObjectWorldTranslation) - forearmPosition);
    VrVector3 twistOffset = vrTranspose(forearmRotation) * (twistPosition - forearmPosition);
    VrVector3 point       = vrTranspose(twistRotation) * (centre - twistPosition);

    m_pipBoyScreen = vrComputeForearmPoint(FreeHandPose, FreeHandWrist, FreeHandTwist,
      wristOffset, twistOffset, point) * (1.0f / VrGameUnitsPerMetre);
    m_hasPipBoyScreen = true;

    if (!m_loggedScreen) {
      Logger::info(str::format("VR: The Pip-Boy screen is ", m_pipBoyScreen.x, " m forward, ",
        m_pipBoyScreen.y, " m up and ", m_pipBoyScreen.z, " m right of the left controller"));
      m_loggedScreen = true;
    }
  }


  void VrWeaponHand::scalePipBoy(const PipBoyNodes& nodes) {
    uint8_t* device = nodes.device;

    if (!device)
      return;

    VrNodeScale current = {
      readVector(device, VrGame::ObjectLocalTranslation),
      field<float>(device, VrGame::ObjectLocalScale) };

    // A model the game attached or placed again is at its own size
    bool replaced = device != m_pipBoyDevice
      || std::memcmp(&current, &m_pipBoyWrittenPlacement, sizeof(current));

    if (replaced) {
      m_pipBoyDevice           = device;
      m_pipBoyOwnPlacement     = current;
      m_pipBoyWrittenPlacement = current;
      m_pipBoyAppliedScale     = 1.0f;
    }

    if (m_pipBoyScale == m_pipBoyAppliedScale)
      return;

    VrNodeScale placement = m_pipBoyOwnPlacement;

    if (m_pipBoyScale != 1.0f) {
      // The screen's centre in the twist bone's frame at the model's own
      // placement, and the forearm's axis there: the line from the forearm
      // bone to the wrist
      ParentFrame twist = parentFrame(device);

      ParentFrame model;
      model.rotate    = readRotation(device, VrGame::ObjectWorldRotation);
      model.translate = readVector(device, VrGame::ObjectWorldTranslation);
      model.scale     = field<float>(device, VrGame::ObjectWorldScale);

      VrVector3 centre = readVector(nodes.screen, VrGame::ObjectWorldTranslation);

      if (auto bound = field<const uint8_t*>(nodes.screen, VrGame::ObjectWorldBound))
        centre = readVector(bound, 0);

      VrVector3 screen = readRotation(device, VrGame::ObjectLocalRotation)
        * (model.toLocal(centre) * m_pipBoyOwnPlacement.scale) + m_pipBoyOwnPlacement.translate;

      VrVector3 pivot = vrClosestPointOnLine(
        twist.toLocal(readVector(nodes.forearm, VrGame::ObjectWorldTranslation)),
        twist.toLocal(readVector(nodes.hand, VrGame::ObjectWorldTranslation)),
        screen);

      placement = vrScaleAboutPoint(m_pipBoyOwnPlacement, pivot, m_pipBoyScale);
    }

    writeVector(device, VrGame::ObjectLocalTranslation, placement.translate);
    field<float>(device, VrGame::ObjectLocalScale) = placement.scale;

    m_pipBoyWrittenPlacement = placement;
    m_pipBoyAppliedScale     = m_pipBoyScale;

    if (!m_loggedPipBoyScale && m_pipBoyScale != 1.0f) {
      Logger::info(str::format("VR: The Pip-Boy grows while it is open, up to ", m_pipBoyScale, " times its size"));
      m_loggedPipBoyScale = true;
    }
  }


  void VrWeaponHand::updateGrip(GripKind kind, bool canHold, const VrVector3& support) {
    if (!canHold || !m_hasOtherHand || !m_grip || kind == GripKind::None) {
      m_twoHanded = false;
      m_braced    = false;
      return;
    }

    // The grip takes the weapon when the controller comes near where the
    // game holds it, and keeps it until it is released. The game only
    // holds a one-handed weapon with both hands while it aims, so there
    // the other controller only has to come near the hand.
    if (kind == GripKind::TwoHanded || kind == GripKind::TwoHandedMelee) {
      m_braced = false;

      if (!m_twoHanded)
        m_twoHanded = vrLength(m_hand.position + support - m_otherHand.position) <= TwoHandedGripDistance;
    } else {
      m_twoHanded = false;

      if (!m_braced) {
        m_braced  = vrLength(m_hand.position - m_otherHand.position) <= BracedGripDistance;
        m_snapAim = m_braced;
      }
    }

    if (m_twoHanded && kind == GripKind::TwoHanded && !m_loggedTwoHanded) {
      Logger::info("VR: The weapon is held with both hands");
      m_loggedTwoHanded = true;
    }

    if (m_twoHanded && kind == GripKind::TwoHandedMelee && !m_loggedTwoHandedMelee) {
      Logger::info("VR: The melee weapon is held with both hands, the right hand swings it");
      m_loggedTwoHandedMelee = true;
    }

    if (m_braced && !m_loggedBraced) {
      Logger::info("VR: The left hand supports the one-handed weapon, the game aims");
      m_loggedBraced = true;
    }
  }


  void VrWeaponHand::placeOtherHand(
          uint8_t*              root,
          uint8_t*              otherHand,
    const VrVector3&            shift,
          void*                 updateData) {
    // The game animates the free hand anywhere, so its pose is not kept:
    // it takes a fixed pose around its controller. The forearm carries the
    // hand, and the hand's meshes only use bones below the forearm.
    uint8_t* forearm = field<uint8_t*>(otherHand, VrGame::ObjectParent);

    VrGameRotation forearmRotation = readRotation(forearm, VrGame::ObjectWorldRotation);
    VrVector3 forearmPosition      = readVector(forearm, VrGame::ObjectWorldTranslation);
    VrVector3 handPosition         = readVector(otherHand, VrGame::ObjectWorldTranslation);

    VrGameCameraPose controller = m_otherHand;
    controller.position = controller.position - shift;

    VrGameTransform target = vrComputeHeldTransform(controller, FreeHandPose);

    // The forearm is placed so that the hand, at its fixed wrist rotation,
    // lands on the target. The hand's offset in the forearm, scale
    // included, is the animation's: it is the length of the forearm.
    VrVector3 wrist = vrTranspose(forearmRotation) * (handPosition - forearmPosition);

    VrGameRotation worldRotation = target.rotate * vrTranspose(FreeHandWrist);
    VrVector3 worldTranslation   = target.translate - worldRotation * wrist;

    ParentFrame parent = parentFrame(forearm);

    VrGameRotation rotation = readRotation(forearm, VrGame::ObjectLocalRotation);
    VrVector3 translation   = readVector(forearm, VrGame::NodeLocalTranslate);

    writeRotation(forearm, VrGame::ObjectLocalRotation, vrTranspose(parent.rotate) * worldRotation);
    writeVector(forearm, VrGame::NodeLocalTranslate, parent.toLocal(worldTranslation));

    // The wrist, the twist bone and the fingers take the pose too
    struct SavedBone {
      uint8_t*       bone;
      VrGameRotation rotate;
    };

    std::array<SavedBone, std::size(FreeHandBones)> saved;
    uint32_t savedCount = 0;

    forEachObject(forearm, MaxModelDepth, [&saved, &savedCount] (uint8_t* object) {
      auto name = field<const char*>(object, VrGame::ObjectName);

      if (!name || savedCount == saved.size())
        return;

      for (const FreeHandBone& pose : FreeHandBones) {
        if (std::strcmp(name, pose.name))
          continue;

        saved[savedCount++] = { object, readRotation(object, VrGame::ObjectLocalRotation) };
        writeRotation(object, VrGame::ObjectLocalRotation, pose.rotate);
        return;
      }
    });

    updateObject(forearm, updateData);

    // The next eye's update starts from the game's pose again
    writeRotation(forearm, VrGame::ObjectLocalRotation, rotation);
    writeVector(forearm, VrGame::NodeLocalTranslate, translation);

    for (uint32_t i = 0; i < savedCount; i++)
      writeRotation(saved[i].bone, VrGame::ObjectLocalRotation, saved[i].rotate);

    // A skinned mesh computes its bound from its bones when it is updated
    // itself, and the hand's meshes are not below the forearm: without this,
    // they are culled where the game animated the hand
    forEachObject(root, MaxModelDepth, [forearm, updateData] (uint8_t* object) {
      uint8_t* geometry = asGeometry(object);

      if (geometry && isSkinnedBelow(geometry, forearm))
        updateObject(geometry, updateData);
    });

    if (!m_loggedOtherHand) {
      auto name = field<const char*>(forearm, VrGame::ObjectName);
      Logger::info(str::format("VR: The left hand takes its free pose around its controller, moved by '",
        name ? name : "", "'"));
      m_loggedOtherHand = true;
    }
  }


  void VrWeaponHand::hideArms(uint8_t* root, bool rightHand) {
    uint8_t* hands[2] = {
      findObject(root, VrGame::LeftHandNodeName),
      findObject(root, VrGame::RightHandNodeName) };

    if (!hands[0] || !hands[1]) {
      if (!m_loggedArms)
        Logger::warn("VR: The hand bones were not found, the arms stay visible");

      m_loggedArms = true;
      return;
    }

    // The right hand and the weapon are shown again when they no longer
    // have to be hidden
    if (rightHand != m_hidesRightHand)
      showArms(root);

    m_hidesRightHand = rightHand;

    UpperArm arms[2];

    for (uint32_t i = 0; i < 2; i++) {
      arms[i].top     = armBone(hands[i], hands[1 - i]);
      arms[i].forearm = arms[i].top != hands[i] ? field<uint8_t*>(hands[i], VrGame::ObjectParent) : hands[i];

      if (!m_loggedArms) {
        auto top     = field<const char*>(arms[i].top, VrGame::ObjectName);
        auto forearm = field<const char*>(arms[i].forearm, VrGame::ObjectName);
        Logger::info(str::format("VR: Hiding the meshes skinned between '", top ? top : "",
          "' and '", forearm ? forearm : "", "'"));
      }
    }

    // A mesh shared by both hands stays, so that the left hand is drawn
    auto isRightHandMesh = [&arms, rightHand] (uint8_t* geometry) {
      return rightHand && isSkinnedBelow(geometry, arms[1].forearm)
        && !isSkinnedBelow(geometry, arms[0].forearm);
    };

    // The meshes are culled with the first-person camera after this update
    forEachObject(root, MaxModelDepth, [this, &arms, &isRightHandMesh] (uint8_t* object) {
      uint8_t* geometry = asGeometry(object);

      if (!geometry)
        return;

      if (isArmMesh(geometry, arms))
        hide(geometry, !m_loggedArms);
      else if (isRightHandMesh(geometry))
        hide(geometry, !m_loggedRightHand);
    });

    if (rightHand) {
      if (uint8_t* weapon = findObject(root, VrGame::WeaponNodeName))
        hide(weapon, !m_loggedRightHand);

      m_loggedRightHand = true;
    }

    if (!m_loggedArms && m_hiddenObjects.empty())
      Logger::warn("VR: No arm mesh was found, the arms stay visible");

    m_loggedArms = true;
  }


  void VrWeaponHand::hideModel(uint8_t* root) {
    // Culling the root culls the whole model. The objects culled one by
    // one are shown again first, so that they are not left culled when the
    // model is shown again.
    if (!m_hidesModel)
      showArms(root);

    hide(root, false);
    m_hidesModel = true;

    if (!m_loggedHidden) {
      Logger::info("VR: The arms and the weapon are hidden while the lasers point at a menu");
      m_loggedHidden = true;
    }
  }


  void VrWeaponHand::hide(uint8_t* object, bool log) {
    uint32_t& flags = field<uint32_t>(object, VrGame::ObjectFlags);
    bool hidden = std::find(m_hiddenObjects.begin(), m_hiddenObjects.end(), object) != m_hiddenObjects.end();

    // An object the game culls itself is left to the game, so that it is
    // not shown again when the VR layer lets go of it
    if (!hidden && (flags & VrGame::ObjectFlagAppCulled))
      return;

    flags |= VrGame::ObjectFlagAppCulled;

    if (hidden)
      return;

    m_hiddenObjects.push_back(object);

    if (log) {
      auto name = field<const char*>(object, VrGame::ObjectName);
      Logger::info(str::format("VR: Hiding '", name ? name : "", "'"));
    }
  }


  void VrWeaponHand::showArms(uint8_t* root) {
    m_hidesRightHand = false;
    m_hidesModel     = false;

    if (m_hiddenObjects.empty())
      return;

    // Objects no longer in the model may have been freed, so only the ones
    // still found in it are touched
    forEachObject(root, MaxModelDepth, [this] (uint8_t* object) {
      if (std::find(m_hiddenObjects.begin(), m_hiddenObjects.end(), object) != m_hiddenObjects.end())
        field<uint32_t>(object, VrGame::ObjectFlags) &= ~VrGame::ObjectFlagAppCulled;
    });

    m_hiddenObjects.clear();
  }


  bool VrWeaponHand::holdsWeapon(const uint8_t* player) const {
    if (readField<uint8_t>(player, VrGame::PlayerThirdPerson))
      return false;

    if (*reinterpret_cast<const uint32_t*>(VrGame::VatsCameraState))
      return false;

    auto process = readField<uint8_t*>(player, VrGame::ActorProcess);

    if (!process)
      return false;

    auto vtable = readField<const uint8_t*>(process, 0);
    auto isWeaponOut = reinterpret_cast<IsWeaponOutFn>(readField<uintptr_t>(vtable, VrGame::ProcessIsWeaponOutSlot));
    return (isWeaponOut(process, nullptr) & 0xFF) != 0;
  }


  void VrWeaponHand::measureBlade(const uint8_t* weapon, const VrGameCameraPose& hand) {
    auto bound = readField<const uint8_t*>(weapon, VrGame::ObjectWorldBound);

    // The hand and the weapon's bound are both where the model is drawn
    m_bladeMeasured = bound
      && vrMeasureBlade(hand, readVector(bound, 0), readField<float>(bound, sizeof(VrVector3)), m_blade);

    if (m_bladeMeasured) {
      Logger::info(str::format("VR: Melee weapon blade ", vrLength(m_blade.tip - m_blade.base) / VrGameUnitsPerMetre,
        " m long, tip at ", m_blade.tip.x, ", ", m_blade.tip.y, ", ", m_blade.tip.z, " units in the hand's frame"));
    }
  }


  const uint8_t* VrWeaponHand::weaponForm(const uint8_t* player) const {
    auto process = readField<uint8_t*>(player, VrGame::ActorProcess);

    if (!process)
      return nullptr;

    auto vtable = readField<const uint8_t*>(process, 0);
    auto getWeaponInfo = reinterpret_cast<GetWeaponInfoFn>(readField<uintptr_t>(vtable, VrGame::ProcessWeaponInfoSlot));
    auto entry = getWeaponInfo(process, nullptr);

    return entry ? field<uint8_t*>(entry, VrGame::InventoryEntryForm) : nullptr;
  }


  int8_t VrWeaponHand::weaponType(const uint8_t* player) const {
    const uint8_t* weapon = weaponForm(player);
    return weapon ? readField<int8_t>(weapon, VrGame::WeaponAnimationType) : WeaponTypeNone;
  }


  bool VrWeaponHand::isMelee(const uint8_t* weapon) {
    int8_t type = weapon ? readField<int8_t>(weapon, VrGame::WeaponAnimationType) : WeaponTypeNone;
    return type == WeaponTypeOneHandMelee || type == WeaponTypeTwoHandMelee;
  }


  VrWeaponHand::GripKind VrWeaponHand::weaponGrip(const uint8_t* player) const {
    // One-handed melee, thrown and placed weapons are left to the game's
    // animation
    int8_t type = weaponType(player);

    if (type == WeaponTypeTwoHandMelee)
      return GripKind::TwoHandedMelee;

    if (type == WeaponTypePistol || type == WeaponTypePistolEnergy)
      return GripKind::Braced;

    if (type >= WeaponTypeRifle && type <= WeaponTypeLauncher)
      return GripKind::TwoHanded;

    return GripKind::None;
  }

}
