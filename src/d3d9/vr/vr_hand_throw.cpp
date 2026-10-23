#include <cmath>
#include <cstring>

#include "../../util/log/log.h"
#include "../../util/util_string.h"

#include "vr_game_addresses.h"
#include "vr_game_memory.h"
#include "vr_hand_throw.h"
#include "vr_math.h"
#include "vr_melee.h"
#include "vr_weapon_aim.h"

namespace dxvk {

  namespace {

    // TESObjectWEAP::Fire, __thiscall with the actor
    using WeaponFireFn = void (__fastcall*)(uint8_t* weapon, void* unused, uint8_t* actor);

    // How long the hand stays empty after a throw before the next weapon
    // is shown in it, about the game's throw animation from its release
    constexpr float RefillTime = 0.5f;

    constexpr uint32_t MaxLoggedThrows = 50;
    constexpr uint32_t MaxLoggedMisses = 10;
    constexpr uint32_t MaxLoggedArms   = 50;

    constexpr float DegreesPerRadian = 57.2957795f;

    uint8_t* player() {
      return *reinterpret_cast<uint8_t* const*>(VrGame::Player);
    }

    template<typename T>
    T& field(uint8_t* base, uintptr_t offset) {
      return *reinterpret_cast<T*>(base + offset);
    }

    template<typename T>
    T readField(const uint8_t* base, uintptr_t offset) {
      T value;
      std::memcpy(&value, base + offset, sizeof(value));
      return value;
    }

    const char* weaponName(const uint8_t* form) {
      const char* name = readField<const char*>(form, VrGame::WeaponFullName);
      return name ? name : "";
    }

    /**
     * \brief A position in the game world, in metres in the body's frame
     */
    VrVector3 inBody(const VrGameCameraPose& body, const VrVector3& world) {
      return vrPointToHand(body, world) * (1.0f / VrGameUnitsPerMetre);
    }

  }


  std::unique_ptr<VrHandThrow> VrHandThrow::install(VrWeaponHand& weaponHand) {
    if (!VrGameMemory::matches(VrGame::WeaponFire, VrGame::WeaponFirePrologue, sizeof(VrGame::WeaponFirePrologue)))
      return nullptr;

    Logger::info("VR: Grenades, mines and thrown weapons are armed with the left hand and thrown with the right");
    return std::unique_ptr<VrHandThrow>(new VrHandThrow(weaponHand));
  }


