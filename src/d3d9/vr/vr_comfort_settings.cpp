#include "../../util/log/log.h"
#include "../../util/util_string.h"

#include "vr_comfort_settings.h"
#include "vr_game_addresses.h"
#include "vr_game_memory.h"

namespace dxvk {

  namespace {

    using SetKillCamModeFn = void (__thiscall*)(uintptr_t player, int32_t mode);

  }


  std::unique_ptr<VrComfortSettings> VrComfortSettings::install() {
    bool found = VrGameMemory::matches(VrGame::AutoVanityCheckSite,
        VrGame::AutoVanityCheck, sizeof(VrGame::AutoVanityCheck))
      && VrGameMemory::matches(VrGame::KillCamModeStoreSite,
        VrGame::KillCamModeStore, sizeof(VrGame::KillCamModeStore))
      && VrGameMemory::matches(VrGame::SetKillCamModeCallback,
        VrGame::SetKillCamModeCallbackPrologue, sizeof(VrGame::SetKillCamModeCallbackPrologue))
      && VrGameMemory::readable(VrGame::DisableAutoVanitySetting + VrGame::SettingValue, sizeof(uint8_t))
      && VrGameMemory::readable(VrGame::Player, sizeof(uintptr_t));

    if (!found)
      return nullptr;

    *reinterpret_cast<uint8_t*>(VrGame::DisableAutoVanitySetting + VrGame::SettingValue) = 1;
    Logger::info("VR: The idle third-person camera is turned off");

    return std::unique_ptr<VrComfortSettings>(new VrComfortSettings());
  }


  void VrComfortSettings::update() {
    uintptr_t player = *reinterpret_cast<const uintptr_t*>(VrGame::Player);

    if (!player)
      return;

    int32_t mode = *reinterpret_cast<const int32_t*>(player + VrGame::PlayerKillCamMode);

    if (mode == VrGame::KillCamPlayer)
      return;

    reinterpret_cast<SetKillCamModeFn>(VrGame::SetKillCamModeCallback)(player, VrGame::KillCamPlayer);

    if (!m_loggedKillCam) {
      Logger::info(str::format("VR: The kill camera is set from mode ", mode,
        " to the first-person slow motion"));
      m_loggedKillCam = true;
    }
  }

}
