#include <cctype>
#include <cstring>

#include "vr_sound_keys.h"

namespace dxvk {

  namespace {

    constexpr char SoundPrefix[] = "Sound: ";
    constexpr char HitKey[]      = "hit";
    constexpr char ReleaseKey[]  = "release";

    /**
     * \brief Whether a key's text is a word, without case, up to a
     *    carriage return
     */
    bool isKey(const char* text, const char* word, size_t wordLength) {
      if (!text)
        return false;

      size_t length = std::strcspn(text, "\r");

      if (length != wordLength)
        return false;

      for (size_t i = 0; i < length; i++) {
        if (std::tolower(static_cast<unsigned char>(text[i])) != word[i])
          return false;
      }

      return true;
    }

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
    return isKey(text, HitKey, sizeof(HitKey) - 1);
  }


  bool vrIsReleaseKey(const char* text) {
    return isKey(text, ReleaseKey, sizeof(ReleaseKey) - 1);
  }

}
