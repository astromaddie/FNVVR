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

namespace dxvk {

  namespace {

    using TimerUpdateFn = void (__fastcall*)(uint8_t* timer, void* unused);

    // Real time covered by each logged window
    constexpr double WindowMs = 10000.0;

    VrGameTimer* g_gameTimer = nullptr;

    // Function the main loop called before the redirection. The call stays
    // redirected after the measurement ends and falls through to it.
    TimerUpdateFn g_timerUpdate = nullptr;

    template<typename T>
    T field(const uint8_t* object, uintptr_t offset) {
      return *reinterpret_cast<const T*>(object + offset);
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
    void logTickCountSource() {
      if (!VrGameMemory::readable(VrGame::TickCountImport, sizeof(uintptr_t)))
        return;

      uintptr_t imported = *reinterpret_cast<const uintptr_t*>(VrGame::TickCountImport);
      HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
      uintptr_t system = kernel32
        ? reinterpret_cast<uintptr_t>(GetProcAddress(kernel32, "GetTickCount")) : 0;

      if (imported == system)
        Logger::info("VR: The game's frame timer reads the system tick count");
      else
        Logger::info(str::format("VR: The game's tick count import was replaced by ", moduleName(imported)));
    }

  }


  VrGameTimer::VrGameTimer()
  : m_stats(WindowMs) { }


  VrGameTimer::~VrGameTimer() {
    g_gameTimer = nullptr;
  }


  std::unique_ptr<VrGameTimer> VrGameTimer::install() {
    if (g_gameTimer || g_timerUpdate)
      return nullptr;

    uintptr_t target = 0;

    if (!VrGameMemory::callTarget(VrGame::TimerUpdateCallSite, &target)
     || !VrGameMemory::readable(VrGame::AppTimer, VrGame::TimerSize))
      return nullptr;

    if (target != VrGame::TimerUpdate)
      Logger::info(str::format("VR: The game's frame timer update was replaced by ", moduleName(target)));

    g_timerUpdate = reinterpret_cast<TimerUpdateFn>(target);

    if (!VrGameMemory::redirectCall(VrGame::TimerUpdateCallSite, target,
        reinterpret_cast<const void*>(&VrGameTimer::updateHook))) {
      g_timerUpdate = nullptr;
      return nullptr;
    }

    logTickCountSource();
    Logger::info("VR: The game's frame timer is measured against real time");

    std::unique_ptr<VrGameTimer> timer(new VrGameTimer());
    g_gameTimer = timer.get();
    return timer;
  }


  void VrGameTimer::measure(
    const uint8_t*                    timer,
          high_resolution_clock::time_point start,
          high_resolution_clock::time_point end) {
    auto ms = [] (high_resolution_clock::time_point from, high_resolution_clock::time_point to) {
      return std::chrono::duration<double, std::milli>(to - from).count();
    };

    bool hadLastUpdate = m_hasLastUpdate;
    high_resolution_clock::time_point lastUpdate = m_lastUpdate;

    m_lastUpdate    = start;
    m_hasLastUpdate = true;

    // A disabled timer takes no step
    if (!hadLastUpdate || field<uint8_t>(timer, VrGame::TimerDisabled))
      return;

    float fixedStep = field<float>(timer, VrGame::TimerFixedStep);

    if (fixedStep != 0.0f && !m_loggedFixedStep) {
      Logger::info(str::format("VR: The game's frame timer takes a fixed step of ", fixed(fixedStep), " ms"));
      m_loggedFixedStep = true;
    }

    VrGameTimerSample sample;
    sample.realMs   = ms(lastUpdate, start);
    sample.gameMs   = double(field<float>(timer, VrGame::TimerStep)) * 1000.0;
    sample.updateMs = ms(start, end);
    sample.noTick   = field<uint8_t>(timer, VrGame::TimerNoTick) != 0;

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
    high_resolution_clock::time_point start = high_resolution_clock::now();
    g_timerUpdate(timer, nullptr);

    if (g_gameTimer && timer)
      g_gameTimer->measure(timer, start, high_resolution_clock::now());
  }

}
