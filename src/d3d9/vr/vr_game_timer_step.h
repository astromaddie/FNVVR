#pragma once

namespace dxvk {

  /**
   * \brief Shortest frame step given to the game, in milliseconds
   */
  constexpr double VrGameTimerMinStepMs = 1.0;

  /**
   * \brief Longest frame step given to the game, in milliseconds
   *
   * The game's own limit for steps taken from the tick count
   * (findings/main-loop.md §2).
   */
  constexpr double VrGameTimerMaxStepMs = 166.0;

  /**
   * \brief Frame step for a real interval between two timer updates
   *
   * The step replaces the game's whole-millisecond tick difference with the
   * real interval, kept within the limits the game applies itself.
   * \param [in] realMs Real time since the previous update, in milliseconds
   * \returns The step in milliseconds
   */
  double vrGameTimerStepMs(double realMs);

  /**
   * \brief Havok's longest simulation step for a frame step
   *
   * Havok steps by \c fMaxTime:HAVOK (1/60 s by default) and catches up with
   * the frame time across frames. With a step no longer than the frame,
   * the world is simulated once per frame by the frame's own time while the
   * game runs faster than the setting, and as before when it runs slower.
   * \param [in] stepMs Frame step in milliseconds
   * \param [in] settingSeconds The game's own \c fMaxTime:HAVOK
   * \returns The longest step in seconds
   */
  float vrHavokMaxStep(double stepMs, float settingSeconds);

}
