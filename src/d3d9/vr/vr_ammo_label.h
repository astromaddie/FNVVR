#pragma once

#include <string>

#include "vr_canvas.h"
#include "vr_canvas_panel.h"
#include "vr_game_images.h"

namespace dxvk {

  /**
   * \brief Layout of the label of the ammunition in the left hand
   */
  struct VrAmmoLabelLayout {
    constexpr static uint32_t ImageWidth  = 384;
    constexpr static uint32_t ImageHeight = 96;
    /// Width of the panel, in metres
    constexpr static float    Width       = 0.12f;
    /// Height above the left controller, in metres
    constexpr static float    Lift        = 0.09f;
  };

  /**
   * \brief Draws the label of the ammunition in the left hand
   *
   * Its name and the rounds held of it on a dark plate in the HUD's
   * colour, and below them the button that changes it when another type
   * is held.
   *
   * \param [out] canvas Canvas of the layout's size
   * \param [in] name The ammunition's name
   * \param [in] held Rounds of it held
   * \param [in] more Another type can be chosen
   * \param [in] colour The HUD's colour
   * \param [in] font Font of the text, or \c nullptr to draw no text
   */
  void vrDrawAmmoLabel(VrCanvas& canvas, const std::string& name, int32_t held, bool more,
    const VrColour& colour, const VrCanvasFont* font);

  /**
   * \brief The label above the left hand while it holds ammunition taken
   *    behind the back, see VrManualReload::cycleAmmo
   */
  class VrAmmoLabel {

  public:

    VrAmmoLabel(IDirect3DDevice9* device, IVRBackend& backend, VrGameImages& images);

    /**
     * \brief Shows the label
     *
     * \param [in] head Head pose in tracking space
     * \param [in] hand Left controller position in tracking space
     * \param [in] name The ammunition's name
     * \param [in] held Rounds of it held
     * \param [in] more Another type can be chosen
     */
    void show(const VrPose& head, const VrVector3& hand, const std::string& name, int32_t held, bool more);

    void hide();

  private:

    VrCanvasPanel  m_panel;
    VrCanvas       m_canvas;
    VrGameImages&  m_images;

    // What the texture shows
    std::string    m_name;
    int32_t        m_held   = -1;
    bool           m_more   = false;
    bool           m_drawn  = false;
    bool           m_failed = false;

  };

}
