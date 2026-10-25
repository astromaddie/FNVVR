#include <vector>

#include "../../util/log/log.h"
#include "../../util/util_string.h"

#include "vr_game_addresses.h"
#include "vr_game_file.h"
#include "vr_game_images.h"

namespace dxvk {

  namespace {

    // The game's HUD fonts, then the console's, which is known to read;
    // each font's first texture is named after it
    constexpr const char* Fonts[] = {
      "Data\\Textures\\Fonts\\Monofonto_Large",
      "Data\\Textures\\Fonts\\Glow_Monofonto_Medium",
      "Data\\Textures\\Fonts\\Fixedsys_Comp_uniform_width",
    };

    // The amber of the game's default HUD
    constexpr uint32_t DefaultHudColour = 0xFFB642FFu;

    bool readImage(const std::string& path, VrImage& image) {
      std::vector<uint8_t> data;
      return VrGameFile::read(path.c_str(), VrGame::ArchiveTextures, data)
          && vrDecodeDds(data.data(), data.size(), image);
    }

  }


  const VrImage* VrGameImages::picture(const std::string& path) {
    if (path.empty() || m_missing.count(path))
      return nullptr;

    auto found = m_pictures.find(path);

    if (found != m_pictures.end())
      return &found->second;

    VrImage image;

    if (!readImage("Data\\Textures\\" + path, image)) {
      Logger::warn(str::format("VR: The interface picture '", path, "' could not be read"));
      m_missing[path] = true;
      return nullptr;
    }

    return &m_pictures.emplace(path, std::move(image)).first->second;
  }


  const VrPaintFont* VrGameImages::font() {
    if (m_fontRead)
      return m_fontLoaded ? &m_font : nullptr;

    m_fontRead = true;

    for (const char* name : Fonts) {
      std::string base = name;
      std::vector<uint8_t> data;

      if (VrGameFile::read((base + ".fnt").c_str(), VrGame::ArchiveTextures, data)
       && m_font.metrics.parse(data.data(), data.size())
       && readImage(base + "_0_Lod_A.dds", m_font.glyphs)) {
        Logger::info(str::format("VR: The VR panels write with the font '", base, "'"));
        m_fontLoaded = true;
        return &m_font;
      }
    }

    Logger::warn("VR: None of the game's fonts could be read, the VR panels show no text");
    return nullptr;
  }


  VrColour VrGameImages::hudColour() {
    auto setting = reinterpret_cast<const uint8_t*>(VrGame::HudColorSetting);
    uint32_t rgba = *reinterpret_cast<const uint32_t*>(setting + VrGame::SettingValue);

    // A colour the player cannot have chosen: the setting is not loaded
    if ((rgba & 0xFF) == 0)
      rgba = DefaultHudColour;

    return VrColour::fromRgba(rgba | 0xFFu);
  }

}
