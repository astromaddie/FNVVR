#pragma once

#include <memory>

#include "vr_types.h"

namespace dxvk {

  /**
   * \brief Centres the sky on the eye being drawn
   *
   * The game moves the sky root (atmosphere, stars, clouds, sun, moons) and
   * the weather root (precipitation, sun glare) to the camera root's
   * position at the start of every world render, see findings/sky.md. The
   * sky is a dome of finite size, which only looks infinitely far away when
   * the viewpoint is at its centre. The eyes are away from the camera root
   * by half the distance between the eyes and by the head's own movement,
   * so each eye saw the dome as a nearby ceiling that moved against the head.
   *
   * Right after the game has placed both roots, they are moved to the
   * position of the eye being drawn. Both eyes then see the sky in the same
   * directions, as for objects at infinity.
   *
   * Only used on the game's main thread.
   */
  class VrSkyAnchor {

  public:

    ~VrSkyAnchor();

    /**
     * \brief Redirects the calls that place the sky
     * \returns \c nullptr if the executable is not the supported version
     */
    static std::unique_ptr<VrSkyAnchor> install();

    /**
     * \brief Sets the eye being drawn
     *
     * \param [in] eyePosition Position of the eye in the game world, read
     *    when the game places the sky, or \c nullptr outside of an eye's
     *    render, where the game's placement is kept
     */
    void setEye(const VrVector3* eyePosition) {
      m_eyePosition = eyePosition;
    }

    /**
     * \brief Starts a world render
     *
     * Calls the game's Main::DrawWorld_Init, then centres the sky on the
     * eye being drawn. The game's own calls are redirected here; the VR
     * layer's own world renders call it as well.
     *
     * \param [in] main The game's Main object
     * \returns The sun to draw, or \c nullptr
     */
    static void* __fastcall drawWorldInit(
            void*                 main,
            void*                 unused = nullptr);

  private:

    VrSkyAnchor() = default;

    const VrVector3* m_eyePosition = nullptr;

    void moveRoots() const;

  };

}
