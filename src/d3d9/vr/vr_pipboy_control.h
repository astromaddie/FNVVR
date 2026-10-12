#pragma once

#include "vr_pipboy_gaze.h"

namespace dxvk {

  /**
   * \brief Opens the game's Pip-Boy and reads its state
   *
   * Reads the Pip-Boy's state machine and makes the same request as the
   * game's Pip-Boy control, so the game raises the arm, enters menu mode
   * and checks whether the player may open it (V.A.T.S., controls
   * disabled, ...). See findings/pipboy.md §1. The Pip-Boy is closed with
   * the menu's back button instead (VrControls::setMenuBack).
   *
   * Only used on the game's main thread, outside InterfaceManager::Update.
   */
  class VrPipBoyControl {

  public:

    /**
     * \brief Checks that the executable is the supported version
     * \returns \c false if the Pip-Boy cannot be driven
     */
    bool initialize();

    /**
     * \brief Reads the Pip-Boy's state
     *
     * Reports \c Changing when the state cannot be read, so that nothing
     * is requested.
     */
    VrPipBoyState state() const;

    /**
     * \brief Asks the game to open the Pip-Boy on its last tab
     */
    void open() const;

  private:

    bool m_available = false;

  };

}
