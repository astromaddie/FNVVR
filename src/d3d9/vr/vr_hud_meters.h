#pragma once

#include <cstdint>

namespace dxvk {

  /**
   * \brief Which meters VrHudMeters placed in the top right corner
   */
  struct VrHudMetersPlaced {
    bool radiation = false;
    bool xp        = false;
  };

  /**
   * \brief Moves the HUD's radiation and XP meters to the top right corner
   *
   * The panel in front of the head brings the top right corner towards the
   * centre (vrComputeHeadHudLayout), so both meters are placed there, in a
   * column: the radiation meter in the row of the messages, which are in
   * the top left corner, and the XP meter just below it, so that the two
   * never overlap.
   *
   * The game puts the radiation meter in the top right corner already, a
   * little above the messages. It shows the experience gained and the
   * "LEVEL UP" line in the XPMeter group, in the bottom right corner, in
   * the same rows as the subtitles, which the panel keeps at the bottom.
   * The game window shows no HUD in game, so moving the groups there
   * changes nothing on the screen.
   *
   * Only HUDMainMenu::Create sets the radiation meter's height. The game
   * keeps the XP meter's base height in a trait of its own and sets its
   * height from that base when the HUD's visibility mode changes
   * (HUDMainMenu::UpdateVisibilityState), so both are moved, by the same
   * distance. See findings/hud.md §11-12.
   *
   * Only used on the game's main thread.
   */
  class VrHudMeters {

  public:

    /**
     * \brief Checks that the executable is the supported version
     * \returns \c false if the game's code was not found for either meter
     */
    bool initialize();

    /**
     * \brief Moves the meters to the top right corner if they are not there
     *
     * Called once per frame, since the game builds the HUD again (and puts
     * the meters back) when it reads the HUD's menu file.
     *
     * \returns Which meters are in the top right corner
     */
    VrHudMetersPlaced place();

  private:

    bool m_radiationAvailable = false;
    bool m_xpAvailable        = false;
    bool m_loggedRadiation    = false;
    bool m_loggedXp           = false;

  };

}
