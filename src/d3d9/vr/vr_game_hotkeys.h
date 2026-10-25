#pragma once

#include <array>
#include <cstdint>
#include <string>

#include "vr_game_addresses.h"

namespace dxvk {

  /**
   * \brief An item of the player's inventory, as the quick wheel shows it
   */
  struct VrHotKeyItem {
    bool           present  = false;
    std::string    name;
    uint32_t       count    = 0;
    /// Path of its icon, relative to Data\Textures
    std::string    icon;
    bool           worn     = false;
    /// Its form (TESForm*), compared but not dereferenced, and its type
    const uint8_t* form     = nullptr;
    uint8_t        formType = 0;
  };

  /**
   * \brief The player's hotkeys, through the game's own hotkey wheel
   *
   * See findings/quick-menu.md §1-3. The game keeps the hotkeys in the
   * inventory; the HUD's wheel equips them as the gamepad's D-pad does, and
   * the Pip-Boy inventory's wheel assigns them. Slot 1 is the ammunition
   * swap, never a hotkey.
   *
   * Only used on the game's main thread, outside of the game's update.
   */
  class VrGameHotKeys {

  public:

    /**
     * \brief Whether the game's functions are the expected ones
     *
     * Checked once, logged when they are not.
     */
    static bool available();

    /**
     * \brief Reads the item of every slot
     *
     * \param [out] items Items by slot; slot 1 is left empty
     */
    static void read(std::array<VrHotKeyItem, VrGame::HotKeySlotCount>& items);

    /**
     * \brief Equips, unequips or uses the item of a slot, as the game's
     *    hotkey wheel does
     *
     * The game toggles an item: one equipped is unequipped. It refuses to
     * change a weapon or armour while the player's anim action runs (an
     * attack, a reload...), which is refused here first.
     *
     * \param [in] slot The slot
     * \returns \c false if nothing was done
     */
    static bool use(uint32_t slot);

    /**
     * \brief The item highlighted in the Pip-Boy's inventory
     *
     * \returns Its inventory entry (owned by the menu), or \c nullptr if
     *    the inventory is not the top menu or no item is highlighted
     */
    static uint8_t* highlighted();

    /**
     * \brief Reads an inventory entry
     */
    static VrHotKeyItem describe(uint8_t* entry);

    /**
     * \brief Whether the highlighted item can be given a hotkey
     *
     * When it cannot, the game's own message tells why (a broken item, an
     * item that cannot be equipped or a book).
     */
    static bool canAssign(uint8_t* entry);

    /**
     * \brief Gives a slot to an item of the Pip-Boy's inventory
     *
     * As the inventory's wheel does with the gamepad: the slot's previous
     * item loses it, the pick up sound plays and the list is refreshed.
     *
     * \param [in] entry The item, the highlighted one when the choice
     *    started
     * \param [in] slot The slot, not the ammunition swap
     * \returns \c false if the inventory is no longer open
     */
    static bool assign(uint8_t* entry, uint32_t slot);

  };

}
