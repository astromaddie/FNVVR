#include <cfloat>
#include <cstring>

#include "../../util/log/log.h"
#include "../../util/util_string.h"

#include "vr_game_addresses.h"
#include "vr_game_memory.h"
#include "vr_game_nodes.h"
#include "vr_game_sound.h"
#include "vr_sound_keys.h"
#include "vr_terminal_keyboard_model.h"

namespace dxvk {

  namespace {

    using LoadFileFn       = void* (__thiscall*)(void* loader, const char* path, uint32_t lodFade,
      uint32_t assignShaders, uint32_t unknown, uint32_t keepUv, uint32_t noUseCount);
    using CloneFn          = void* (__thiscall*)(void* object, void* process);
    using CloningInitFn    = void  (__thiscall*)(void* process, float scale);
    using CloningFreeFn    = void  (__thiscall*)(void* process);
    using GetControllerFn  = uint8_t* (__thiscall*)(void* object, uintptr_t rtti);
    using ActivateFn       = bool  (__thiscall*)(void* manager, void* sequence, int32_t priority,
      uint32_t startOver, float weight, float easeIn, void* timeSync);
    using DeactivateFn     = bool  (__thiscall*)(void* manager, void* sequence, float easeOut);
    using DeactivateAllFn  = void  (__thiscall*)(void* manager, float easeOut);
    using StartFn          = void  (__cdecl*)(void* object);
    using UpdateFn         = void  (__thiscall*)(void* object, void* data);
    using AttachChildFn    = void  (__thiscall*)(void* node, void* child, uint32_t firstFree);
    using DetachChildFn    = void  (__thiscall*)(void* node, void* child);
    using SetRangeFn       = void  (__thiscall*)(void* node, float nearDistance, float farDistance);
    using MethodFn         = void  (__thiscall*)(void* object);
    using SceneObjectFn    = void  (__thiscall*)(void* shadowSceneNode, void* object);
    using GetPropertyFn    = uint8_t* (__thiscall*)(void* object, uint32_t type);
    using AssignFn         = void* (__thiscall*)(void* pointer, void* object);

    constexpr char ModelPath[]    = "Meshes\\Terminals\\Terminal01.nif";
    constexpr char MonitorNode[]  = "T01Screen01";
    constexpr char KeyboardNode[] = "T01Keyboard01";
    constexpr char PlateShape[]   = "T01Keyboard01:0";
    constexpr char OpenName[]     = "Open";
    constexpr char CloseName[]    = "Close";

    // The parts of the world terminal the console does not show: its
    // monitor and glass, its base and the texts scrolling on its screen
    constexpr const char* HiddenParts[] = {
      "T01Screen01:0", "T01Screen01:1", "Terminal01base:0", "Text01", "Text02", "Text03",
    };

    // The model is a few levels deep
    constexpr uint32_t MaxDepth = 8;

    // As FORenderedTerminal::OnMenuOpen sets its monitor's fade node
    constexpr float FadeDistance = 100000.0f;

    template<typename T>
    T& field(void* object, uintptr_t offset) {
      return *reinterpret_cast<T*>(static_cast<uint8_t*>(object) + offset);
    }

    template<typename Fn>
    Fn method(void* object, uintptr_t slot) {
      return *reinterpret_cast<Fn*>(field<uint8_t*>(object, 0) + slot);
    }

    template<typename Fn>
    Fn game(uintptr_t address) {
      return reinterpret_cast<Fn>(address);
    }

    uint8_t* findObject(void* root, const char* name) {
      return reinterpret_cast<uint8_t*>(vrFindGameObject(reinterpret_cast<uintptr_t>(root), name, MaxDepth));
    }

    VrGameScaledTransform readTransform(void* object, uintptr_t rotation, uintptr_t translation, uintptr_t scale) {
      VrGameScaledTransform result;
      std::memcpy(result.rotate.m, static_cast<uint8_t*>(object) + rotation, sizeof(result.rotate.m));
      std::memcpy(&result.translate, static_cast<uint8_t*>(object) + translation, sizeof(result.translate));
      result.scale = field<float>(object, scale);
      return result;
    }

    void* menuShadowSceneNode() {
      return reinterpret_cast<void* const*>(VrGame::ShadowSceneNodes)[VrGame::MenuShadowSceneNode];
    }

    uint8_t* findSequence(uint8_t* manager, const char* name) {
      auto sequences = field<uint8_t**>(manager, VrGame::ControllerManagerSequences);
      auto count     = field<uint16_t>(manager, VrGame::ControllerManagerSequenceCount);

      for (uint32_t i = 0; sequences && i < count; i++) {
        const char* sequenceName = sequences[i] ? field<const char*>(sequences[i], VrGame::SequenceName) : nullptr;

        if (sequenceName && !std::strcmp(sequenceName, name))
          return sequences[i];
      }

      return nullptr;
    }

