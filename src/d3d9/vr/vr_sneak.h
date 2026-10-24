#pragma once

#include <memory>

#include "vr_game_space.h"
#include "vr_sneak_rules.h"

namespace dxvk {

  /**
   * \brief How the player sneaks
   */
  struct VrSneakConfig {
    /// Sneak by crouching instead of with the sneak button
    bool           physical = false;
    VrCrouchConfig crouch;
  };

  /**
   * \brief Makes the player sneak by crouching
   *
   * With the sneak button (the default), the button toggles the game's
   * sneaking and the game lowers its camera, as with a gamepad: nothing is
   * done here.
   *
   * With the physical crouch, the button no longer sneaks. The game sneaks
   * while the head is held low (VrPhysicalCrouch), through the game's own
   * sneak control (VrSneakToggle), so that the game's checks, sounds and
   * HUD apply as for the button. The view already follows the head down,
   * so the camera is kept at the standing height while the game sneaks
   * (VrSneakCamera). See findings/sneak.md.
   *
   * Only used on the game's main thread.
   */
  class VrSneak {

  public:

    /**
     * \brief Checks the game's code
     * \returns \c nullptr if the executable is not the supported version
     */
    static std::unique_ptr<VrSneak> install();

    void setConfig(const VrSneakConfig& config);

    /**
     * \brief Whether the player sneaks by crouching
     */
    bool physical() const {
      return m_config.physical;
    }

    /**
     * \brief Takes the head's height for a frame
     *
     * Called once per frame, before the next player update.
     *
     * \param [in] headHeight Height of the head in tracking space in
     *    metres, or \c nullptr when the head is not tracked
     * \param [in] inGame The game is played, with no menu open
     * \param [in] dt Duration of a frame in seconds
     */
    void update(const float* headHeight, bool inGame, float dt);

    /**
     * \brief Whether the game's sneak control is held for the next player update
     */
    bool holdsSneakControl() const {
      return m_holdsControl;
    }

    /**
     * \brief Takes the camera the game computed for this frame
     *
     * Called once per frame, before adjustCamera.
     *
     * \param [in] pose The camera, or \c nullptr when it does not follow
     *    the player's head, such as in V.A.T.S. or on furniture
     * \param [in] dt Duration of a frame in seconds
     */
    void measureCamera(const VrGameCameraPose* pose, float dt);

    /**
     * \brief Raises a camera pose by what the game's sneaking lowered
     *
     * \param [in,out] pose Camera pose the game computed this frame
     */
    void adjustCamera(VrGameCameraPose& pose) const;

  private:

    VrSneak();

    VrSneakConfig    m_config;
    VrPhysicalCrouch m_crouch;
    VrSneakToggle    m_toggle;
    VrSneakCamera    m_camera;

    bool             m_holdsControl = false;
    bool             m_wasCrouched  = false;
    bool             m_loggedRaise  = false;

    static bool isSneaking(uintptr_t player);

  };

}
