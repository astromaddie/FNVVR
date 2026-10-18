#pragma once

#include "vr_game_space.h"

namespace dxvk {

  /**
   * \brief Camera values saved and restored around an eye render
   */
  struct VrGameCameraState {
    float rotation[9];
    float translation[3];
    float frustum[6];
  };

  /**
   * \brief Access to one of the game's cameras (NiCamera)
   *
   * Reads and writes the camera through its known memory layout and asks the
   * game to refresh the matrices it derives from it. Nothing here knows about
   * VR: callers pass poses and frustums in the game's own frame. The camera
   * is the world camera, or another one such as a rendered menu's.
   */
  class VrGameCamera {

  public:

    /**
     * \brief Looks up the world camera
     * \returns \c false while the game has no scene graph
     */
    bool acquire();

    /**
     * \brief Uses another camera than the world camera
     *
     * \param [in] camera A NiCamera of the game
     */
    void attach(void* camera) {
      m_camera = static_cast<uint8_t*>(camera);
    }

    /**
     * \brief Checks whether a pointer is the camera found by acquire()
     */
    bool isCamera(const void* camera) const { return camera && camera == m_camera; }

    VrGameCameraState save() const;

    /**
     * \brief Writes a saved state back and refreshes the camera
     */
    void restore(const VrGameCameraState& state);

    /**
     * \brief Pose the game computed for the camera this frame
     */
    VrGameCameraPose readPose() const;

    /**
     * \brief Moves the camera and sets its frustum
     *
     * The near and far planes are kept. The rotation is written as
     * the columns forward, up and right, and the cached matrices are
     * refreshed so that the next render uses the new values.
     */
    void apply(const VrGameCameraPose& pose, const VrGameFrustum& frustum);

    /**
     * \brief Sets the near and far planes
     *
     * \param [in] nearPlane Distance of the near plane, in the camera's units
     * \param [in] farPlane Distance of the far plane
     */
    void setDepthRange(float nearPlane, float farPlane);

  private:

    uint8_t* m_camera = nullptr;

    void refresh();

  };

}
