#include <algorithm>
#include <cmath>
#include <cstring>

#include "../../util/log/log.h"

#include "vr_console_terminal.h"
#include "vr_d3d9_bridge.h"
#include "vr_game_addresses.h"
#include "vr_game_console.h"
#include "vr_game_file.h"
#include "vr_math.h"
#include "vr_terminal_keyboard.h"

namespace dxvk {

  namespace {

    // Steepest the keyboard tilts back to face the eyes, in radians (45 degrees)
    constexpr float MaxPitch = 0.7853982f;

    // A light tick in the hand that typed a key
    constexpr float   KeyHaptic         = 0.15f;
    constexpr int64_t KeyHapticDuration = 15'000'000;

    // The keyboard of the world's terminals and the terminal font (font 5),
    // see findings/console.md §7
    constexpr char KeyboardTexture[] = "Data\\Textures\\Terminals\\terminal01.dds";
    constexpr char FontFile[]        = "Data\\Textures\\Fonts\\Fixedsys_Comp_uniform_width.fnt";
    constexpr char FontTexture[]     = "Data\\Textures\\Fonts\\Fixedsys_Comp_uniform_width_0_Lod_A.dds";

    bool readImage(const char* path, VrImage& image) {
      std::vector<uint8_t> data;
      return VrGameFile::read(path, VrGame::ArchiveTextures, data)
          && vrDecodeDds(data.data(), data.size(), image);
    }

  }


  VrConsoleTerminal::VrConsoleTerminal(IDirect3DDevice9* device, IVRBackend& backend)
  : m_device(device), m_backend(backend), m_screen(VrConsoleScreen::install(device)) {
    m_placement.width  = KeyboardWidth;
    m_placement.height = KeyboardWidth * float(VrTerminalImage::Height) / float(VrTerminalImage::Width);
  }


  VrConsoleTerminal::~VrConsoleTerminal() {
    if (m_shown)
      m_backend.hidePanel(VrPanelId::Terminal);
  }


  void VrConsoleTerminal::update(const VrPose& head, bool stereo, const VrPointerState& pointer, bool back, float dt) {
    if (m_failed || !VrGameConsole::isOpen()) {
      hide();
      return;
    }

    if (!m_loaded && !load()) {
      Logger::err("VR: Failed to read the terminal's keyboard and font, the console is not shown in the headset");
      m_failed = true;
      return;
    }

    if (!m_shown) {
      place(head);
      m_button    = pointer.button;
      m_blinkTime = 0.0f;
      m_pressTime = 0.0f;
      m_view      = VrTerminalKeyboardView();
      Logger::info("VR: The console's keyboard is shown");
    }

    int32_t hovered = pointer.cursorActive ? VrTerminalKeyboard::findKey(pointer.cursor) : -1;

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

    m_view.hoveredKey = hovered;

    updateScreen(stereo);

    bool changed = m_image.update(m_view);

    if ((changed || !m_shown) && !upload()) {
      Logger::err("VR: Failed to draw the console's keyboard, the console is not shown in the headset");
      m_failed = true;
      hide();
      return;
    }

    if (changed || !m_shown) {
      m_shown = VrD3D9Bridge::submitPanel(m_device, m_backend, VrPanelId::Terminal,
        m_texture.ptr(), m_placement.pose, KeyboardWidth, VrPanelAnchor::Room);
    }
  }


  void VrConsoleTerminal::drawEye(const VrEyeView& eye) {
    if (m_screen)
      m_screen->drawEye(eye);
  }


  void VrConsoleTerminal::hide() {
    if (m_screen)
      m_screen->hide();

    if (!m_shown)
      return;

    m_backend.hidePanel(VrPanelId::Terminal);
    m_shown = false;
  }


  bool VrConsoleTerminal::load() {
    std::vector<uint8_t> font;
    VrBitmapFont metrics;
    VrImage keyboard, glyphs;

    m_loaded = VrGameFile::read(FontFile, VrGame::ArchiveTextures, font)
      && metrics.parse(font.data(), font.size())
      && readImage(FontTexture, glyphs)
      && readImage(KeyboardTexture, keyboard)
      && m_image.initialize(keyboard, metrics, glyphs);

    return m_loaded;
  }


  void VrConsoleTerminal::place(const VrPose& head) {
    // The monitor straight ahead at eye height, as the game shows its
    // terminals, and the keyboard in front of its bottom, lowered and
    // tilted back to face the eyes as on a desk
    m_screenAnchor = vrComputePanelPose(head, ScreenDistance);

    VrVector3 centre = vrComputePanelPose(head, KeyboardDistance).position;
    centre.y -= KeyboardDrop;

    m_placement.pose = vrComputePanelPoseTowards(head, centre, KeyboardDistance, MaxPitch);
  }


  void VrConsoleTerminal::press(int32_t index, const VrPointerState& pointer) {
    uint32_t count = 0;
    const VrTerminalKey& key = VrTerminalKeyboard::keys(count)[index];

    bool shifted = VrTerminalKeyboard::isShifted(key, m_view.shift, m_view.capsLock);

    switch (key.kind) {
      case VrTerminalKeyKind::Char:
        VrGameConsole::pressKey(VrTerminalKeyboard::code(key, shifted));
        m_view.shift = false;
        break;

      case VrTerminalKeyKind::Edit:
        VrGameConsole::pressKey(VrTerminalKeyboard::code(key, shifted));
        break;

      case VrTerminalKeyKind::Shift:
        m_view.shift = !m_view.shift;
        break;

      case VrTerminalKeyKind::CapsLock:
        m_view.capsLock = !m_view.capsLock;
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


  void VrConsoleTerminal::updateScreen(bool stereo) {
    if (!m_screen)
      return;

    if (!stereo || !m_screen->show(m_screenAnchor, ScreenWidth)) {
      m_screen->hide();
      return;
    }

    std::vector<std::string> lines;
    VrGameConsole::readOutput(lines, VrTerminalTextLayout::OutputLines);

    std::string input;
    uint32_t cursor = 0;
    VrGameConsole::readInput(input, cursor);

    VrTerminalScreenText text;
    text.output = vrWrapTerminalLines(lines, VrTerminalTextLayout::Columns, VrTerminalTextLayout::OutputLines);
    vrLayoutTerminalCommand(input, cursor, VrTerminalTextLayout::Columns, text.command, text.cursorColumn);
    text.cursorShown = std::fmod(m_blinkTime, 2.0f * BlinkTime) < BlinkTime;

    m_screen->setText(text);
  }


  bool VrConsoleTerminal::createTextures() {
    constexpr UINT Width  = VrTerminalImage::Width;
    constexpr UINT Height = VrTerminalImage::Height;

    if (FAILED(m_device->CreateTexture(Width, Height, 1, 0, D3DFMT_A8R8G8B8,
          D3DPOOL_SYSTEMMEM, &m_staging, nullptr))
     || FAILED(m_device->CreateTexture(Width, Height, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
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

    constexpr uint32_t Width  = VrTerminalImage::Width;
    constexpr uint32_t Height = VrTerminalImage::Height;

    D3DLOCKED_RECT locked = { };

    if (FAILED(m_staging->LockRect(0, &locked, nullptr, 0)))
      return false;

    const uint32_t* pixels = m_image.pixels();

    for (uint32_t y = 0; y < Height; y++) {
      std::memcpy(static_cast<uint8_t*>(locked.pBits) + y * locked.Pitch,
        &pixels[y * Width], Width * sizeof(uint32_t));
    }

    m_staging->UnlockRect(0);

    return SUCCEEDED(m_device->UpdateTexture(m_staging.ptr(), m_texture.ptr()));
  }

}
