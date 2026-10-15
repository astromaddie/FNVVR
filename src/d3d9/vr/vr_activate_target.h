#pragma once

#include "vr_types.h"

namespace dxvk {

  /**
   * \brief Finds what the player would activate
   *
   * The game picks the reference along the hands (VrHandActivatePick), or
   * under its crosshair, which follows the head, when the hands are not
   * tracked, and the HUD's interaction prompt names it. When the game
   * picked along the view, the prompt is anchored to the hand nearest the
   * reference's position, see findings/hud.md §9 and §10.
   *
   * Only used on the game's main thread.
   */
  class VrActivateTarget {

  public:

    /**
     * \brief Checks that the executable is the supported version
     * \returns \c false if the game's function was not found
     */
    bool initialize();

    /**
     * \brief Finds the centre of the reference the player would activate
     *
     * \param [out] position Centre of the reference's 3D model in the
     *    game's world, in game units
     * \returns \c false if there is no such reference or it is not loaded
     */
    bool find(VrVector3& position) const;

  private:

    bool m_available = false;

  };

}
