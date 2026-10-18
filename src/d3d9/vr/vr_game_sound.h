#pragma once

#include <cstdint>
#include <string>

#include "vr_types.h"

namespace dxvk {

  /**
   * \brief Finds a sound form by its editor ID
   *
   * \param [in] editorId The TESSound's editor ID
   * \returns The TESSound, or \c nullptr if there is none of that name
   */
  const uint8_t* vrFindGameSound(const std::string& editorId);

  /**
   * \brief File name of a sound form, for logs
   */
  const char* vrGameSoundFile(const uint8_t* sound);

  /**
   * \brief Plays a sound form once, as the game plays animation sounds
   *
   * \param [in] sound The TESSound
   * \param [in] flags BSAudio flags, without the sound's loop flag,
   *    which is added from the form
   * \param [in] position Where the sound is, in game units, for 3D sounds
   * \param [in] volume Volume, from 0 to 1
   * \param [in] frequency Playback speed, 1 for the recorded one
   */
  void vrPlayGameSound(
    const uint8_t*              sound,
          uint32_t              flags,
    const VrVector3&            position,
          float                 volume,
          float                 frequency);

}
