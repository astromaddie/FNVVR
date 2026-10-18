#include <cctype>
#include <cstring>

#include "vr_sound_keys.h"

namespace dxvk {

  namespace {

    constexpr char SoundPrefix[] = "Sound: ";

  }


  bool vrSoundKeyCrossed(float previous, float current, float key) {
    if (previous > current)
      return previous < key || current >= key;

    return previous <= key && current > key;
  }


  std::string vrSoundKeyEditorId(const char* text) {
    constexpr size_t prefixLength = sizeof(SoundPrefix) - 1;

    if (!text)
      return std::string();

    // The game compares the prefix without case
    for (size_t i = 0; i < prefixLength; i++) {
      if (std::tolower(static_cast<unsigned char>(text[i])) != std::tolower(static_cast<unsigned char>(SoundPrefix[i])))
        return std::string();
    }

    const char* id  = text + prefixLength;
    size_t      end = std::strcspn(id, " \r");
    return std::string(id, end);
  }

}
