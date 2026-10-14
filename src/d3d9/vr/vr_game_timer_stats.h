#pragma once

#include <array>
#include <cstdint>

namespace dxvk {

  /**
   * \brief One update of the game's frame timer
   */
  struct VrGameTimerSample {
    /// Real time since the previous update, in milliseconds
    double realMs   = 0.0;
    /// Time step the game took, before its time multiplier, in milliseconds
    double gameMs   = 0.0;
    /// Time spent in the update, which sleeps when the step is short
    double updateMs = 0.0;
    /// The tick count had not advanced since the previous update
    bool   noTick   = false;
  };

  /**
   * \brief A time step and how many frames took it
   */
  struct VrGameTimerStep {
    uint32_t ms     = 0;
    uint32_t frames = 0;
  };

  /**
   * \brief The game's frame timer against real time, over a window
   *
   * The game measures each frame with the system tick count in whole
   * milliseconds, which only advances every 15.6 ms on Windows by default,
   * and sleeps inside the update when the step is below 10 ms. The window
   * shows how far the game's steps are from the real frame intervals and
   * how long the game slept. See findings/main-loop.md §2.
   *
   * Intervals longer than LongIntervalMs (loading screens, a paused
   * process) are counted but left out of the figures.
   */
  class VrGameTimerStats {

  public:

    /// Real intervals from this length on are left out
    static constexpr double LongIntervalMs = 250.0;

    /// An update that took at least this long has slept
    static constexpr double SleptMs = 0.5;

    /// Steps of this many milliseconds and more share the last count
    static constexpr uint32_t StepCounts = 200;

    /// Number of steps listed by commonSteps
    static constexpr uint32_t CommonSteps = 4;

    /**
     * \param [in] windowMs Real time covered by a window, in milliseconds
     */
    explicit VrGameTimerStats(double windowMs);

    /**
     * \brief Adds a timer update
     * \returns \c true if the window is full. It stays full until reset.
     */
    bool add(const VrGameTimerSample& sample);

    /**
     * \brief Empties the window
     */
    void reset();

    uint32_t frames() const {
      return m_frames;
    }

    uint32_t longIntervals() const {
      return m_long;
    }

    uint32_t noTickFrames() const {
      return m_noTick;
    }

    uint32_t sleptFrames() const {
      return m_slept;
    }

    /**
     * \brief Real time covered by the frames, in seconds
     */
    double seconds() const {
      return m_realSum / 1000.0;
    }

    double averageRealMs() const;

    double averageGameMs() const;

    /**
     * \brief Game time minus real time over the window, in milliseconds
     */
    double driftMs() const {
      return m_gameSum - m_realSum;
    }

    /**
     * \brief Average distance between a step and its real interval
     */
    double averageErrorMs() const;

    double maxErrorMs() const {
      return m_errorMax;
    }

    /**
     * \brief Average time spent in the updates that slept
     */
    double averageSleptMs() const;

    double maxUpdateMs() const {
      return m_updateMax;
    }

    /**
     * \brief The most frequent steps, rounded to milliseconds
     *
     * Most frequent first. Unused entries have no frames.
     */
    std::array<VrGameTimerStep, CommonSteps> commonSteps() const;

  private:

    double   m_windowMs;
    uint32_t m_frames    = 0;
    uint32_t m_long      = 0;
    uint32_t m_noTick    = 0;
    uint32_t m_slept     = 0;
    double   m_realSum   = 0.0;
    double   m_gameSum   = 0.0;
    double   m_errorSum  = 0.0;
    double   m_errorMax  = 0.0;
    double   m_sleptSum  = 0.0;
    double   m_updateMax = 0.0;

    std::array<uint32_t, StepCounts> m_steps = { };

  };

}
