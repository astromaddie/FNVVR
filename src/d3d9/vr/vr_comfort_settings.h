#pragma once

#include <memory>

namespace dxvk {

  /**
   * \brief Turns off the game's cameras that leave the first person
   *
   * Two cameras of the game move the view away from the head:
   *
   * - The idle camera: after some time without input, the game switches to
   *   third person and turns the camera around the player (auto vanity
   *   mode). The game's own bDisableAutoVanityMode:General INI setting
   *   turns it off and is set here.
   * - The kill camera: when the player kills an actor outside V.A.T.S., the
   *   game may show the kill in slow motion. Its mode (none, player,
   *   cinematic) is read from iKillCamera:GamePlay into the player when the
   *   player is created; the cinematic mode, the default, plays the kill
   *   through V.A.T.S. with third-person cameras. The player's mode is set
   *   to the player camera, the slow motion in first person, through the
   *   game's own PlayerCharacter::SetKillCamModeCallback, which ends a
   *   kill camera already running. The INI value is left alone, so the
   *   game's preference file keeps the user's choice.
   *
   * The cameras of V.A.T.S. itself are not changed here. See
   * findings/camera.md §7.
   *
   * Only used on the game's main thread.
   */
  class VrComfortSettings {

  public:

    /**
     * \brief Checks the game's code and turns the idle camera off
     *
     * Called once the game has read its INI files.
     *
     * \returns \c nullptr if the executable is not the supported version
     */
    static std::unique_ptr<VrComfortSettings> install();

    /**
     * \brief Keeps the kill camera in first person
     *
     * Called once per frame, before the frame is drawn. The player is
     * created after the device, and a value set by a script or the console
     * is put back.
     */
    void update();

  private:

    VrComfortSettings() = default;

    bool m_loggedKillCam = false;

  };

}
