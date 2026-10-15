#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "vr_game_state.h"

namespace dxvk {

  /**
   * \brief What the developer mode gives
   *
   * Set by \c d3d9.vrDeveloperMode and \c d3d9.vrDeveloperWeapons.
   */
  struct VrDeveloperConfig {
    /// God mode, ammunition and the weapons below after each loading screen
    bool     enabled     = false;
    /// Weapon types to give, bit \c 1 << type (see vr_weapon_types.h)
    uint32_t weaponTypes = 0;
  };

  /**
   * \brief Helps testing the mod: god mode, weapons and unlimited ammunition
   *
   * Enabled by \c d3d9.vrDeveloperMode in dxvk.conf. Each time the game
   * comes back from a loading screen or the main menu, the player gets
   * god mode, as with the console's \c tgm, and one of every playable
   * weapon of the chosen types (\c d3d9.vrDeveloperWeapons) in the loaded
   * files (DLCs included) that is not in the inventory yet. The weapons are
   * added the way the console's \c AddItem adds them, without the message,
   * at full condition. Weapons the game marks as not playable, such as
   * creature weapons, are left out. A weapon dropped on purpose comes back
   * after the next loading screen.
   *
   * Ammunition is unlimited: god mode already keeps the game from using
   * any, but a weapon without ammunition in the inventory cannot fire, so
   * every weapon in the inventory whose ammunition the player has none of
   * gets one clip of it. A weapon picked up later gets its ammunition after
   * the next loading screen. See findings/developer-mode.md.
   *
   * Only used on the game's main thread.
   */
  class VrDeveloperMode {

  public:

    ~VrDeveloperMode();

    /**
     * \brief Checks the game functions it calls
     *
     * \param [in] weaponTypes Weapon types to give, bit \c 1 << type
     * \returns \c nullptr if the executable is not the supported version
     */
    static std::unique_ptr<VrDeveloperMode> install(uint32_t weaponTypes);

    /**
     * \brief Gives god mode, weapons and ammunition on the first frame in game
     *
     * Called once per frame, before the frame is drawn.
     *
     * \param [in] state What the game shows this frame
     */
    void update(VrGameStateKind state);

  private:

    explicit VrDeveloperMode(uint32_t weaponTypes);

    uint32_t m_weaponTypes;

    /// The game showed a loading screen or the main menu since the
    /// weapons were last given
    bool m_pending = true;

    void enableGodMode();

    void addWeaponsAndAmmo();

    /// Adds one clip of the weapon's ammunition if the player has none
    void addAmmo(uint8_t* container, uint8_t* changes, uint8_t* weapon, std::vector<uint8_t*>& added);

  };

}
