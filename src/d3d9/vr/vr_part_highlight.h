#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace dxvk {

  /**
   * \brief Makes parts of the first-person weapon glow as V.A.T.S. does
   *
   * V.A.T.S. highlights a limb with a mask: the target is drawn into a
   * rendered texture, its depth first and then the limb in colour, and the
   * interface image space effect turns the mask into a pulsing glow in the
   * HUD colour with scan lines. The same is done here for the parts of the
   * weapon in the hand that the reload needs worked (vrPartsToWork): the
   * first-person model and the parts are drawn into the same kind of mask
   * with the first-person camera of the eye being drawn, just before the
   * game applies its image space effects, and the effect is set up through
   * the V.A.T.S. manager's own PrepareHighlightEffect. The hand holding a
   * part hides it, as the rest of an actor hides a limb. See
   * findings/reload.md §9.
   *
   * Nothing is drawn while V.A.T.S. draws its own highlight.
   *
   * Only used on the game's main thread, which renders.
   */
  class VrPartHighlight {

  public:

    /**
     * \brief Gives the first-person root and the parts to glow
     *
     * Called once per eye, as the game applies its image space effects.
     *
     * \returns \c false if nothing is to glow in this eye
     */
    using TakeFn = std::function<bool (uint8_t*& root, std::vector<uint8_t*>& parts)>;

    ~VrPartHighlight();

    /**
     * \brief Checks the game's code and redirects its image space effects
     *
     * \param [in] take Gives the parts to glow
     * \returns \c nullptr if the executable is not the supported version
     */
    static std::unique_ptr<VrPartHighlight> install(TakeFn take);

  private:

    explicit VrPartHighlight(TakeFn take);

    TakeFn                m_take;
    std::vector<uint8_t*> m_parts;

    std::chrono::steady_clock::time_point m_start;

    bool m_loggedGlow    = false;
    bool m_loggedNoMask  = false;

    void drawMask(void* renderer, void* manager, void* mask, uint8_t* root);

    void prepareEffect(void* manager, void* mask);

    static void __cdecl imageSpaceEffectsHook(
            void*                 renderer,
            void*                 target,
            void*                 destination);

  };

}
