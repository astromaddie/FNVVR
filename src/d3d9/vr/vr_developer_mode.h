#pragma once

#include <memory>

#include "vr_game_state.h"

namespace dxvk {

  /**
   * \brief Helps testing the mod: god mode and every playable weapon
   *
   * Enabled by \c d3d9.vrDeveloperMode in dxvk.conf. Each time the game
   * comes back from a loading screen or the main menu, the player gets
   * god mode, as with the console's \c tgm, and one of every playable
   * weapon of the loaded files (DLCs included) that is not in the inventory
   * yet. The weapons are added the way the console's \c AddItem adds them,
   * without the message, at full condition. Weapons the game marks as not
   * playable, such as creature weapons, are left out. A weapon dropped on
   * purpose comes back after the next loading screen. See
   * findings/developer-mode.md.
   *
   * Only used on the game's main thread.
   */
  class VrDeveloperMode {

  public:

    ~VrDeveloperMode();

    /**
     * \brief Checks the game functions it calls
     * \returns \c nullptr if the executable is not the supported version
     */
    static std::unique_ptr<VrDeveloperMode> install();

    /**
     * \brief Gives god mode and the weapons on the first frame in game
     *
     * Called once per frame, before the frame is drawn.
     *
     * \param [in] state What the game shows this frame
     */
    void update(VrGameStateKind state);

  private:

    VrDeveloperMode() = default;

    /// The game showed a loading screen or the main menu since the
    /// weapons were last given
    bool m_pending = true;

    void enableGodMode();

    void addPlayableWeapons();

  };

}
