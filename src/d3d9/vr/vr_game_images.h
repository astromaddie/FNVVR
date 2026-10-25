#pragma once

#include <map>
#include <string>

#include "vr_paint.h"

namespace dxvk {

  /**
   * \brief The game's interface pictures, fonts and colour
   *
   * Read through the game's own file lookup (VrGameFile), so only on the
   * game's main thread once its archives are loaded. Each picture is read
   * once; one that cannot be read is not tried again.
   */
  class VrGameImages {

  public:

    /**
     * \brief An interface picture, such as an item's icon
     *
     * \param [in] path Path relative to Data\Textures, as the game's forms
     *    give it
     * \returns The picture, or \c nullptr if it cannot be read
     */
    const VrImage* picture(const std::string& path);

    /**
     * \brief The font of the HUD's names
     *
     * \returns The font, or \c nullptr if none of the game's fonts could
     *    be read
     */
    const VrPaintFont* font();

    /**
     * \brief The HUD's colour, as the player chose it
     */
    static VrColour hudColour();

  private:

    std::map<std::string, VrImage> m_pictures;
    std::map<std::string, bool>    m_missing;

    VrPaintFont m_font;
    bool         m_fontRead   = false;
    bool         m_fontLoaded = false;

  };

}
