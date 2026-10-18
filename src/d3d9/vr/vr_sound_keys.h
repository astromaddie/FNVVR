#pragma once

#include <string>

namespace dxvk {

  /**
   * \brief Tells whether an animation's update went past a text key
   *
   * The rule of BSAnimGroupSequence::PlaySounds (0x004EEF00): a key is
   * crossed if it is at or after the previous time and before the new
   * one; when the new time is before the previous one, the animation
   * looped and every key after the previous time or up to the new one is
   * crossed. See findings/console.md §11.2.
   *
   * \param [in] previous Scaled time of the previous update
   * \param [in] current Scaled time of this update
   * \param [in] key Time of the text key
   */
  bool vrSoundKeyCrossed(float previous, float current, float key);

  /**
   * \brief Editor ID of the sound a text key plays
   *
   * Sound keys read "Sound: <editor ID>", optionally followed by a space
   * and the node the sound follows, and may end with a carriage return.
   *
   * \param [in] text Text of the key
   * \returns The editor ID, empty if the key plays no sound
   */
  std::string vrSoundKeyEditorId(const char* text);

}
