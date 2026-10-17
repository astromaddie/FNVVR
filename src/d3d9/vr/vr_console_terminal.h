#pragma once

#include <memory>

#include "../d3d9_include.h"

#include "../../util/com/com_pointer.h"

#include "vr_backend.h"
#include "vr_console_screen.h"
#include "vr_menu_pointer.h"
#include "vr_terminal_image.h"

namespace dxvk {

  /**
   * \brief Shows the game's console on a terminal, with a keyboard
   *
   * While the game's console is open, the game's own terminal monitor
   * stands in front of the player at eye height, as when a terminal of
   * the world is used, with the console's output and command line on its
   * screen (VrConsoleScreen). Below it, over the bottom of the monitor, a
   * keyboard in the style of the world's terminals is shown on its own
   * panel (VrPanelId::Terminal), and the lasers type on it. The keys go to
   * the game's console through VrGameConsole, so commands run as typed
   * ones do. The right stick scrolls the output, the back button and the
   * EXIT and ESC keys close the console.
   *
   * Where there is no stereo view (the main menu), or when the game's
   * terminal cannot be used, only the keyboard is shown, and the console
   * is seen where the game draws it.
   *
   * Only used on the game's main thread.
   */
  class VrConsoleTerminal {

  public:

    /// Distance from the eyes to the terminal's screen, in metres
    constexpr static float ScreenDistance   = 0.7f;
    /// Width of the game's view of the terminal, in metres
    constexpr static float ScreenWidth      = 0.62f;
    /// Distance from the eyes to the keyboard, in metres
    constexpr static float KeyboardDistance = 0.5f;
    /// Width of the keyboard, in metres
    constexpr static float KeyboardWidth    = 0.55f;
    /// How far below the eyes the keyboard's centre is, in metres
    constexpr static float KeyboardDrop     = 0.3f;
    /// The text cursor is lit and off for this long each, in seconds, as
    /// on the game's terminals
    constexpr static float BlinkTime        = 0.4f;
    /// How long a key is drawn pressed, in seconds
    constexpr static float PressTime        = 0.12f;

    VrConsoleTerminal(IDirect3DDevice9* device, IVRBackend& backend);

    ~VrConsoleTerminal();

    /**
     * \brief Where the keyboard is, for the lasers
     * \returns The keyboard's placement, or \c nullptr while it is hidden
     */
    const VrPanelPlacement* placement() const {
      return m_shown ? &m_placement : nullptr;
    }

    /**
     * \brief Whether the terminal's monitor is in the eye images
     *
     * The menu panel, which the console is opened from, is then hidden:
     * the runtime would draw it over the monitor.
     */
    bool showsScreen() const {
      return m_screen && m_screen->isShown();
    }

    /**
     * \brief Shows the terminal while the console is open
     *
     * Called once per frame, after the controls pointed the lasers at
     * the keyboard's placement.
     *
     * \param [in] head Head pose in tracking space, which places the
     *    terminal when the console opens
     * \param [in] stereo Whether the game draws the eye images this frame
     * \param [in] pointer What the lasers did on the keyboard
     * \param [in] back Whether the menu's back button was pressed
     * \param [in] dt Duration of a frame in seconds
     */
    void update(const VrPose& head, bool stereo, const VrPointerState& pointer, bool back, float dt);

    /**
     * \brief Draws the monitor into the eye image being drawn
     *
     * \param [in] eye The eye's pose in tracking space and field of view
     */
    void drawEye(const VrEyeView& eye);

    /**
     * \brief Hides the terminal until the console opens again
     */
    void hide();

  private:

    IDirect3DDevice9*      m_device;
    IVRBackend&            m_backend;

    std::unique_ptr<VrConsoleScreen> m_screen;

    Com<IDirect3DTexture9> m_staging;
    Com<IDirect3DTexture9> m_texture;

    VrTerminalImage        m_image;
    VrTerminalKeyboardView m_view;
    VrPanelPlacement       m_placement;
    VrPose                 m_screenAnchor;

    bool  m_loaded     = false;
    bool  m_failed     = false;
    bool  m_shown      = false;
    bool  m_button     = false;
    float m_blinkTime  = 0.0f;
    float m_pressTime  = 0.0f;

    bool load();

    void place(const VrPose& head);

    void press(int32_t index, const VrPointerState& pointer);

    void updateScreen(bool stereo);

    bool createTextures();

    bool upload();

  };

}
