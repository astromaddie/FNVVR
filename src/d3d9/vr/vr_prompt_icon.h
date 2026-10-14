#pragma once

#include "vr_types.h"

namespace dxvk {

  /**
   * \brief Shows the button that activates next to the interaction prompt
   *
   * The game shows the gamepad button bound to its Activate control next to
   * the prompt, as the "_xbox_button" trait of a tile of the prompt (see
   * findings/hud.md §9.2). While the prompt is at the left hand, the left
   * hand's reload button activates instead (see VrActionFilter), so the
   * prompt shows the button that reload is bound to: X with the default
   * SteamVR bindings, which is also the gamepad's X.
   *
   * Only used on the game's main thread.
   */
  class VrPromptIcon {

  public:

    /**
     * \brief Checks that the executable is the supported version
     * \returns \c false if the game's code was not found
     */
    bool initialize();

    /**
     * \brief Shows the button of the hand the prompt is at
     *
     * The game sets the icon again when the prompt names another
     * reference, so this is called on every frame the prompt is shown.
     *
     * \param [in] hand Hand the prompt is at
     */
    void show(VrHand hand);

  private:

    bool     m_available = false;
    uint32_t m_trait     = 0;

  };

}
