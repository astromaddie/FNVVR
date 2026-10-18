#include <algorithm>
#include <cmath>

#include "../../util/log/log.h"
#include "../../util/util_string.h"

#include "vr_game_addresses.h"
#include "vr_game_memory.h"
#include "vr_part_highlight.h"

namespace dxvk {

  namespace {

    struct GameColor {
      float r, g, b, a;
    };

    using EffectsFn          = void  (__cdecl*)(void* renderer, void* target, void* destination);
    using GetManagerFn       = void* (__cdecl*)();
    using TextureManagerFn   = void* (__cdecl*)();
    using BorrowFn           = void* (__thiscall*)(void* manager, void* renderer, uint32_t type,
      uint32_t a, uint32_t b, uint32_t c);
    using ReturnFn           = void  (__thiscall*)(void* manager, void* texture);
    using AssignFn           = void* (__thiscall*)(void* pointer, void* object);
    using GetGroupFn         = void* (__thiscall*)(void* texture);
    using StartOffscreenFn   = void  (__cdecl*)(uint32_t clear, void* group);
    using StopOffscreenFn    = void  (__cdecl*)();
    using BackgroundFn       = void  (__thiscall*)(void* renderer, GameColor* color);
    using ColorWriteFn       = void  (__cdecl*)(uint32_t mask, uint32_t a);
    using RestoreColorFn     = void  (__cdecl*)(uint32_t a);
    using AccumulatorModeFn  = void  (__thiscall*)(void* accumulator, uint32_t mode);
    using ShaderModeFn       = void  (__cdecl*)(uint32_t mode);
    using SetAccumulatorFn   = void  (__thiscall*)(void* culler, void* accumulator);
    using SetCullModeFn      = void  (__thiscall*)(void* culler, uint32_t mode);
    using AccumulateFn       = void  (__cdecl*)(void* camera, void* node, void* culler);
    using RenderSceneFn      = void  (__cdecl*)(void* camera, void* accumulator, uint32_t a);
    using PrepareFn          = void  (__thiscall*)(void* manager, uint32_t a, void* mask, uint32_t b, uint32_t clear);
    using ShutdownFn         = void  (__cdecl*)();
    using PointerFn          = void* (__cdecl*)();
    using GetEffectFn        = void* (__fastcall*)(void* manager, void* unused, uint32_t id);

    /** Seconds between two lines of the log while parts glow */
    constexpr float MeasureSeconds = 3.0f;

    VrPartHighlight* g_highlight = nullptr;

    template<typename T>
    T& field(void* object, uintptr_t offset) {
      return *reinterpret_cast<T*>(static_cast<uint8_t*>(object) + offset);
    }

    template<typename T>
    T& global(uintptr_t address) {
      return *reinterpret_cast<T*>(address);
    }

    template<typename Fn>
    Fn method(void* object, uintptr_t slot) {
      return *reinterpret_cast<Fn*>(field<uint8_t*>(object, 0) + slot);
    }

    template<typename Fn>
    Fn game(uintptr_t address) {
      return reinterpret_cast<Fn>(address);
    }

    float setting(uintptr_t address) {
      return global<float>(address + VrGame::SettingValue);
    }

