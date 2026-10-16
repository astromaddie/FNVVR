#pragma once

#include <cstdint>

namespace dxvk {

  /**
   * \brief Moves the HUD's XP meter to the top right corner
   *
   * The game shows the experience gained and the "LEVEL UP" line in the
   * XPMeter group, in the bottom right corner, in the same rows as the
   * subtitles, which the panel in front of the head keeps at the bottom.
   * The meter is moved up to the row of the messages, which are in the top
   * left corner, so that the panel can bring both corners towards the
   * centre (vrComputeHeadHudLayout). The game window shows no HUD in game,
   * so moving the group there changes nothing on the screen.
   *
   * The game keeps the meter's base height in a trait of its own and sets
   * its height from that base when the HUD's visibility mode changes
   * (HUDMainMenu::UpdateVisibilityState), so both are moved, by the same
   * distance. See findings/hud.md §11.
   *
   * Only used on the game's main thread.
   */
  class VrXpMeter {

  public:

    /**
     * \brief Checks that the executable is the supported version
     * \returns \c false if the game's code was not found
     */
    bool initialize();

    /**
     * \brief Moves the meter to the row of the messages if it is not there
     *
     * Called once per frame, since the game builds the HUD again (and puts
     * the meter back) when it reads the HUD's menu file.
     *
     * \returns \c true if the meter is in the top right corner
     */
    bool place();

  private:

    bool m_available = false;
    bool m_loggedMove = false;

  };

}
