#include <cmath>
#include <cstring>

#include "../../util/log/log.h"

#include "vr_console_terminal.h"
#include "vr_d3d9_bridge.h"
#include "vr_game_console.h"
#include "vr_math.h"
#include "vr_terminal_keyboard.h"

namespace dxvk {

  namespace {

    // Steepest the terminal tilts back to face the eyes, in radians (45 degrees)
    constexpr float MaxPitch = 0.7853982f;

    // A light tick in the hand that typed a key
    constexpr float   KeyHaptic         = 0.15f;
    constexpr int64_t KeyHapticDuration = 15'000'000;

  }


  VrConsoleTerminal::VrConsoleTerminal(IDirect3DDevice9* device, IVRBackend& backend)
  : m_device(device), m_backend(backend) {
    m_placement.width  = Width;
    m_placement.height = Width * float(VrTerminalLayout::ImageHeight) / float(VrTerminalLayout::ImageWidth);
  }


  VrConsoleTerminal::~VrConsoleTerminal() {
    hide();
  }


  void VrConsoleTerminal::update(const VrPose& head, const VrPointerState& pointer, bool back, float dt) {
    if (m_failed || !VrGameConsole::isOpen()) {
      hide();
      return;
    }

    if (!m_shown) {
      place(head);
      m_button    = pointer.button;
      m_shift     = false;
      m_blinkTime = 0.0f;
      m_pressTime = 0.0f;
      m_view.pressedKey = -1;
      Logger::info("VR: The console is shown on the terminal panel");
    }

    int32_t hovered = pointer.cursorActive ? VrTerminalLayout::findKey(pointer.cursor) : -1;

    // A key types when the trigger is pressed on it
    bool pressed = pointer.button && !m_button;
    m_button = pointer.button;

    if (pressed && hovered >= 0)
      press(hovered, pointer);

    // The wheel stick scrolls the output a page per notch
    for (int32_t notch = pointer.wheel; notch >= VrMenuPointer::WheelNotch; notch -= VrMenuPointer::WheelNotch)
      VrGameConsole::pressKey(VrConsoleKey::PageUp);

    for (int32_t notch = pointer.wheel; notch <= -VrMenuPointer::WheelNotch; notch += VrMenuPointer::WheelNotch)
      VrGameConsole::pressKey(VrConsoleKey::PageDown);

    if (back)
      VrGameConsole::requestClose();

    m_blinkTime += dt;
    m_pressTime  = std::max(m_pressTime - dt, 0.0f);

    if (m_pressTime <= 0.0f)
      m_view.pressedKey = -1;

    m_view.hoveredKey  = hovered;
    m_view.shift       = m_shift;
    m_view.cursorShown = std::fmod(m_blinkTime, 2.0f * BlinkTime) < BlinkTime;

    VrGameConsole::readOutput(m_view.output, VrTerminalLayout::OutputLines);
    VrGameConsole::readInput(m_view.input, m_view.cursor);

    bool changed = m_image.update(m_view);

    if ((changed || !m_shown) && !upload()) {
      Logger::err("VR: Failed to draw the console terminal, the console is not shown in the headset");
      m_failed = true;
      hide();
      return;
    }

    if (changed || !m_shown) {
      m_shown = VrD3D9Bridge::submitPanel(m_device, m_backend, VrPanelId::Terminal,
        m_texture.ptr(), m_placement.pose, Width, VrPanelAnchor::Room);
    }
  }


  void VrConsoleTerminal::hide() {
    if (!m_shown)
      return;

    m_backend.hidePanel(VrPanelId::Terminal);
    m_shown = false;
  }


  void VrConsoleTerminal::place(const VrPose& head) {
    // In front of the head along its heading, then lowered and tilted back
    // to face the eyes, as a terminal on a desk
    VrPose level = vrComputePanelPose(head, Distance);
    VrVector3 centre = level.position;
    centre.y -= Drop;

    m_placement.pose = vrComputePanelPoseTowards(head, centre, Distance, MaxPitch);
  }


  void VrConsoleTerminal::press(int32_t index, const VrPointerState& pointer) {
    uint32_t count = 0;
    const VrTerminalKey& key = VrTerminalKeyboard::keys(count)[index];

    switch (key.kind) {
      case VrTerminalKeyKind::Char:
        VrGameConsole::pressKey(VrTerminalKeyboard::code(key, m_shift));
        m_shift = false;
        break;

      case VrTerminalKeyKind::Edit:
        VrGameConsole::pressKey(VrTerminalKeyboard::code(key, m_shift));
        break;

      case VrTerminalKeyKind::Shift:
        m_shift = !m_shift;
        break;

      case VrTerminalKeyKind::Close:
        VrGameConsole::requestClose();
        break;
    }

    // The cursor stays lit while typing
    m_blinkTime = 0.0f;
    m_pressTime = PressTime;
    m_view.pressedKey = index;

    for (uint32_t i = 0; i < VrHandCount; i++) {
      if (pointer.beams[i].active)
        m_backend.applyHaptic(VrHand(i), KeyHaptic, KeyHapticDuration);
    }
  }


  bool VrConsoleTerminal::createTextures() {
    constexpr UINT ImageWidth  = VrTerminalLayout::ImageWidth;
    constexpr UINT ImageHeight = VrTerminalLayout::ImageHeight;

    if (FAILED(m_device->CreateTexture(ImageWidth, ImageHeight, 1, 0, D3DFMT_A8R8G8B8,
          D3DPOOL_SYSTEMMEM, &m_staging, nullptr))
     || FAILED(m_device->CreateTexture(ImageWidth, ImageHeight, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
          D3DPOOL_DEFAULT, &m_texture, nullptr))) {
      m_staging = nullptr;
      m_texture = nullptr;
      return false;
    }

    return true;
  }


  bool VrConsoleTerminal::upload() {
    if (m_texture == nullptr && !createTextures())
      return false;

    constexpr uint32_t ImageWidth  = VrTerminalLayout::ImageWidth;
    constexpr uint32_t ImageHeight = VrTerminalLayout::ImageHeight;

    D3DLOCKED_RECT locked = { };

    if (FAILED(m_staging->LockRect(0, &locked, nullptr, 0)))
      return false;

    const uint32_t* pixels = m_image.pixels();

    for (uint32_t y = 0; y < ImageHeight; y++) {
      std::memcpy(static_cast<uint8_t*>(locked.pBits) + y * locked.Pitch,
        &pixels[y * ImageWidth], ImageWidth * sizeof(uint32_t));
    }

    m_staging->UnlockRect(0);

    return SUCCEEDED(m_device->UpdateTexture(m_staging.ptr(), m_texture.ptr()));
  }

}
