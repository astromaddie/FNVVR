#pragma once

#include <cstdint>
#include <memory>

namespace dxvk {

  /**
   * \brief Keeps the player's weapon drawn, without the draw and holster animations
   *
   * In VR the weapon is held in the right hand, so a holstered weapon only
   * hides it and the draw animation only delays the first shot. See
   * findings/weapon.md §7.
   *
   * - The draw and holster animations are skipped: the game's wait on the
   *   weapon section's Equip or Unequip group is reported as finished as
   *   soon as the group has started, so the game places the weapon itself.
   *   The holster animation still plays before sitting or sleeping, which
   *   wait for it to end.
   * - Holding the ready/reload control no longer holsters the weapon.
   * - Every frame in game, the player wants the weapon drawn again unless
   *   the game holsters it on purpose: using furniture (sitting, sleeping,
   *   including the walk up to it), fighting controls disabled by a script,
   *   dead. The game itself refuses to draw while swimming or aiming down
   *   the sights.
   *
   * Only used on the game's main thread.
   */
  class VrWeaponDrawn {

  public:

    ~VrWeaponDrawn();

    /**
     * \brief Redirects the equip wait and the holster control
     * \returns \c nullptr if the executable is not the supported version
     */
    static std::unique_ptr<VrWeaponDrawn> install();

    /**
     * \brief Draws the player's weapon if it is holstered without a reason
     *
     * Called once per frame while the game runs without a menu.
     */
    void update();

  private:

    VrWeaponDrawn() = default;

    bool m_loggedEquip     = false;
    bool m_loggedHolster   = false;
    bool m_loggedDraw      = false;
    bool m_loggedFurniture = false;

    static uint32_t __fastcall equipWaitHook(
            uint8_t*              animData,
            void*                 unused,
            int32_t               section);

    static void __fastcall holsterInputHook(
            uint8_t*              actor,
            void*                 unused,
            bool                  drawn);

  };

}
