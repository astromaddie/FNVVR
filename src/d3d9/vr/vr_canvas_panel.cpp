#include <cstring>

#include "vr_canvas_panel.h"
#include "vr_d3d9_bridge.h"

namespace dxvk {

  VrCanvasPanel::~VrCanvasPanel() {
    hide();
  }


  bool VrCanvasPanel::upload(const VrCanvas& canvas) {
    uint32_t width  = canvas.width();
    uint32_t height = canvas.height();

    if (m_texture == nullptr || width != m_width || height != m_height) {
      m_staging = nullptr;
      m_texture = nullptr;

      if (FAILED(m_device->CreateTexture(width, height, 1, 0, D3DFMT_A8R8G8B8,
            D3DPOOL_SYSTEMMEM, &m_staging, nullptr))
       || FAILED(m_device->CreateTexture(width, height, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
            D3DPOOL_DEFAULT, &m_texture, nullptr))) {
        m_staging = nullptr;
        m_texture = nullptr;
        return false;
      }

      m_width  = width;
      m_height = height;
    }

    D3DLOCKED_RECT locked = { };

    if (FAILED(m_staging->LockRect(0, &locked, nullptr, 0)))
      return false;

    for (uint32_t y = 0; y < height; y++) {
      std::memcpy(static_cast<uint8_t*>(locked.pBits) + y * locked.Pitch,
        canvas.pixels() + size_t(y) * width, width * sizeof(uint32_t));
    }

    m_staging->UnlockRect(0);
    return SUCCEEDED(m_device->UpdateTexture(m_staging.ptr(), m_texture.ptr()));
  }


  bool VrCanvasPanel::show(const VrPose& pose, float width) {
    if (m_texture == nullptr)
      return false;

    m_shown = VrD3D9Bridge::submitPanel(m_device, m_backend, m_id, m_texture.ptr(), pose, width, VrPanelAnchor::Room);
    return m_shown;
  }


  void VrCanvasPanel::hide() {
    if (!m_shown)
      return;

    m_backend.hidePanel(m_id);
    m_shown = false;
  }

}
