#pragma once

#include <cstdint>
#include <memory>

#include "../../util/thread.h"

namespace dxvk {

  /**
   * \brief Reloading the weapons with the hands
   *
   * First step, measuring only (findings/reload.md §8): every reload of
   * the player goes through one entry of the PlayerCharacter vtable, which
   * is replaced so that each reload is logged with its caller, its
   * arguments and the clip before and after, and the game's jams of the
   * player's weapon are logged where StartAttack looks up their animation.
   * The game's behaviour is unchanged.
   *
   * The reloads may be started from the game's other threads, so the clip
   * left to watch is shared under a lock.
   */
  class VrManualReload {

  public:

    ~VrManualReload();

    /**
     * \brief Replaces the player's reload and watches the jams
     * \returns \c nullptr if the executable is not the supported version
     */
    static std::unique_ptr<VrManualReload> install();

    /**
     * \brief Logs the clip of a recent reload when it changes
     *
     * The game fills the clip through its equipment queue a frame or so
     * after the reload starts. Called once per frame on the game's main
     * thread.
     */
    void update();

  private:

    VrManualReload() = default;

    dxvk::mutex     m_mutex;

    // Frames left during which the clip of the last reload is watched, and
    // the clip count logged last
    uint32_t        m_watchFrames = 0;
    int32_t         m_watchedClip = 0;

    void watchClip(int32_t clip);

    static uint32_t __fastcall reloadHook(
            uint8_t*              player,
            void*                 unused,
            uint8_t*              weapon,
            uint32_t              animation,
            uint32_t              useMod,
            uint32_t              fromInventory);

    static uint32_t __fastcall jamGroupHook(
            uint8_t*              actor,
            void*                 unused,
            uint32_t              group,
            void*                 weaponEntry,
            uint32_t              checkHandToHand,
            void*                 animation);

  };

}
