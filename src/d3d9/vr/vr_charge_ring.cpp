#include <algorithm>
#include <cmath>
#include <cstring>

#include "../../util/log/log.h"

#include "vr_charge_ring.h"
#include "vr_d3d9_bridge.h"
#include "vr_power_attack.h"

namespace dxvk {

  namespace {

    // Steps the fill is drawn in; one more stands for the charged ring
    constexpr int32_t FillSteps = 64;
    constexpr int32_t ChargedStep = FillSteps + 1;

  }


  VrChargeRing::~VrChargeRing() {
    hide();
  }


  void VrChargeRing::show(const VrPose& pose, float fill, bool charged) {
    if (m_failed)
      return;

    int32_t step = charged ? ChargedStep
      : int32_t(std::lround(std::clamp(fill, 0.0f, 1.0f) * float(FillSteps)));

    if (step != m_drawnStep && !draw(step, charged)) {
      Logger::err("VR: Failed to draw the power attack's charge ring, it is not shown");
      m_failed = true;
      hide();
      return;
    }

    m_shown = VrD3D9Bridge::submitPanel(m_device, m_backend, VrPanelId::HudCharge, m_texture.ptr(), pose, VrChargeRingLayout::Width, VrPanelAnchor::Room);
  }


  void VrChargeRing::hide() {
    if (!m_shown)
      return;

    m_backend.hidePanel(VrPanelId::HudCharge);
    m_shown = false;
  }


  bool VrChargeRing::createTextures() {
    const UINT size = VrChargeRingLayout::ImageSize;

    if (FAILED(m_device->CreateTexture(size, size, 1, 0, D3DFMT_A8R8G8B8,
          D3DPOOL_SYSTEMMEM, &m_staging, nullptr))
     || FAILED(m_device->CreateTexture(size, size, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
          D3DPOOL_DEFAULT, &m_texture, nullptr))) {
      m_staging = nullptr;
      m_texture = nullptr;
      return false;
    }

    m_pixels.resize(size * size);
    return true;
  }


  bool VrChargeRing::draw(int32_t step, bool charged) {
    if (m_texture == nullptr && !createTextures())
      return false;

    const uint32_t size = VrChargeRingLayout::ImageSize;
    vrDrawChargeRing(m_pixels.data(), size, float(std::min(step, FillSteps)) / float(FillSteps), charged);

    D3DLOCKED_RECT locked = { };

    if (FAILED(m_staging->LockRect(0, &locked, nullptr, 0)))
      return false;

    for (uint32_t y = 0; y < size; y++) {
      std::memcpy(static_cast<uint8_t*>(locked.pBits) + y * locked.Pitch,
        &m_pixels[y * size], size * sizeof(uint32_t));
    }

    m_staging->UnlockRect(0);

    if (FAILED(m_device->UpdateTexture(m_staging.ptr(), m_texture.ptr())))
      return false;

    m_drawnStep = step;
    return true;
  }

}
