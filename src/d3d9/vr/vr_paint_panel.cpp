#include "vr_d3d9_bridge.h"
#include "vr_paint_panel.h"

namespace dxvk {

  VrPaintPanel::~VrPaintPanel() {
    hide();
  }


  bool VrPaintPanel::paint(const VrPaintList& list) {
    uint32_t width  = list.width();
    uint32_t height = list.height();

    if (m_texture == nullptr || width != m_width || height != m_height) {
      m_texture = nullptr;
      m_drawn   = false;

      if (FAILED(m_painter.device()->CreateTexture(width, height, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
            D3DPOOL_DEFAULT, &m_texture, nullptr))) {
        m_texture = nullptr;
        return false;
      }

      m_width  = width;
      m_height = height;
    }

    m_drawn = m_painter.paint(m_texture.ptr(), list, true);
    return m_drawn;
  }


  bool VrPaintPanel::show(const VrPose& pose, float width) {
    if (!m_drawn)
      return false;

    m_shown = VrD3D9Bridge::submitPanel(m_painter.device(), m_backend, m_id, m_texture.ptr(),
      pose, width, VrPanelAnchor::Room, true);
    return m_shown;
  }


  void VrPaintPanel::hide() {
    if (!m_shown)
      return;

    m_backend.hidePanel(m_id);
    m_shown = false;
  }

}
