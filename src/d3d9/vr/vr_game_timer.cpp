#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>

#include <windows.h>

#include "../../util/log/log.h"
#include "../../util/util_string.h"

#include "vr_game_addresses.h"
#include "vr_game_memory.h"
#include "vr_game_timer.h"
#include "vr_game_timer_step.h"

namespace dxvk {

  namespace {

    using TimerUpdateFn   = void (__fastcall*)(uint8_t* timer, void* unused);
    using TimerUpdateMsFn = void (__fastcall*)(uint8_t* timer, void* unused, uint32_t ms);

    // Real time covered by each logged window
    constexpr double WindowMs = 10000.0;

    VrGameTimer* g_gameTimer = nullptr;

    // Function the main loop called before the redirection. The call stays
    // redirected after the timer is gone and falls through to it.
    TimerUpdateFn g_timerUpdate = nullptr;

    template<typename T>
    T& field(uint8_t* object, uintptr_t offset) {
      return *reinterpret_cast<T*>(object + offset);
    }

    template<typename T>
    T field(const uint8_t* object, uintptr_t offset) {
      return *reinterpret_cast<const T*>(object + offset);
    }

    double millisecondsBetween(high_resolution_clock::time_point from, high_resolution_clock::time_point to) {
      return std::chrono::duration<double, std::milli>(to - from).count();
    }

    std::string moduleName(uintptr_t address) {
      HMODULE module = nullptr;

      if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
          reinterpret_cast<LPCWSTR>(address), &module))
        return "an unknown module";

