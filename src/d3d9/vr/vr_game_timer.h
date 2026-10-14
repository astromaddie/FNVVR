#pragma once

#include <cstdint>
#include <memory>

#include "../../util/util_time.h"

#include "vr_game_timer_stats.h"

namespace dxvk {

  /**
   * \brief How the game's frame timer is driven
   *
   * Set by \c d3d9.vrPreciseGameTimer and \c d3d9.vrHavokStepPerFrame.
   */
  struct VrGameTimerConfig {
    /// The frame step is the real interval instead of the tick count
    bool preciseStep     = true;
    /// Havok steps once per frame while the game runs faster than 60 fps
    bool havokPerFrame   = true;
  };

  /**
   * \brief Drives and measures the game's frame timer
   *
   * The game's time step for a frame comes from the system tick count in
   * whole milliseconds, which advances every 15.6 ms on Windows, and the
   * timer update sleeps when the step is below 10 ms (findings/main-loop.md
   * §2). The main loop's call to the update is redirected:
   *
   * - with a precise step, the update gets the real interval since the
   *   previous one as its fixed step, so it neither rounds nor sleeps, and
   *   a tick count taken from the same precise clock. While a save or a new
   *   game loads, the game's own step is kept.
   * - with Havok stepping per frame, Havok's longest step follows the frame
   *   step, so the physics are simulated once per frame by the frame's time.
   *
   * These are the timer fixes of the New Vegas Tick Fix plugin (LGPL),
   * reimplemented from the game's code. When another module already replaced
   * the update or the tick count, as that plugin does, the timer is left to
   * it and only measured.
   *
   * Each update is timed and its step compared with the real interval; a
   * summary is logged every ten seconds of measured frames.
   * Only used on the game's main thread.
   */
  class VrGameTimer {

  public:

    ~VrGameTimer();

    /**
     * \brief Redirects the main loop's timer update
     * \returns \c nullptr if the executable is not the supported version
     */
    static std::unique_ptr<VrGameTimer> install(const VrGameTimerConfig& config);

  private:

    explicit VrGameTimer(const VrGameTimerConfig& config);

    VrGameTimerConfig m_config;
    VrGameTimerStats  m_stats;

    high_resolution_clock::time_point m_lastUpdate;
    bool m_hasLastUpdate   = false;
    bool m_loggedFixedStep = false;

    high_resolution_clock::time_point m_clockStart;
    uint32_t m_tickCountStart = 0;

    float m_havokSetting  = 0.0f;
    bool  m_hasHavokSetting = false;

    void update(
            uint8_t*              timer);

    void updatePrecise(
            uint8_t*              timer);

    void setHavokStep(
            bool                  frameStep,
            double                stepMs);

    bool keepsGameStep() const;

    uint32_t tickCount() const;

    void measure(
      const uint8_t*              timer,
            double                realMs,
            double                updateMs,
            bool                  tickStep);

    void logWindow();

    static void __fastcall updateHook(
            uint8_t*              timer,
            void*                 unused);

  };

}
