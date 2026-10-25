#pragma once

#include "vr_backend.h"
#include "vr_paint_panel.h"

namespace dxvk {

  /**
   * \brief Shows the charge of a power attack as a ring beside the hand
   *
   * The ring (vrPaintChargeRing) is drawn by the GPU and shown on its own
   * panel (VrPanelId::HudCharge). It is only drawn again when its look
   * changes.
   */
  class VrChargeRing {

  public:

    VrChargeRing(VrPainter& painter, IVRBackend& backend);

    /**
     * \brief Shows the ring
     *
     * \param [in] pose Panel pose in tracking space
     * \param [in] fill How full the ring is, from 0 to 1
     * \param [in] charged Whether the attack is charged
     */
    void show(const VrPose& pose, float fill, bool charged);

    /**
     * \brief Hides the ring
     */
    void hide();

  private:

    VrPaintPanel m_panel;
    VrPaintList  m_list;

    // Look of the ring on the panel, -1 before it is first drawn
    int32_t m_drawnStep = -1;
    bool    m_failed    = false;

  };

}
