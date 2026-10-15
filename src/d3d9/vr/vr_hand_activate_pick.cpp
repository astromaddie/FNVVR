#include "../../util/log/log.h"

#include "vr_game_addresses.h"
#include "vr_game_memory.h"
#include "vr_hand_activate_pick.h"

namespace dxvk {

  namespace {

    using FindActivateRefFn = void* (__fastcall*)(
      void* viewCaster, void* unused, VrVector3* origin, VrVector3* direction,
      float distance, float* hitDistance, bool* fuzzy);

    static_assert(sizeof(VrVector3) == 12, "The game's NiPoint3 is three floats");

    // What one cast left behind, kept so that the chosen one can be restored
    struct HandCast {
      void* reference   = nullptr;
      float hitDistance = 0.0f;
      bool  fuzzy       = false;
      void* fuzzyBody   = nullptr;
    };

    void*& fuzzyPickBody() {
      return *reinterpret_cast<void**>(VrGame::FuzzyPickBody);
    }

    VrHandActivatePick* g_handActivatePick = nullptr;

  }


  VrHandActivatePick::~VrHandActivatePick() {
    // The patched call stays in place and falls through to the game's function
    g_handActivatePick = nullptr;
  }


  std::unique_ptr<VrHandActivatePick> VrHandActivatePick::install() {
    if (g_handActivatePick)
      return nullptr;

    bool found = VrGameMemory::matches(VrGame::FuzzyPickBodyClear,
        VrGame::FuzzyPickBodyClearBytes, sizeof(VrGame::FuzzyPickBodyClearBytes))
      && VrGameMemory::redirectCall(VrGame::FindActivateRefCallSite, VrGame::FindActivateRef,
           reinterpret_cast<const void*>(&VrHandActivatePick::findHook));

    if (!found)
      return nullptr;

    std::unique_ptr<VrHandActivatePick> pick(new VrHandActivatePick());
    g_handActivatePick = pick.get();
    return pick;
  }


  void VrHandActivatePick::setRays(const std::array<const VrHandRay*, VrHandCount>& rays) {
    for (uint32_t i = 0; i < VrHandCount; i++) {
      m_tracked[i] = rays[i] != nullptr;

      if (rays[i])
        m_rays[i] = *rays[i];
    }
  }


  void* __fastcall VrHandActivatePick::findHook(
          void*                 viewCaster,
          void*                 unused,
          VrVector3*            origin,
          VrVector3*            direction,
          float                 distance,
          float*                hitDistance,
          bool*                 fuzzy) {
    if (!g_handActivatePick) {
      auto original = reinterpret_cast<FindActivateRefFn>(VrGame::FindActivateRef);
      return original(viewCaster, nullptr, origin, direction, distance, hitDistance, fuzzy);
    }

    return g_handActivatePick->find(viewCaster, origin, direction, distance, hitDistance, fuzzy);
  }


  void* VrHandActivatePick::find(
          void*                 viewCaster,
          VrVector3*            origin,
          VrVector3*            direction,
          float                 distance,
          float*                hitDistance,
          bool*                 fuzzy) {
    auto original = reinterpret_cast<FindActivateRefFn>(VrGame::FindActivateRef);

    m_pickedWithHands = m_tracked[0] || m_tracked[1];

    if (!m_pickedWithHands)
      return original(viewCaster, nullptr, origin, direction, distance, hitDistance, fuzzy);

    std::array<HandCast, VrHandCount> casts;
    std::array<VrHandPickResult, VrHandCount> results;

    for (uint32_t i = 0; i < VrHandCount; i++) {
      if (!m_tracked[i])
        continue;

      *origin    = m_rays[i].origin;
      *direction = m_rays[i].direction;

      HandCast& cast = casts[i];
      cast.reference = original(viewCaster, nullptr, origin, direction, distance, &cast.hitDistance, &cast.fuzzy);
      cast.fuzzyBody = fuzzyPickBody();

      results[i].tracked = true;
      results[i].hit     = cast.reference != nullptr;
      results[i].fuzzy   = cast.fuzzy;
    }

    m_hand = vrChooseActivateHand(results, m_hand);

    // The game goes on with the chosen hand's ray and results
    const HandCast& chosen = casts[uint32_t(m_hand)];
    *origin         = m_rays[uint32_t(m_hand)].origin;
    *direction      = m_rays[uint32_t(m_hand)].direction;
    *hitDistance    = chosen.hitDistance;
    *fuzzy          = chosen.fuzzy;
    fuzzyPickBody() = chosen.fuzzyBody;

    if (!m_logged) {
      Logger::info("VR: The activation target is picked along the hands instead of the view");
      m_logged = true;
    }

    return chosen.reference;
  }

}