    bool supported() {
      struct Bytes {
        uintptr_t      address;
        const uint8_t* bytes;
        size_t         size;
      };

      const Bytes checks[] = {
        { VrGame::ModelLoaderLoadFile,         VrGame::ModelLoaderLoadFilePrologue,         sizeof(VrGame::ModelLoaderLoadFilePrologue) },
        { VrGame::ModelLoaderSingletonUseSite, VrGame::ModelLoaderSingletonUse,             sizeof(VrGame::ModelLoaderSingletonUse) },
        { VrGame::ObjectClone,                 VrGame::ObjectClonePrologue,                 sizeof(VrGame::ObjectClonePrologue) },
        { VrGame::CloningProcessConstruct,     VrGame::CloningProcessConstructPrologue,     sizeof(VrGame::CloningProcessConstructPrologue) },
        { VrGame::CloningProcessDestruct,      VrGame::CloningProcessDestructPrologue,      sizeof(VrGame::CloningProcessDestructPrologue) },
        { VrGame::ObjectGetController,         VrGame::ObjectGetControllerPrologue,         sizeof(VrGame::ObjectGetControllerPrologue) },
        { VrGame::ControllerManagerRttiUseSite, VrGame::ControllerManagerRttiUse,           sizeof(VrGame::ControllerManagerRttiUse) },
        { VrGame::ActivateSequence,            VrGame::ActivateSequencePrologue,            sizeof(VrGame::ActivateSequencePrologue) },
        { VrGame::DeactivateSequence,          VrGame::DeactivateSequencePrologue,          sizeof(VrGame::DeactivateSequencePrologue) },
        { VrGame::DeactivateAllSequences,      VrGame::DeactivateAllSequencesPrologue,      sizeof(VrGame::DeactivateAllSequencesPrologue) },
        { VrGame::StartAnimations,             VrGame::StartAnimationsPrologue,             sizeof(VrGame::StartAnimationsPrologue) },
        { VrGame::DataHandlerGetSound,         VrGame::DataHandlerGetSoundPrologue,         sizeof(VrGame::DataHandlerGetSoundPrologue) },
        { VrGame::FadeNodeSetRange,            VrGame::FadeNodeSetRangePrologue,            sizeof(VrGame::FadeNodeSetRangePrologue) },
        { VrGame::FadeNodeTurnOff,             VrGame::FadeNodeTurnOffPrologue,             sizeof(VrGame::FadeNodeTurnOffPrologue) },
        { VrGame::ShadowSceneNodeAddObject,    VrGame::ShadowSceneNodeAddObjectPrologue,    sizeof(VrGame::ShadowSceneNodeAddObjectPrologue) },
        { VrGame::ShadowSceneNodeRemoveObject, VrGame::ShadowSceneNodeRemoveObjectPrologue, sizeof(VrGame::ShadowSceneNodeRemoveObjectPrologue) },
        { VrGame::ObjectGetProperty,           VrGame::ObjectGetPropertyPrologue,           sizeof(VrGame::ObjectGetPropertyPrologue) },
      };

      for (const auto& check : checks) {
        if (!VrGameMemory::matches(check.address, check.bytes, check.size))
          return false;
      }

      return VrGameMemory::readable(VrGame::ModelLoaderSingleton, sizeof(uintptr_t))
          && VrGameMemory::readable(VrGame::DataHandler, sizeof(uintptr_t))
          && VrGameMemory::readable(VrGame::AudioSingleton, sizeof(uintptr_t))
          && VrGameMemory::readable(VrGame::ShadowSceneNodes, (VrGame::MenuShadowSceneNode + 1) * sizeof(uintptr_t));
    }

  }


  std::unique_ptr<VrTerminalKeyboardModel> VrTerminalKeyboardModel::create() {
    if (!supported()) {
      Logger::warn("VR: The game's model functions were not found, the console's keyboard is flat");
      return nullptr;
    }

    return std::unique_ptr<VrTerminalKeyboardModel>(new VrTerminalKeyboardModel());
  }


  VrTerminalKeyboardModel::~VrTerminalKeyboardModel() {
    // The model is left to the game, which may be shutting down
    endDraw();
  }


  bool VrTerminalKeyboardModel::attach(void* monitor) {
    if (m_failed || !monitor)
      return false;

    if (m_state != State::Detached)
      detach();

    if (!m_root && !load(monitor)) {
      Logger::err("VR: Failed to read the terminal's keyboard model, the console's keyboard is flat");
      m_failed = true;
      return false;
    }

    m_monitor = monitor;
    method<AttachChildFn>(monitor, VrGame::NodeAttachChildSlot)(monitor, m_root, 1);

    play(m_open, m_close);
    m_state = State::Opening;
    update(0.0f);

    // Lit by the lights of the menu scene, as the monitor
    if (void* scene = menuShadowSceneNode())
      game<SceneObjectFn>(VrGame::ShadowSceneNodeAddObject)(scene, m_root);

    return true;
  }


