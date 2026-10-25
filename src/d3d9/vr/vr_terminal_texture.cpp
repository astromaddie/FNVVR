#include "vr_terminal_texture.h"

namespace dxvk {

  namespace {

    static_assert(VrTerminalTexture::KeyTops.left   >= VrTerminalKeyboard::Plate.left
               && VrTerminalTexture::KeyTops.top    >= VrTerminalKeyboard::Plate.top
               && VrTerminalTexture::KeyTops.right  <= VrTerminalKeyboard::Plate.right
               && VrTerminalTexture::KeyTops.bottom <= VrTerminalKeyboard::Plate.bottom,
      "The key tops must lie inside the keyboard image");

  }


  bool VrTerminalTexture::initialize(const VrImage& texture) {
    if (texture.width < uint32_t(VrTerminalKeyboard::Plate.right) + 1
     || texture.height < uint32_t(VrTerminalKeyboard::Plate.bottom) + 1)
      return false;

    m_texture = texture;
    return true;
  }


  void VrTerminalTexture::paint(VrPaintList& list, const VrTerminalImage& keyboard,
      const VrTerminalKeyboardView& view) const {
    list.clear();

    // The whole texture, enlarged with bilinear filtering
    list.setBlend(VrPaintBlend::Copy);
    list.drawImage(m_texture, VrPaintImageUse::Picture, 0.0f, 0.0f, float(width()), float(height()),
      0.0f, 0.0f, 1.0f, 1.0f, { 1.0f, 1.0f, 1.0f, 1.0f });

    const VrTexelRect& plate = VrTerminalKeyboard::Plate;
    keyboard.paint(list, view, plate.left * float(Scale), plate.top * float(Scale), &KeyTops, true);
  }

}
