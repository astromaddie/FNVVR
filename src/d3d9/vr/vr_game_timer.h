#pragma once

#include <cstdint>
#include <memory>

#include "../../util/util_time.h"

#include "vr_game_timer_stats.h"

namespace dxvk {

  /**
   * \brief Measures the game's frame timer against real time
   *
   * The game's time step for a frame comes from the system tick count in
   * whole milliseconds, and the timer update sleeps when the step is below
   * 10 ms (findings/main-loop.md §2). The main loop's call to the update is
   * redirected so that each update is timed, and its step compared with the
   * real interval since the previous one. A summary is logged every ten
   * seconds of measured frames.
   *
   * Measurement only: the game's timer is not changed. When another module
   * (such as the New Vegas Tick Fix plugin) already replaced the update or
   * the tick count, that is logged and its replacement is measured.
   * Only used on the game's main thread.
   */
  class VrGameTimer {

  public:

    ~VrGameTimer();

    /**
     * \brief Redirects the main loop's timer update
     * \returns \c nullptr if the executable is not the supported version
     */
    static std::unique_ptr<VrGameTimer> install();

  private:

    VrGameTimer();

    VrGameTimerStats m_stats;

    high_resolution_clock::time_point m_lastUpdate;
    bool m_hasLastUpdate   = false;
    bool m_loggedFixedStep = false;

    void measure(
      const uint8_t*                    timer,
            high_resolution_clock::time_point start,
            high_resolution_clock::time_point end);

    void logWindow();

    static void __fastcall updateHook(
            uint8_t*              timer,
            void*                 unused);

  };

}
