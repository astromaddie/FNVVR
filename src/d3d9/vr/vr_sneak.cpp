#include "../../util/log/log.h"
#include "../../util/util_string.h"

#include "vr_game_addresses.h"
#include "vr_game_memory.h"
#include "vr_sneak.h"

namespace dxvk {

  namespace {

    // Actor::IsSneaking, __thiscall without arguments, returns a bool in AL
    using IsSneakingFn = uint32_t (__thiscall*)(uintptr_t actor);

    uintptr_t player() {
      return *reinterpret_cast<const uintptr_t*>(VrGame::Player);
    }

  }


  VrSneak::VrSneak()
  : m_crouch(VrCrouchConfig()) { }


  std::unique_ptr<VrSneak> VrSneak::install() {
    bool found = VrGameMemory::matches(VrGame::ActorIsSneaking,
        VrGame::ActorIsSneakingPrologue, sizeof(VrGame::ActorIsSneakingPrologue))
      && VrGameMemory::readable(VrGame::Player, sizeof(uintptr_t));

    if (!found)
      return nullptr;

    return std::unique_ptr<VrSneak>(new VrSneak());
  }


  void VrSneak::setConfig(const VrSneakConfig& config) {
    if (config.physical != m_config.physical) {
      Logger::info(config.physical
        ? str::format("VR: The player sneaks by crouching ", config.crouch.depth, " m")
        : std::string("VR: The player sneaks with the sneak button"));

      m_toggle.reset();
      m_holdsControl = false;
    }

    m_config = config;
    m_crouch.setConfig(config.crouch);
  }


  void VrSneak::update(const float* headHeight, bool inGame, float dt) {
    uintptr_t actor = player();

    if (!m_config.physical || !actor) {
      m_holdsControl = false;
      m_toggle.reset();
      return;
    }

    bool crouched = headHeight
      ? m_crouch.update(*headHeight, dt)
      : m_crouch.crouched();

    if (headHeight && crouched != m_wasCrouched) {
      Logger::info(str::format("VR: The head is ", m_crouch.standingHeight() - *headHeight,
        " m below its standing height of ", m_crouch.standingHeight(), " m, the player ",
        crouched ? "crouches" : "stands"));
      m_wasCrouched = crouched;
    }

    // The game refuses the control itself at times (some animations,
    // furniture), and a press would only be repeated
    bool allowed = inGame && headHeight
      && !(*reinterpret_cast<const uint8_t*>(actor + VrGame::PlayerDisabledControls) & VrGame::ControlSneak);

    m_holdsControl = m_toggle.update(crouched, isSneaking(actor), allowed, dt);
  }


  void VrSneak::measureCamera(const VrGameCameraPose* pose, float dt) {
    uintptr_t actor = player();

    if (!m_config.physical) {
      m_camera.reset();
      return;
    }

    if (!pose || !actor) {
      m_camera.update(nullptr, false, dt);
      return;
    }

    const float* location = reinterpret_cast<const float*>(actor + VrGame::RefLocation);
    float height = pose->position.z - location[2];

    if (m_camera.update(&height, isSneaking(actor), dt) > 0.0f && !m_loggedRaise) {
      Logger::info("VR: The game lowers its camera to sneak, the view is kept at the head's height");
      m_loggedRaise = true;
    }
  }


  void VrSneak::adjustCamera(VrGameCameraPose& pose) const {
    // The game's world is Z up
    if (m_config.physical)
      pose.position.z += m_camera.raise();
  }


  bool VrSneak::isSneaking(uintptr_t actor) {
    return (reinterpret_cast<IsSneakingFn>(VrGame::ActorIsSneaking)(actor) & 0xFF) != 0;
  }

}
