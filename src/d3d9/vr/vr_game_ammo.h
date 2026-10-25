#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace dxvk {

  /**
   * \brief A type of ammunition the equipped weapon takes
   */
  struct VrAmmoType {
    /// The ammunition (TESAmmo*)
    const uint8_t* form = nullptr;
    std::string    name;
    /// Rounds of it in the inventory, those in the clip included
    int32_t        held = 0;
  };

  /**
   * \brief The ammunition of the player's weapon, see findings/quick-menu.md §4
   *
   * Only used on the game's main thread, outside of the game's update.
   */
  class VrGameAmmo {

  public:

    /**
     * \brief Whether the game's functions are the expected ones
     *
     * Checked once, logged when they are not.
     */
    static bool available();

    /**
     * \brief The types of ammunition the equipped weapon takes
     *
     * In the order of the weapon's list, with the rounds held of each;
     * one type for a weapon that takes a single one, none without a
     * weapon or for one that uses no ammunition.
     */
    static std::vector<VrAmmoType> types();

    /**
     * \brief The ammunition in the clip, or \c nullptr
     */
    static const uint8_t* current();

    /**
     * \brief Gives the weapon an empty clip of another type
     *
     * As the game's swap does, without its reload: the weapon's hotkey
     * remembers the type, and the clip holds none of it yet (no round is
     * lost, the clip is a view on the inventory).
     *
     * \param [in] ammo The ammunition, one of \ref types
     * \returns \c false if nothing was changed
     */
    static bool select(const uint8_t* ammo);

    /**
     * \brief The game's own ammunition swap: the next type held, reloaded
     *    with the weapon's reload animation
     */
    static void swapToNext();

    /**
     * \brief The name of an ammunition as the game shows it
     */
    static std::string name(const uint8_t* ammo);

  };

}
