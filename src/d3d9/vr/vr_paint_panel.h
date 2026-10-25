#pragma once

#include "vr_backend.h"
#include "vr_painter.h"

namespace dxvk {

  /**
   * \brief Shows an image drawn by the GPU on one of the backend's panels
   *
   * The paint list is drawn into a render target texture the runtime
   * reads, with premultiplied alpha. It is only drawn again when it
   * changed.
   */
  class VrPaintPanel {

  public:

    VrPaintPanel(VrPainter& painter, IVRBackend& backend, VrPanelId id)
    : m_painter(painter), m_backend(backend), m_id(id) { }

    ~VrPaintPanel();

    /**
     * \brief Draws a new image of the panel
     *
     * \param [in] list What the image shows; its size is the image's
     * \returns \c false if the texture could not be made or drawn
     */
    bool paint(const VrPaintList& list);

    /**
     * \brief Shows the last image drawn
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

    VrPainter&             m_painter;
    IVRBackend&            m_backend;
    VrPanelId              m_id;

    Com<IDirect3DTexture9> m_texture;
    uint32_t               m_width  = 0;
    uint32_t               m_height = 0;
    bool                   m_drawn  = false;
    bool                   m_shown  = false;

  };

}