  void VrTerminalKeyboardModel::close() {
    if (m_state != State::Opening && m_state != State::Open)
      return;

    play(m_close, m_open);
    m_state = State::Closing;
  }


  void VrTerminalKeyboardModel::detach() {
    if (m_state == State::Detached)
      return;

    endDraw();

    if (void* scene = menuShadowSceneNode())
      game<SceneObjectFn>(VrGame::ShadowSceneNodeRemoveObject)(scene, m_root);

    method<DetachChildFn>(m_monitor, VrGame::NodeDetachChildSlot)(m_monitor, m_root);

    m_monitor = nullptr;
    m_state   = State::Detached;
  }


  void VrTerminalKeyboardModel::update(float dt) {
    if (m_state == State::Detached)
      return;

    // The sequence starts at the first update after it was played; only
    // the time since then matters
    m_time += dt;

    uint8_t data[VrGame::UpdateDataSize] = { };
    std::memcpy(data, &m_time, sizeof(m_time));
    data[sizeof(m_time)] = 1;

    game<UpdateFn>(VrGame::ObjectUpdate)(m_root, data);
    playSounds();

    if (!sequenceEnded())
      return;

    if (m_state == State::Opening)
      m_state = State::Open;
    else if (m_state == State::Closing)
      detach();
  }


  void VrTerminalKeyboardModel::playSounds() {
    // The sounds of the model's sequences are 3D sounds of the world's
    // terminals; the game would play them at the world's origin, out of
    // hearing, as it has no reference here. The keyboard is in front of
    // the head, so they play as the interface's sounds do
    // (InterfaceManager::PlayMenuSound): not driven by an animation, as
    // those are paused while a menu is open.
    float previous = field<float>(m_sequence, VrGame::SequenceStartTime);
    float current  = field<float>(m_sequence, VrGame::SequenceLastScaledTime);

    if (previous == -FLT_MAX || current == -FLT_MAX || previous == current)
      return;

    void* textKeys = field<void*>(m_sequence, VrGame::SequenceTextKeys);
    uint8_t* keys  = textKeys ? field<uint8_t*>(textKeys, VrGame::TextKeyArray) : nullptr;
    uint32_t count = textKeys ? field<uint32_t>(textKeys, VrGame::TextKeyCount) : 0;

    for (uint32_t i = 0; keys && i < count; i++) {
      uint8_t* key = keys + i * VrGame::TextKeySize;

      if (!vrSoundKeyCrossed(previous, current, field<float>(key, 0)))
        continue;

      std::string editorId = vrSoundKeyEditorId(field<const char*>(key, VrGame::TextKeyText));

      if (editorId.empty())
        continue;

      const uint8_t* sound = vrFindGameSound(editorId);

      if (!sound) {
        Logger::warn(str::format("VR: The terminal's keyboard sound '", editorId, "' was not found"));
        continue;
      }

      vrPlayGameSound(sound, VrGame::SoundFlag2D | VrGame::SoundFlagSystem | VrGame::SoundFlagOneShot,
        VrVector3(), 1.0f, 1.0f);
    }
  }


  VrGameScaledTransform VrTerminalKeyboardModel::keyboardTransform() const {
    return readTransform(m_keyboard, VrGame::ObjectWorldRotation,
      VrGame::ObjectWorldTranslation, VrGame::ObjectWorldScale);
  }


  void VrTerminalKeyboardModel::beginDraw(IDirect3DBaseTexture9* texture) {
    endDraw();

    if (m_state == State::Detached || !texture)
      return;

    // The base map of the plate's shader property; the bracket's is the
    // same texture object
    uint8_t* property = game<GetPropertyFn>(VrGame::ObjectGetProperty)(m_plate, VrGame::ShadePropertyType);
    auto     textures = property ? field<uint8_t**>(property, VrGame::ShaderPropertyTextures) : nullptr;
    uint8_t* base     = textures ? field<uint8_t*>(textures, 0) : nullptr;
    uint8_t* data     = base ? field<uint8_t*>(base, VrGame::TextureRendererData) : nullptr;

    // The game creates the Direct3D texture when it first draws the model
    if (!data)
      return;

    m_textureSlot  = &field<void*>(data, VrGame::TextureDataD3DTexture);
    m_savedTexture = *m_textureSlot;
    *m_textureSlot = texture;
  }


  void VrTerminalKeyboardModel::endDraw() {
    if (!m_textureSlot)
      return;

    *m_textureSlot = m_savedTexture;
    m_textureSlot  = nullptr;
    m_savedTexture = nullptr;
  }


