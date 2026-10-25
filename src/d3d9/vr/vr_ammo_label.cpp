#include "../../util/log/log.h"

#include "vr_ammo_label.h"
#include "vr_math.h"

namespace dxvk {

  namespace {

    const VrColour Plate = { 0.0f, 0.0f, 0.0f, 0.7f };

    // Steepest the label tilts to face the eyes, in radians (60 degrees)
    constexpr float MaxPitch = 1.05f;

    // The button that changes the ammunition, the left controller's X
    constexpr char NextHint[] = "X >";

  }


  void vrDrawAmmoLabel(VrCanvas& canvas, const std::string& name, int32_t held, bool more,
      const VrColour& colour, const VrCanvasFont* font) {
    float width  = float(canvas.width());
    float height = float(canvas.height());

    canvas.clear();
    canvas.fillRoundedRect(0.0f, 0.0f, width, height, 0.2f * height, Plate);

    if (!font)
      return;

    std::string text = held >= 0 ? name + " (" + std::to_string(held) + ")" : name;
    float lineY = more ? 0.36f * height : 0.5f * height;

    canvas.drawTextBlock(*font, text, 0.5f * width, lineY, 0.92f * width, 1.0f, 0.6f, 1, colour);

    if (more)
      canvas.drawTextBlock(*font, NextHint, 0.5f * width, 0.76f * height, 0.5f * width, 0.8f, 0.6f, 1, colour.faded(0.7f));
  }


  VrAmmoLabel::VrAmmoLabel(IDirect3DDevice9* device, IVRBackend& backend, VrGameImages& images)
  : m_panel(device, backend, VrPanelId::HudAmmo),
    m_canvas(VrAmmoLabelLayout::ImageWidth, VrAmmoLabelLayout::ImageHeight),
    m_images(images) { }


  void VrAmmoLabel::show(const VrPose& head, const VrVector3& hand, const std::string& name, int32_t held, bool more) {
    if (m_failed)
      return;

    if (!m_drawn || name != m_name || held != m_held || more != m_more) {
      vrDrawAmmoLabel(m_canvas, name, held, more, VrGameImages::hudColour(), m_images.font());

      if (!m_panel.upload(m_canvas)) {
        Logger::err("VR: Failed to draw the ammunition label, it is not shown");
        m_failed = true;
        hide();
        return;
      }

      m_name  = name;
      m_held  = held;
      m_more  = more;
      m_drawn = true;
    }

    VrVector3 centre = hand + VrVector3 { 0.0f, VrAmmoLabelLayout::Lift, 0.0f };
    VrPose pose = vrComputePanelPoseTowards(head, centre, vrLength(centre - head.position), MaxPitch);
    pose.position = centre;

    m_panel.show(pose, VrAmmoLabelLayout::Width);
  }


  void VrAmmoLabel::hide() {
    m_panel.hide();
  }

}
