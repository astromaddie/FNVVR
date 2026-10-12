#pragma once

#include <cstdint>
#include <memory>

namespace dxvk {

  /**
   * \brief Skips the arm animations that raise and lower the Pip-Boy
   *
   * The game opens the Pip-Boy's menus only once the left arm has been
   * raised, and closes it only once the arm has been lowered. In VR the
   * forearm already follows the left controller, so those animations are
   * mostly invisible and only delay the Pip-Boy. See findings/pipboy.md §3.6.
   *
   * Both waits in the game's Pip-Boy state machine are redirected:
   * - raising: the left arm's action is reported as stopped as soon as the
   *   PipBoy sequence has started, so the game snaps the arm to the end of
   *   the animation itself and opens the menus;
   * - lowering: the left arm's Pip-Boy sequence is stopped before the game
   *   checks whether it still plays, so the game restores the arm at once.
   *
   * The game's sounds, menu mode and menus are unchanged. Only used on the
   * game's main thread.
   */
  class VrPipBoyAnimation {

  public:

    ~VrPipBoyAnimation();

    /**
     * \brief Redirects the waits on the arm animations
     * \returns \c nullptr if the executable is not the supported version
     */
    static std::unique_ptr<VrPipBoyAnimation> install();

  private:

    VrPipBoyAnimation() = default;

    bool m_loggedRaise = false;
    bool m_loggedLower = false;

    static uint32_t __fastcall raiseWaitHook(
            uint8_t*              animData,
            void*                 unused,
            int32_t               section);

    static bool __fastcall lowerWaitHook(
            uint8_t*              player,
            void*                 unused);

  };

}
