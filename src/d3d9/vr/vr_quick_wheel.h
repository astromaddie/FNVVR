#pragma once

#include <array>
#include <cstdint>

#include "vr_canvas.h"
#include "vr_canvas_panel.h"
#include "vr_game_hotkeys.h"
#include "vr_game_images.h"
#include "vr_quick_wheel_image.h"

namespace dxvk {

  /**
   * \brief What the quick wheel is for
   */
  enum class VrQuickWheelMode {
    /// In game: the hotkeys are used
    Use,
    /// In the Pip-Boy's inventory: the highlighted item is given a hotkey
    Assign,
  };

  /**
   * \brief What the hands do with the quick wheel in a frame
   *
   * Poses in tracking space.
   */
  struct VrQuickWheelInput {
    /// The wheel's control is held: the right stick pushed down in game,
    /// its click in the Pip-Boy's inventory
    bool             held = false;
    VrQuickWheelMode mode = VrQuickWheelMode::Use;
    /// The head and the right controller, or \c nullptr if not tracked
    const VrPose*    head = nullptr;
    const VrPose*    hand = nullptr;
    /// The ammunition is changed on the magazine (VrManualReload): the
    /// ammunition cell only shows it
    bool             manualReload = false;
  };

  enum class VrQuickWheelEvent {
    None,
    /// The wheel opened
    Opened,
    /// The wheel could not open: nothing to give a hotkey to, or the game
    /// refused the item
    Refused,
    /// Another cell is picked
    Picked,
    /// The wheel closed on a cell, which was used or given
    Used,
    /// The wheel closed on a cell the game refused, or on an empty one
    Failed,
    /// The wheel closed with no cell picked
    Closed,
  };

  /**
   * \brief The quick wheel of hotkeys, at the right hand
   *
   * See findings/quick-menu.md §6. Holding the wheel's control shows a
   * ring of eight cells where the right hand is, facing the head: the
   * seven hotkeys and, at the top, the ammunition. Moving the hand
   * towards a cell picks it; letting go of the control uses it (the game's
   * hotkey wheel equips, unequips or uses the item; the ammunition cell
   * swaps to the next type held, unless the ammunition is changed on the
   * magazine) or, in the Pip-Boy's inventory, gives its slot to the item
   * highlighted when the wheel opened. Letting go with no cell picked does
   * nothing.
   *
   * A weapon is drawn when it is chosen: picking the weapon already
   * equipped draws it if it is holstered instead of unequipping it.
   *
   * Only used on the game's main thread, outside of the game's update.
   */
  class VrQuickWheel {

  public:

    VrQuickWheel(IDirect3DDevice9* device, IVRBackend& backend, VrGameImages& images);

    VrQuickWheelEvent update(const VrQuickWheelInput& input);

    /**
     * \brief Closes the wheel at once, using nothing
     */
    void close();

    bool isOpen() const {
      return m_open;
    }

    /**
     * \brief What the last closing on a cell asked of the weapon
     */
    bool drawsWeapon() const {
      return m_drawsWeapon;
    }

  private:

    VrCanvasPanel     m_panel;
    VrCanvas          m_canvas;
    VrGameImages&     m_images;

    bool              m_open        = false;
    bool              m_held        = false;
    bool              m_failed      = false;
    bool              m_drawsWeapon = false;
    VrQuickWheelMode  m_mode        = VrQuickWheelMode::Use;

    // Where the wheel stands, and the hotkeys it shows
    VrPose            m_pose;
    std::array<VrHotKeyItem, VrGame::HotKeySlotCount> m_items;
    VrQuickWheelView  m_view;
    bool              m_drawn = false;

    // The ammunition cell swaps the ammunition
    bool              m_swapsAmmo = false;

    // The item given a hotkey, highlighted when the wheel opened
    uint8_t*          m_entry = nullptr;
    VrHotKeyItem      m_entryItem;

    bool open(const VrQuickWheelInput& input);

    void fillView(const VrQuickWheelInput& input);

    VrQuickWheelEvent choose(int32_t cell);

    void show();

  };

}