  VrHandThrowEvents VrHandThrow::update(const VrHandThrowInput& input) {
    VrHandThrowEvents events;

    const VrHeldThrown* held = m_weaponHand.heldThrown();
    bool vats = *reinterpret_cast<const uint32_t*>(VrGame::VatsCameraState) != 0;

    m_throwsWithHand = held && input.hand && !vats;

    // The next one comes into the hand, unarmed, even if the Pip-Boy was
    // opened meanwhile
    if (m_refill > 0.0f) {
      m_refill -= input.dt;

      if (m_refill <= 0.0f) {
        m_weaponHand.showThrownAgain();
        m_arm.reset(m_arm.arming());
      }
    }

    if (!m_throwsWithHand) {
      m_holding = false;
      m_tracker.reset();

      // V.A.T.S. throws with the game's animation, which brings the next
      // one unarmed. Elsewhere (Pip-Boy, menus) the weapon stays as it was.
      if (vats) {
        m_weaponHand.setThrowArmed(false);
        m_form   = nullptr;
        m_refill = 0.0f;
      }

      return events;
    }

    if (held->form != m_form)
      startWeapon(*held);

    VrVector3 centre = vrPointFromHand(*input.hand, held->centre);
    bool ready = held->shown && m_refill <= 0.0f;

    if (ready && input.otherHand) {
      events.arm = m_arm.update(*input.otherHand * (1.0f / VrGameUnitsPerMetre), centre * (1.0f / VrGameUnitsPerMetre),
        held->radius / VrGameUnitsPerMetre, input.grip, input.trigger, input.dt, m_config);
      logArm(events.arm);
    }

    m_weaponHand.setThrowArmed(m_arm.armed() && m_arm.arming() == VrThrowArming::Pin);

    // The motion counts from the press of the trigger
    if (input.attack && !m_holding)
      m_tracker.reset();

    m_tracker.add(inBody(input.body, centre), input.dt);

    bool released = m_holding && !input.attack;
    m_holding = input.attack;

    if (!released || !ready)
      return events;

    if (!m_arm.armed()) {
      if (m_loggedUnarmed < MaxLoggedMisses) {
        Logger::info(str::format("VR: '", weaponName(held->form), "' is not armed (",
          vrThrowArmingName(m_arm.arming()), "), it stays in the hand"));
        m_loggedUnarmed++;
      }
      return events;
    }

    VrVector3 velocity  = m_tracker.velocity(m_config.velocityWindow);
    float     handSpeed = vrLength(velocity);
    float     speed     = vrComputeThrowSpeed(handSpeed, m_config);

    if (!(speed > 0.0f)) {
      if (m_loggedSlow < MaxLoggedMisses) {
        Logger::info(str::format("VR: The hand let go at ", handSpeed, " m/s, slower than ", m_config.minSpeed,
          " m/s: '", weaponName(held->form), "' stays in the hand"));
        m_loggedSlow++;
      }
      return events;
    }

    // From the body's frame back to the world, in game units per second
    VrVector3 direction = vrPointFromHand(input.body, velocity * (1.0f / handSpeed)) - input.body.position;
    fire(*held, centre, direction * (speed * VrGameUnitsPerMetre));

    if (m_loggedThrows < MaxLoggedThrows) {
      const VrThrowSpeeds& speeds = m_weaponHand.throwSpeeds();
      VrHeadAngles angles = vrComputeAimAngles(direction);

      Logger::info(str::format("VR: Threw '", weaponName(held->form), "', hand at ", handSpeed, " m/s, ",
        -angles.pitch * DegreesPerRadian, " degrees up: ", speeds.given / VrGameUnitsPerMetre,
        " m/s (game's full throw ", speeds.full / VrGameUnitsPerMetre, " m/s)"));
      m_loggedThrows++;
    }

    events.thrown = true;
    return events;
  }


  void VrHandThrow::startWeapon(const VrHeldThrown& held) {
    const char* model = readField<const char*>(held.form, VrGame::WeaponModelPath);
    VrThrowArming arming = vrThrowArming(held.type, model);

    m_form    = held.form;
    m_holding = false;
    m_refill  = 0.0f;
    m_arm.reset(arming);
    m_tracker.reset();

    Logger::info(str::format("VR: '", weaponName(held.form), "' (", model ? model : "", ", type ", held.type,
      ") is armed by hand: ", vrThrowArmingName(arming)));
  }


  void VrHandThrow::fire(const VrHeldThrown& held, const VrVector3& origin, const VrVector3& velocity) {
    uint8_t* actor  = player();
    auto     weapon = const_cast<uint8_t*>(held.form);

    // Thrown weapons get the power of a throw held to the end, as the game
    // gives V.A.T.S. throws: the hand's speed decides instead
    float& releaseTimer = field<float>(actor, VrGame::PlayerProjectileReleaseTimer);
    float  timer = releaseTimer;
    releaseTimer = VrGame::ThrowReleaseTimerFull;

    VrThrowRelease release = { origin, velocity };
    m_weaponHand.setThrowRelease(&release);
    reinterpret_cast<WeaponFireFn>(VrGame::WeaponFire)(weapon, nullptr, actor);
    m_weaponHand.setThrowRelease(nullptr);

    releaseTimer = timer;

    m_holding = false;
    m_refill  = RefillTime;
    m_tracker.reset();
    m_weaponHand.setThrowArmed(false);
  }


  void VrHandThrow::logArm(VrArmEvent event) {
    if (event == VrArmEvent::None || m_loggedArms >= MaxLoggedArms)
      return;

    const char* what = "";

    switch (event) {
      case VrArmEvent::PinTaken:   what = "The left hand took the pin"; break;
      case VrArmEvent::PinDropped: what = "The left hand let go of the pin"; break;
      case VrArmEvent::Lighting:   what = "The lighter is at the fuse"; break;
      case VrArmEvent::Armed:      what = "Armed"; break;
      case VrArmEvent::None:       break;
    }

    Logger::info(str::format("VR: ", what, " (", vrThrowArmingName(m_arm.arming()), ")"));
    m_loggedArms++;
  }

}
