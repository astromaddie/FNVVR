#pragma once

#include "vr_pipboy_gaze.h"

namespace dxvk {

  /**
   * \brief Opens and closes the game's Pip-Boy
   *
   * Reads the Pip-Boy's state machine and makes the same requests as the
   * game's Pip-Boy control, so the game raises the arm, enters menu mode
   * and checks whether the player may open it (V.A.T.S., controls
   * disabled, ...). See findings/pipboy.md §1.
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

    /**
     * \brief Asks the game to close the Pip-Boy, if it is open
     */
    void close() const;

  private:

    bool m_available = false;

  };

}
