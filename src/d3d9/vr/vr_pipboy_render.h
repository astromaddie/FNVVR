#pragma once

#include <cstdint>
#include <functional>
#include <memory>

#include "vr_math.h"

namespace dxvk {

  /**
   * \brief Draws the Pip-Boy over the live world, for each eye
   *
   * When the Pip-Boy opens, the game renders the world once into a frozen,
   * blurred background and shows that image behind the Pip-Boy, which in
   * the headset is a flat picture stuck to the head. Several calls of the
   * game are redirected, see findings/pipboy.md §3:
   *
   * - The background capture is skipped, when the Pip-Boy opens and when
   *   the character creation opens, so the game's flag for it stays clear
   *   and its world branch keeps running.
   * - That branch draws the rendered-menu path while a rendered menu is
   *   open, as the game's own menu branch would.
   * - In the rendered-menu path, the frozen background is replaced with
   *   the live world, drawn with the pieces of the game's normal world
   *   render (the recipe of the Real-Time Menus plugin), whenever the game
   *   has no frozen background to show.
   * - The Pip-Boy screen pass moves the world camera into the space the
   *   first-person model is drawn in. The callback given to \c install
   *   is called around that pass with the offset between both spaces, so
   *   that the eye pose can be kept there.
   *
   * Only used on the game's main thread.
   */
  class VrPipBoyRender {

  public:

    /**
     * \brief Called before the Pip-Boy screen pass with the offset from the
     *    world to the first-person drawing space, and with \c nullptr after it
     */
    using ScreenPassFn = std::function<void (const VrVector3* shift)>;

    ~VrPipBoyRender();

    /**
     * \brief Redirects the game's calls
     *
     * \param [in] screenPass Called around the Pip-Boy screen pass
     * \returns \c nullptr if the executable is not the supported version
     */
    static std::unique_ptr<VrPipBoyRender> install(ScreenPassFn screenPass);

  private:

    VrPipBoyRender() = default;

    ScreenPassFn m_screenPass;

    // Render target of the rendered-menu render, kept from its
    // initialisation for the world drawn into it
    void*        m_target = nullptr;

    // Set while the world is drawn behind the Pip-Boy
    bool         m_drawingWorld = false;

    bool         m_loggedWorld  = false;
    bool         m_loggedScreen = false;

    static void __fastcall backgroundHook(
            void*                 main,
            void*                 unused);

    static void __fastcall worldRenderHook(
            void*                 main,
            void*                 unused,
            void*                 target,
            uint32_t              renderedMenu,
            uint32_t              flag);

    static void* __fastcall targetHook(
            void*                 main,
            void*                 unused,
            void*                 renderer,
            void*                 destination,
            uint32_t              disableImageSpace,
            uint32_t              multisampling);

    static void __fastcall copyBackgroundHook(
            void*                 main,
            void*                 unused,
            void*                 imageSpaceManager,
            void*                 renderer,
            uint32_t              multisampling,
            void*                 destination);

    static int32_t* __fastcall threadsSettingHook(
            void*                 setting,
            void*                 unused);

    static void __fastcall screenHook(
            void*                 main,
            void*                 unused,
            void*                 screen,
            void*                 renderer);

    void drawWorld(uint8_t* main, void* target);

    static bool firstPersonShift(VrVector3& shift);

  };

}
