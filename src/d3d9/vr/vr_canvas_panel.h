#pragma once

#include <vector>

#include "../d3d9_include.h"

#include "../../util/com/com_pointer.h"

#include "vr_backend.h"
#include "vr_canvas.h"

namespace dxvk {

  /**
   * \brief Shows an image drawn on the CPU on one of the backend's panels
   *
   * The canvas is copied into a system memory texture, then to a texture
   * the runtime can read, as VrChargeRing does. It is only copied again
   * when it changed.
   */
  class VrCanvasPanel {

  public:

    VrCanvasPanel(IDirect3DDevice9* device, IVRBackend& backend, VrPanelId id)
    : m_device(device), m_backend(backend), m_id(id) { }

    ~VrCanvasPanel();

    /**
     * \brief Copies a new image of the canvas to the panel's texture
     *
     * \returns \c false if the textures could not be made or filled
     */
    bool upload(const VrCanvas& canvas);

    /**
     * \brief Shows the last image uploaded
     *
     * \param [in] pose Panel pose in tracking space
     * \param [in] width Panel width, in metres
     * \returns Whether the backend shows it
     */
    bool show(const VrPose& pose, float width);

    /**
     * \brief Hides the panel
     */
    void hide();

    bool shown() const {
      return m_shown;
    }

  private:

    IDirect3DDevice9*      m_device;
    IVRBackend&            m_backend;
    VrPanelId              m_id;

    Com<IDirect3DTexture9> m_staging;
    Com<IDirect3DTexture9> m_texture;
    uint32_t               m_width  = 0;
    uint32_t               m_height = 0;
    bool                   m_shown  = false;

  };

}
