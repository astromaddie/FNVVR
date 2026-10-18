#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

#include "../d3d9_include.h"

#include "../../util/com/com_pointer.h"

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
     * \param [in] device The game's device, for the occlusion queries
     *    and the readback that measure the mask; it outlives the highlight
     * \param [in] take Gives the parts to glow
     * \returns \c nullptr if the executable is not the supported version
     */
    static std::unique_ptr<VrPartHighlight> install(IDirect3DDevice9* device, TakeFn take);

  private:

    /**
     * \brief What the masks of a few seconds held, for the log
     *
     * The samples are counted by occlusion queries around the depth pass
     * of the model and the colour pass of the parts, read back without
     * waiting; the effect's state is read once the game has applied it.
     * The first mask of each line is also read back from the GPU.
     */
    struct MaskReadback {
      bool      read          = false;
      HRESULT   result        = S_OK;
      UINT      width         = 0;
      UINT      height        = 0;
      D3DFORMAT format        = D3DFMT_UNKNOWN;
      bool      multisampled  = false;
      bool      bound         = false;
      bool      depth         = false;
      uint32_t  alphaPixels   = 0;
      uint32_t  colorPixels   = 0;
    };

    struct Measure {
      uint32_t masks          = 0;
      uint32_t samples        = 0;
      uint64_t modelSamples   = 0;
      uint64_t partSamples    = 0;
      uint32_t emptyParts     = 0;
      uint32_t effectActive   = 0;
      uint32_t effectRendered = 0;
      uint32_t activeEffects  = 0;
      int32_t  lastEffect     = -1;
      MaskReadback mask;
    };

    VrPartHighlight(IDirect3DDevice9* device, TakeFn take);

    IDirect3DDevice9*     m_device;
    TakeFn                m_take;
    std::vector<uint8_t*> m_parts;

    Com<IDirect3DQuery9>  m_modelQuery;
    Com<IDirect3DQuery9>  m_partQuery;
    bool                  m_queryPending = false;

    std::chrono::steady_clock::time_point m_start;

    Measure               m_measure;
    std::chrono::steady_clock::time_point m_measureStart;

    bool m_loggedGlow    = false;
    bool m_loggedNoMask  = false;

    void drawMask(void* renderer, void* manager, void* mask, uint8_t* root);

    void prepareEffect(void* manager, void* mask);

    void readMask(IDirect3DSurface9* surface, bool bound);

    void readQueries();

    void measureEffect();

    void logMeasure();

    static void __cdecl imageSpaceEffectsHook(
            void*                 renderer,
            void*                 target,
            void*                 destination);

  };

}