    bool supported() {
      struct Prologue {
        uintptr_t      address;
        const uint8_t* bytes;
        size_t         size;
      };

      const Prologue prologues[] = {
        { VrGame::VatsPrepareHighlight,  VrGame::VatsPrepareHighlightPrologue,  sizeof(VrGame::VatsPrepareHighlightPrologue) },
        { VrGame::VatsShutdownEffect,    VrGame::VatsShutdownEffectPrologue,    sizeof(VrGame::VatsShutdownEffectPrologue) },
        { VrGame::AccumulateScene,       VrGame::AccumulateScenePrologue,       sizeof(VrGame::AccumulateScenePrologue) },
        { VrGame::RenderScene,           VrGame::RenderScenePrologue,           sizeof(VrGame::RenderScenePrologue) },
        { VrGame::GetTextureManager,     VrGame::GetTextureManagerBytes,        sizeof(VrGame::GetTextureManagerBytes) },
        { VrGame::BorrowRenderedTexture, VrGame::BorrowRenderedTexturePrologue, sizeof(VrGame::BorrowRenderedTexturePrologue) },
        { VrGame::ReturnRenderedTexture, VrGame::ReturnRenderedTexturePrologue, sizeof(VrGame::ReturnRenderedTexturePrologue) },
        { VrGame::NiPointerAssign,       VrGame::NiPointerAssignPrologue,       sizeof(VrGame::NiPointerAssignPrologue) },
      };

      for (const auto& prologue : prologues) {
        if (!VrGameMemory::matches(prologue.address, prologue.bytes, prologue.size))
          return false;
      }

      return VrGameMemory::readable(VrGame::MainSingleton, sizeof(uintptr_t))
          && VrGameMemory::readable(VrGame::WorldSceneGraph, sizeof(uintptr_t))
          && VrGameMemory::readable(VrGame::VatsMaskTexture, sizeof(uintptr_t))
          && VrGameMemory::readable(VrGame::ShaderManagerCamera, sizeof(uintptr_t))
          && VrGameMemory::readable(VrGame::Lighting30AlwaysWriteAlpha, 1)
          && VrGameMemory::readable(VrGame::ShadowLightAlwaysWriteAlpha, 1)
          && VrGameMemory::readable(VrGame::SettingTargetPulseRateVats + VrGame::SettingValue, sizeof(float));
    }

    /**
     * \brief Draws nodes into the current render target
     *
     * As FOVATSEffectManager::DrawTargetToTexture: the lighting shaders
     * write alpha, the V.A.T.S. manager's accumulator and the shader
     * manager draw in the constant alpha mode, and the world's culling
     * process passes every node to the accumulator.
     */
    void drawNodes(void* camera, void* culler, void* accumulator, uint8_t* const* nodes, size_t count) {
      global<uint8_t>(VrGame::Lighting30AlwaysWriteAlpha)  = 1;
      global<uint8_t>(VrGame::ShadowLightAlwaysWriteAlpha) = 1;

      game<AccumulatorModeFn>(VrGame::AccumulatorSetRenderMode)(accumulator, VrGame::RenderModeConstAlpha);
      game<ShaderModeFn>(VrGame::ShaderManagerSetRenderMode)(VrGame::RenderModeConstAlpha);

      game<SetAccumulatorFn>(VrGame::CullingSetAccumulator)(culler, accumulator);
      game<SetCullModeFn>(VrGame::CullingSetCullMode)(culler, VrGame::CullModeAllPass);

      for (size_t i = 0; i < count; i++)
        game<AccumulateFn>(VrGame::AccumulateScene)(camera, nodes[i], culler);

      game<RenderSceneFn>(VrGame::RenderScene)(camera, accumulator, 0);

      game<SetAccumulatorFn>(VrGame::CullingSetAccumulator)(culler, nullptr);
      game<SetCullModeFn>(VrGame::CullingSetCullMode)(culler, VrGame::CullModeNormal);

      game<ShaderModeFn>(VrGame::ShaderManagerSetRenderMode)(VrGame::RenderModeNormal);
      game<AccumulatorModeFn>(VrGame::AccumulatorSetRenderMode)(accumulator, VrGame::RenderModeNormal);

      global<uint8_t>(VrGame::Lighting30AlwaysWriteAlpha)  = 0;
      global<uint8_t>(VrGame::ShadowLightAlwaysWriteAlpha) = 0;
    }

  }


  VrPartHighlight::VrPartHighlight(IDirect3DDevice9* device, TakeFn take)
  : m_take(std::move(take)), m_start(std::chrono::steady_clock::now()), m_measureStart(m_start) {
    if (FAILED(device->CreateQuery(D3DQUERYTYPE_OCCLUSION, &m_modelQuery))
     || FAILED(device->CreateQuery(D3DQUERYTYPE_OCCLUSION, &m_partQuery))) {
      m_modelQuery = nullptr;
      m_partQuery  = nullptr;
      Logger::warn("VR: No occlusion query for the highlight of the weapon's parts, its masks are not measured");
    }
  }


  VrPartHighlight::~VrPartHighlight() {
    // The redirected call stays in place and falls through to the game's function
    g_highlight = nullptr;
  }