  bool VrTerminalKeyboardModel::load(void* monitor) {
    void* loader = *reinterpret_cast<void* const*>(VrGame::ModelLoaderSingleton);
    void* cached = loader ? game<LoadFileFn>(VrGame::ModelLoaderLoadFile)(loader, ModelPath, 0, 1, 0, 0, 0) : nullptr;

    if (!cached)
      return false;

    // A copy, since the terminals of the world may draw the cached model
    alignas(4) uint8_t process[VrGame::CloningProcessSize];
    game<CloningInitFn>(VrGame::CloningProcessConstruct)(process, 1.0f);
    void* copy = game<CloneFn>(VrGame::ObjectClone)(cached, process);
    game<CloningFreeFn>(VrGame::CloningProcessDestruct)(process);

    if (!copy)
      return false;

    game<AssignFn>(VrGame::NiPointerAssign)(&m_root, copy);

    // The copy's collision body never joins a physics world
    vrVisitGameObjects(reinterpret_cast<uintptr_t>(m_root), MaxDepth, [] (uintptr_t object, uint32_t) {
      void** collision = &field<void*>(reinterpret_cast<void*>(object), VrGame::ObjectCollision);

      if (*collision)
        game<AssignFn>(VrGame::NiPointerAssign)(collision, nullptr);
    });

    for (const char* part : HiddenParts) {
      if (uint8_t* object = findObject(m_root, part))
        field<uint32_t>(object, VrGame::ObjectFlags) |= VrGame::ObjectFlagAppCulled;
    }

    // Shown up close like the monitor, a fade node of the same class
    if (field<void*>(m_root, 0) == field<void*>(monitor, 0)) {
      field<uint32_t>(m_root, VrGame::FadeNodeLodMultType) = VrGame::LodMultRenderedMenu;
      game<SetRangeFn>(VrGame::FadeNodeSetRange)(m_root, FadeDistance, FadeDistance);
      game<MethodFn>(VrGame::FadeNodeTurnOff)(m_root);
    }

    m_manager  = game<GetControllerFn>(VrGame::ObjectGetController)(m_root, VrGame::ControllerManagerRtti);
    m_open     = m_manager ? findSequence(m_manager, OpenName)  : nullptr;
    m_close    = m_manager ? findSequence(m_manager, CloseName) : nullptr;
    m_keyboard = findObject(m_root, KeyboardNode);
    m_plate    = findObject(m_root, PlateShape);

    if (!m_open || !m_close || !m_keyboard || !m_plate || !place()) {
      game<AssignFn>(VrGame::NiPointerAssign)(&m_root, nullptr);
      return false;
    }

    // As the game prepares the animated models of its menus
    game<DeactivateAllFn>(VrGame::DeactivateAllSequences)(m_manager, 0.0f);
    game<StartFn>(VrGame::StartAnimations)(m_root);

    Logger::info("VR: The terminal's keyboard model was read");
    return true;
  }


  bool VrTerminalKeyboardModel::place() {
    uint8_t* monitor = findObject(m_root, MonitorNode);

    if (!monitor)
      return false;

    VrGameScaledTransform screen = readTransform(monitor, VrGame::ObjectLocalRotation,
      VrGame::ObjectLocalTranslation, VrGame::ObjectLocalScale);
    VrGameScaledTransform root = VrTerminalKeyboardMount::computeRoot(screen);

    std::memcpy(static_cast<uint8_t*>(m_root) + VrGame::ObjectLocalRotation, root.rotate.m, sizeof(root.rotate.m));
    std::memcpy(static_cast<uint8_t*>(m_root) + VrGame::ObjectLocalTranslation, &root.translate, sizeof(root.translate));
    field<float>(m_root, VrGame::ObjectLocalScale) = root.scale;
    return true;
  }


  void VrTerminalKeyboardModel::play(uint8_t* sequence, uint8_t* previous) {
    game<DeactivateFn>(VrGame::DeactivateSequence)(m_manager, previous, 0.0f);
    game<DeactivateFn>(VrGame::DeactivateSequence)(m_manager, sequence, 0.0f);
    game<ActivateFn>(VrGame::ActivateSequence)(m_manager, sequence, 0, 0, 1.0f, 0.0f, nullptr);

    // Started at the next update, as the game plays a door's or a
    // container's Open and Close
    field<float>(sequence, VrGame::SequenceOffset) = -FLT_MAX;
    field<uint16_t>(m_manager, VrGame::ControllerFlags) |= VrGame::ControllerActive;

    m_sequence = sequence;
  }


  bool VrTerminalKeyboardModel::sequenceEnded() const {
    return field<float>(m_sequence, VrGame::SequenceLastScaledTime)
        >= field<float>(m_sequence, VrGame::SequenceEndKeyTime);
  }

}
