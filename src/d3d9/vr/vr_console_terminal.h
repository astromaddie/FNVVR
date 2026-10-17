#pragma once

#include <vector>

#include "../d3d9_include.h"

#include "../../util/com/com_pointer.h"

#include "vr_backend.h"
#include "vr_menu_pointer.h"
#include "vr_terminal_image.h"

namespace dxvk {

  /**
   * \brief Shows the game's console as a terminal with a keyboard
   *
   * While the game's console is open, a terminal is shown on its own
   * panel (VrPanelId::Terminal) in front of the player, a little below
   * the eyes: the console's output and command line on a screen in the
   * style of the game's terminals, and a keyboard the lasers type on
   * (see VrTerminalImage). The keys go to the game's console through
   * VrGameConsole, so commands run as typed ones do. The right stick
   * scrolls the output, the back button and the EXIT key close the
   * console.
   *
   * The image is drawn on the CPU into a system memory texture and
   * copied to a texture the runtime can read when it changes.
   *
   * Only used on the game's main thread.
   */
  class VrConsoleTerminal {

  public:

    /// Distance from the eyes to the terminal, in metres
    constexpr static float Distance  = 0.6f;
    /// Width of the terminal, in metres; its image is square
    constexpr static float Width     = 0.7f;
    /// How far below the eyes the terminal's centre is, in metres
    constexpr static float Drop      = 0.2f;
    /// The text cursor is lit and off for this long each, in seconds
    constexpr static float BlinkTime = 0.4f;
    /// How long a key is drawn pressed, in seconds
    constexpr static float PressTime = 0.12f;

    VrConsoleTerminal(IDirect3DDevice9* device, IVRBackend& backend);

    ~VrConsoleTerminal();

    /**
     * \brief Where the terminal is, for the lasers
     * \returns The terminal's placement, or \c nullptr while it is hidden
     */
    const VrPanelPlacement* placement() const {
      return m_shown ? &m_placement : nullptr;
    }

    /**
     * \brief Shows the terminal while the console is open
     *
     * Called once per frame, after the controls pointed the lasers at
     * the terminal's placement.
     *
     * \param [in] head Head pose in tracking space, which places the
     *    terminal when the console opens
     * \param [in] pointer What the lasers did on the terminal
     * \param [in] back Whether the menu's back button was pressed
     * \param [in] dt Duration of a frame in seconds
     */
    void update(const VrPose& head, const VrPointerState& pointer, bool back, float dt);

    /**
     * \brief Hides the terminal until the console opens again
     */
    void hide();

  private:

    IDirect3DDevice9*      m_device;
    IVRBackend&            m_backend;

    Com<IDirect3DTexture9> m_staging;
    Com<IDirect3DTexture9> m_texture;

    VrTerminalImage        m_image;
    VrTerminalView         m_view;
    VrPanelPlacement       m_placement;

    bool  m_shown      = false;
    bool  m_failed     = false;
    bool  m_button     = false;
    bool  m_shift      = false;
    float m_blinkTime  = 0.0f;
    float m_pressTime  = 0.0f;

    void place(const VrPose& head);

    void press(int32_t index, const VrPointerState& pointer);

    bool createTextures();

    bool upload();

  };

}