  std::unique_ptr<VrPartHighlight> VrPartHighlight::install(IDirect3DDevice9* device, TakeFn take) {
    if (g_highlight || !device || !supported())
      return nullptr;

    if (!VrGameMemory::redirectCall(VrGame::ImageSpaceEffectsCallSite, VrGame::RenderImageSpaceEffects,
        reinterpret_cast<const void*>(&VrPartHighlight::imageSpaceEffectsHook)))
      return nullptr;

    std::unique_ptr<VrPartHighlight> highlight(new VrPartHighlight(device, std::move(take)));
    g_highlight = highlight.get();
    return highlight;
  }


  void VrPartHighlight::drawMask(void* renderer, void* manager, void* mask, uint8_t* root) {
    // The first-person camera, which PlaceCamera has just placed for this
    // eye: the model's world transforms are still around its draw origin
    auto main     = global<uint8_t*>(VrGame::MainSingleton);
    void* camera  = main ? field<void*>(main, VrGame::MainFirstPersonCamera) : nullptr;
    auto graph    = global<uint8_t*>(VrGame::WorldSceneGraph);
    void* culler  = graph ? field<void*>(graph, VrGame::SceneGraphCuller) : nullptr;
    void* accumulator = field<void*>(manager, VrGame::VatsEffectAccumulator);

    if (!camera || !culler || !accumulator)
      return;

    // The mask is cleared to transparent black
    GameColor background = { };
    GameColor clear      = { };
    method<BackgroundFn>(renderer, VrGame::RendererGetBackgroundSlot)(renderer, &background);
    method<BackgroundFn>(renderer, VrGame::RendererSetBackgroundSlot)(renderer, &clear);

    // RenderScene leaves its camera to the image space effects, which the
    // game gives the world camera
    void* shaderCamera = global<void*>(VrGame::ShaderManagerCamera);

    void* group = game<GetGroupFn>(VrGame::RenderedTextureGetGroup)(mask);
    game<StartOffscreenFn>(VrGame::StartOffscreen)(VrGame::ClearAll, group);

    // One mask at a time is measured, until its counts are read back
    bool measure = m_modelQuery && !m_queryPending;

    // As DrawWorld_VATS: the whole model fills the depth only, then the
    // parts are drawn in colour where nothing of the model hides them
    game<ColorWriteFn>(VrGame::SetColorWriteEnable)(0, 1);

    if (measure)
      m_modelQuery->Issue(D3DISSUE_BEGIN);

    drawNodes(camera, culler, accumulator, &root, 1);

    if (measure)
      m_modelQuery->Issue(D3DISSUE_END);

    game<RestoreColorFn>(VrGame::RestoreColorWriteEnable)(1);

    if (measure)
      m_partQuery->Issue(D3DISSUE_BEGIN);

    drawNodes(camera, culler, accumulator, m_parts.data(), m_parts.size());

    if (measure) {
      m_partQuery->Issue(D3DISSUE_END);
      m_queryPending = true;
    }

    game<StopOffscreenFn>(VrGame::StopOffscreen)();

    global<void*>(VrGame::ShaderManagerCamera) = shaderCamera;

    method<BackgroundFn>(renderer, VrGame::RendererSetBackgroundSlot)(renderer, &background);
  }


  void VrPartHighlight::prepareEffect(void* manager, void* mask) {
    game<PrepareFn>(VrGame::VatsPrepareHighlight)(manager, 0, mask, 0, 0);

    // The manager's distortion, pulse and burst follow V.A.T.S., which is
    // not running: no distortion, the settings' blur, and a pulse of the
    // manager's own form, (sin(time * rate) + 1) / 2
    global<float>(VrGame::InterfaceDistortVertical)   = 0.0f;
    global<float>(VrGame::InterfaceDistortHorizontal) = 0.0f;
    global<float>(VrGame::InterfaceDistortPosition)   = -1.0f;
    global<float>(VrGame::InterfaceBlurIntensity)     = setting(VrGame::SettingBlurIntensityVats);
    global<float>(VrGame::InterfaceBlurRadius)        = setting(VrGame::SettingBlurRadiusVats);

    float seconds = std::chrono::duration<float>(std::chrono::steady_clock::now() - m_start).count();
    global<float>(VrGame::InterfacePulse) = 0.5f * (std::sin(seconds * setting(VrGame::SettingTargetPulseRateVats)) + 1.0f);
  }


