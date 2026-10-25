#include <algorithm>
#include <cmath>

#include "../../util/log/log.h"

#include "vr_charge_ring.h"
#include "vr_power_attack.h"

namespace dxvk {

  namespace {

    // Steps the fill is drawn in; one more stands for the charged ring
    constexpr int32_t FillSteps = 64;
    constexpr int32_t ChargedStep = FillSteps + 1;

  }


  VrChargeRing::VrChargeRing(VrPainter& painter, IVRBackend& backend)
  : m_panel(painter, backend, VrPanelId::HudCharge),
    m_list(VrChargeRingLayout::ImageSize, VrChargeRingLayout::ImageSize) { }


  void VrChargeRing::show(const VrPose& pose, float fill, bool charged) {
    if (m_failed)
      return;

    int32_t step = charged ? ChargedStep
      : int32_t(std::lround(std::clamp(fill, 0.0f, 1.0f) * float(FillSteps)));

    if (step != m_drawnStep) {
      vrPaintChargeRing(m_list, float(std::min(step, FillSteps)) / float(FillSteps), charged);

      if (!m_panel.paint(m_list)) {
        Logger::err("VR: Failed to draw the power attack's charge ring, it is not shown");
        m_failed = true;
        hide();
        return;
      }

      m_drawnStep = step;
    }

    m_panel.show(pose, VrChargeRingLayout::Width);
  }


  void VrChargeRing::hide() {
    m_panel.hide();
  }

}
