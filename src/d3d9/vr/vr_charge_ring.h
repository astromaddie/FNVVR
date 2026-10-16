#pragma once

#include <vector>

#include "../d3d9_include.h"

#include "../../util/com/com_pointer.h"

#include "vr_backend.h"

namespace dxvk {

  /**
   * \brief Shows the charge of a power attack as a ring beside the hand
   *
   * The ring is drawn on the CPU (vrDrawChargeRing) into a system memory
   * texture, copied to a texture the runtime can read, and shown on its
   * own panel (VrPanelId::HudCharge). It is only drawn again when its
   * look changes.
   */
  class VrChargeRing {

  public:

    VrChargeRing(IDirect3DDevice9* device, IVRBackend& backend)
    : m_device(device), m_backend(backend) { }

    ~VrChargeRing();

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

    IDirect3DDevice9*      m_device;
    IVRBackend&            m_backend;

    Com<IDirect3DTexture9> m_staging;
    Com<IDirect3DTexture9> m_texture;
    std::vector<uint32_t>  m_pixels;

    // Look of the ring in the texture, -1 before it is first drawn
    int32_t m_drawnStep = -1;
    bool    m_shown     = false;
    bool    m_failed    = false;

    bool createTextures();

    bool draw(int32_t step, bool charged);

  };

}