  void VrPartHighlight::readQueries() {
    if (!m_queryPending)
      return;

    DWORD model = 0;
    DWORD parts = 0;

    if (m_modelQuery->GetData(&model, sizeof(model), 0) != S_OK
     || m_partQuery->GetData(&parts, sizeof(parts), 0) != S_OK)
      return;

    m_queryPending = false;
    m_measure.samples      += 1;
    m_measure.modelSamples += model;
    m_measure.partSamples  += parts;
    m_measure.emptyParts   += parts ? 0 : 1;
  }


  void VrPartHighlight::measureEffect() {
    void* imageSpace = game<PointerFn>(VrGame::ImageSpaceManagerSingleton)();
    void* effect = imageSpace
      ? game<GetEffectFn>(VrGame::ImageSpaceGetEffect)(imageSpace, nullptr, VrGame::ImageSpaceInterface)
      : nullptr;

    if (!effect)
      return;

    bool active = field<uint8_t>(effect, VrGame::ImageSpaceEffectActive) != 0;
    auto last   = field<int32_t>(imageSpace, VrGame::ImageSpaceLastEffect);

    m_measure.effectActive   += active ? 1 : 0;
    m_measure.effectRendered += active && last >= int32_t(VrGame::ImageSpaceInterface) ? 1 : 0;
    m_measure.activeEffects  += uint32_t(field<int32_t>(imageSpace, VrGame::ImageSpaceActiveEffects));
  }


  void VrPartHighlight::logMeasure() {
    auto now = std::chrono::steady_clock::now();

    if (std::chrono::duration<float>(now - m_measureStart).count() < MeasureSeconds)
      return;

    if (m_measure.masks) {
      const Measure& m = m_measure;
      uint32_t read = std::max(m.samples, 1u);

      Logger::info(str::format("VR: Weapon part highlight over ", m.masks, " mask(s): ", m.samples,
        " measured, the model's depth pass covered ", m.modelSamples / read, " samples and the parts ",
        m.partSamples / read, " on average (none in ", m.emptyParts, "); interface effect active in ",
        m.effectActive, ", within the image space effects run in ", m.effectRendered, ", ",
        float(m.activeEffects) / float(m.masks), " effects active on average"));
    }

    m_measure      = Measure();
    m_measureStart = now;
  }


  void __cdecl VrPartHighlight::imageSpaceEffectsHook(
          void*                 renderer,
          void*                 target,
          void*                 destination) {
    auto original = game<EffectsFn>(VrGame::RenderImageSpaceEffects);
    VrPartHighlight* self = g_highlight;
    uint8_t* root = nullptr;

    if (self) {
      self->readQueries();
      self->logMeasure();
    }

    // V.A.T.S. draws its own highlight with the same effect
    if (!self || !renderer || global<void*>(VrGame::VatsMaskTexture)
     || !self->m_take(root, self->m_parts) || !root || self->m_parts.empty()) {
      original(renderer, target, destination);
      return;
    }

    void* manager  = game<GetManagerFn>(VrGame::GetVatsEffect)();
    void* textures = manager ? game<TextureManagerFn>(VrGame::GetTextureManager)() : nullptr;
    void* borrowed = textures ? game<BorrowFn>(VrGame::BorrowRenderedTexture)(textures, renderer,
      VrGame::TextureInterfaceVats, 0, 0, 0) : nullptr;

    if (!borrowed) {
      if (!self->m_loggedNoMask) {
        Logger::warn("VR: No texture was free for the highlight of the weapon's parts, they do not glow");
        self->m_loggedNoMask = true;
      }

      original(renderer, target, destination);
      return;
    }

    // Held as the game holds its own mask, until it is returned
    void* mask = nullptr;
    game<AssignFn>(VrGame::NiPointerAssign)(&mask, borrowed);

    self->drawMask(renderer, manager, mask, root);
    self->prepareEffect(manager, mask);

    if (!self->m_loggedGlow) {
      Logger::info(str::format("VR: The weapon's parts to work glow as V.A.T.S. highlights a limb (",
        self->m_parts.size(), " part(s))"));
      self->m_loggedGlow = true;
    }

    original(renderer, target, destination);

    self->m_measure.masks += 1;
    self->measureEffect();

    game<ShutdownFn>(VrGame::VatsShutdownEffect)();
    game<ReturnFn>(VrGame::ReturnRenderedTexture)(textures, mask);
    game<AssignFn>(VrGame::NiPointerAssign)(&mask, nullptr);
  }

}