      WCHAR path[MAX_PATH] = { };
      DWORD length = GetModuleFileNameW(module, path, MAX_PATH);
      return str::fromws(std::wstring(path, length < MAX_PATH ? length : 0).c_str());
    }

    std::string fixed(double value) {
      std::ostringstream stream;
      stream << std::fixed << std::setprecision(2) << value;
      return stream.str();
    }

    // Says where the game's tick count comes from: another module may have
    // replaced the import, as the New Vegas Tick Fix plugin does
    bool tickCountIsSystem() {
      if (!VrGameMemory::readable(VrGame::TickCountImport, sizeof(uintptr_t)))
        return false;

      uintptr_t imported = *reinterpret_cast<const uintptr_t*>(VrGame::TickCountImport);
      HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
      uintptr_t system = kernel32
        ? reinterpret_cast<uintptr_t>(GetProcAddress(kernel32, "GetTickCount")) : 0;

      if (imported == system) {
        Logger::info("VR: The game's frame timer reads the system tick count");
        return true;
      }

      Logger::info(str::format("VR: The game's tick count import was replaced by ", moduleName(imported)));
      return false;
    }

    bool statesReadable() {
      return VrGameMemory::readable(VrGame::SaveLoadGame, sizeof(uintptr_t))
          && VrGameMemory::readable(VrGame::LoadingNewGame, 1)
          && VrGameMemory::readable(VrGame::PauseFade, 1);
    }

  }


  VrGameTimer::VrGameTimer(const VrGameTimerConfig& config)
  : m_config(config), m_stats(WindowMs) { }


  VrGameTimer::~VrGameTimer() {
    // Havok gets its own setting back
    if (m_hasHavokSetting)
      *reinterpret_cast<float*>(VrGame::HavokMaxStep) = m_havokSetting;

    g_gameTimer = nullptr;
  }


  std::unique_ptr<VrGameTimer> VrGameTimer::install(const VrGameTimerConfig& config) {
    if (g_gameTimer || g_timerUpdate)
      return nullptr;

    uintptr_t target = 0;

    if (!VrGameMemory::callTarget(VrGame::TimerUpdateCallSite, &target)
     || !VrGameMemory::readable(VrGame::AppTimer, VrGame::TimerSize))
      return nullptr;

    bool gameUpdate = target == VrGame::TimerUpdate;

    if (!gameUpdate)
      Logger::info(str::format("VR: The game's frame timer update was replaced by ", moduleName(target)));

    g_timerUpdate = reinterpret_cast<TimerUpdateFn>(target);

    if (!VrGameMemory::redirectCall(VrGame::TimerUpdateCallSite, target,
        reinterpret_cast<const void*>(&VrGameTimer::updateHook))) {
      g_timerUpdate = nullptr;
      return nullptr;
    }

    bool systemTicks = tickCountIsSystem();

    VrGameTimerConfig used = config;

    if (used.preciseStep && !(gameUpdate && systemTicks)) {
      Logger::info("VR: Another module drives the game's frame timer, the VR layer only measures it");
      used.preciseStep = false;
    } else if (used.preciseStep && (!statesReadable() || !VrGameMemory::readable(VrGame::HavokMaxStep, sizeof(float)))) {
      Logger::info("VR: The game's loading states were not found, the frame timer keeps the tick count");
      used.preciseStep = false;
    }

    used.havokPerFrame = used.havokPerFrame && used.preciseStep;

    std::unique_ptr<VrGameTimer> timer(new VrGameTimer(used));
    timer->m_clockStart     = high_resolution_clock::now();
    timer->m_tickCountStart = GetTickCount();

    if (used.preciseStep)
      Logger::info("VR: The game's frame step is the real frame time (timer fix of the New Vegas Tick Fix)");

    if (used.havokPerFrame)
      Logger::info("VR: Havok steps once per frame while the game runs faster than its fMaxTime setting");

    Logger::info("VR: The game's frame timer is measured against real time");

    g_gameTimer = timer.get();
    return timer;
  }


  void VrGameTimer::update(uint8_t* timer) {
    high_resolution_clock::time_point start = high_resolution_clock::now();

    double realMs = m_hasLastUpdate ? millisecondsBetween(m_lastUpdate, start) : 0.0;
    bool tickStep = true;

    if (m_config.preciseStep && m_hasLastUpdate && !keepsGameStep()
     && field<float>(timer, VrGame::TimerFixedStep) == 0.0f) {
      double stepMs = vrGameTimerStepMs(realMs);

      // The update takes a fixed step without rounding or sleeping. The
      // field is cleared again so the game never sees it set.
      field<float>(timer, VrGame::TimerFixedStep) = float(stepMs);
      setHavokStep(true, stepMs);
      updatePrecise(timer);
      field<float>(timer, VrGame::TimerFixedStep) = 0.0f;
      tickStep = false;
    } else if (m_config.preciseStep) {
      setHavokStep(false, 0.0);
      updatePrecise(timer);
    } else {
      g_timerUpdate(timer, nullptr);
    }

    high_resolution_clock::time_point end = high_resolution_clock::now();

    m_lastUpdate    = start;
    m_hasLastUpdate = true;

    measure(timer, realMs, millisecondsBetween(start, end), tickStep);
  }


  void VrGameTimer::updatePrecise(
          uint8_t*              timer) {
    // The tick count comes from the same clock as the step, so that the
    // game's own step after the precise ones starts from the right time
    uint32_t ms = tickCount();

    // Fixed steps advance the last tick by their rounded length, which can
    // run up to a millisecond ahead of the clock. A tick behind the last one
    // would wrap into the game's longest step.
    if (field<float>(timer, VrGame::TimerFixedStep) == 0.0f) {
      uint32_t time = ms - field<uint32_t>(timer, VrGame::TimerFirstTime);
      uint32_t& last = field<uint32_t>(timer, VrGame::TimerLastTime);

      if (int32_t(time - last) < 0)
        last = time;
    }

    auto updateMs = reinterpret_cast<TimerUpdateMsFn>(VrGame::TimerUpdateMs);
    updateMs(timer, nullptr, ms);
  }


  void VrGameTimer::setHavokStep(
          bool                  frameStep,
          double                stepMs) {
    if (!m_config.havokPerFrame)
      return;

    float& maxStep = *reinterpret_cast<float*>(VrGame::HavokMaxStep);

    if (!m_hasHavokSetting) {
      m_havokSetting    = maxStep;
      m_hasHavokSetting = true;
    }

    bool pauseFade = *reinterpret_cast<const uint8_t*>(VrGame::PauseFade) != 0;

    maxStep = frameStep && !pauseFade && m_havokSetting > 0.0f
      ? vrHavokMaxStep(stepMs, m_havokSetting)
      : m_havokSetting;
  }


  bool VrGameTimer::keepsGameStep() const {
    if (*reinterpret_cast<const uint8_t*>(VrGame::LoadingNewGame))
      return true;

    const uint8_t* saveLoad = *reinterpret_cast<uint8_t* const*>(VrGame::SaveLoadGame);
    return saveLoad && (field<uint32_t>(saveLoad, VrGame::SaveLoadFlags) & VrGame::SaveLoadLoading);
  }


  uint32_t VrGameTimer::tickCount() const {
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      high_resolution_clock::now() - m_clockStart);
    return m_tickCountStart + uint32_t(elapsed.count());
  }


  void VrGameTimer::measure(
    const uint8_t*              timer,
          double                realMs,
          double                updateMs,
          bool                  tickStep) {
    // The first update has no interval, and a disabled timer takes no step
    if (realMs <= 0.0 || field<uint8_t>(timer, VrGame::TimerDisabled))
      return;

    float fixedStep = field<float>(timer, VrGame::TimerFixedStep);

    if (fixedStep != 0.0f && !m_loggedFixedStep) {
      Logger::info(str::format("VR: The game's frame timer takes a fixed step of ", fixed(fixedStep), " ms"));
      m_loggedFixedStep = true;
    }

    VrGameTimerSample sample;
    sample.realMs   = realMs;
    sample.gameMs   = double(field<float>(timer, VrGame::TimerStep)) * 1000.0;
    sample.updateMs = updateMs;
    sample.noTick   = tickStep && field<uint8_t>(timer, VrGame::TimerNoTick) != 0;

    if (m_stats.add(sample)) {
      logWindow();
      m_stats.reset();
    }
  }


  void VrGameTimer::logWindow() {
    std::string steps;

    for (const auto& step : m_stats.commonSteps()) {
      if (!step.frames)
        continue;

      if (!steps.empty())
        steps += ", ";

      steps += str::format(step.ms, step.ms + 1 == VrGameTimerStats::StepCounts ? "+" : "", " ms x", step.frames);
    }

    double drift = m_stats.driftMs();

    Logger::info(str::format("VR: Game timer over ", fixed(m_stats.seconds()), " s, ", m_stats.frames(),
      " frames: real interval ", fixed(m_stats.averageRealMs()), " ms, game step ", fixed(m_stats.averageGameMs()),
      " ms, game time ", drift >= 0.0 ? "ahead by " : "behind by ", fixed(std::abs(drift)),
      " ms, step error average/largest ", fixed(m_stats.averageErrorMs()), "/", fixed(m_stats.maxErrorMs()),
      " ms, ", m_stats.noTickFrames(), " frames without a tick, ", m_stats.sleptFrames(),
      " frames slept in the update for ", fixed(m_stats.averageSleptMs()), " ms on average (longest update ",
      fixed(m_stats.maxUpdateMs()), " ms), ", m_stats.longIntervals(), " long intervals left out. Most frequent steps: ",
      steps));
  }


  void __fastcall VrGameTimer::updateHook(
          uint8_t*              timer,
          void*                 unused) {
    if (g_gameTimer && timer)
      g_gameTimer->update(timer);
    else
      g_timerUpdate(timer, nullptr);
  }

}
