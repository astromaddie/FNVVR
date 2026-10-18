#include <cctype>
#include <cstring>

#include "vr_sound_keys.h"

namespace dxvk {

  namespace {

    constexpr char SoundPrefix[] = "Sound: ";
    constexpr char HitKey[]      = "hit";

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


  bool vrIsHitKey(const char* text) {
    if (!text)
      return false;

    size_t length = std::strcspn(text, "\r");

    if (length != sizeof(HitKey) - 1)
      return false;

    for (size_t i = 0; i < length; i++) {
      if (std::tolower(static_cast<unsigned char>(text[i])) != HitKey[i])
        return false;
    }

    return true;
  }

}
