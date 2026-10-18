#include "test.h"

#include "../vr_sound_keys.h"

using namespace dxvk;

TEST_CASE(sound_key_is_crossed_once_while_the_animation_plays) {
  // terminal01.nif: Open plays its sound at 0 s, Close at 0.033 s
  CHECK(vrSoundKeyCrossed(0.0f, 0.011f, 0.0f));
  CHECK(!vrSoundKeyCrossed(0.011f, 0.022f, 0.0f));
  CHECK(!vrSoundKeyCrossed(0.0f, 0.022f, 0.033f));
  CHECK(vrSoundKeyCrossed(0.022f, 0.044f, 0.033f));
  CHECK(!vrSoundKeyCrossed(0.044f, 0.055f, 0.033f));
  CHECK(!vrSoundKeyCrossed(0.044f, 0.044f, 0.033f));
}

TEST_CASE(sound_key_is_crossed_when_the_animation_loops) {
  CHECK(vrSoundKeyCrossed(0.6f, 0.01f, 0.0f));
  CHECK(vrSoundKeyCrossed(0.6f, 0.01f, 0.65f));
  CHECK(!vrSoundKeyCrossed(0.6f, 0.01f, 0.3f));
  // A sequence started again: from the end of its last run to its start
  CHECK(!vrSoundKeyCrossed(0.67f, 0.0f, 0.033f));
}

TEST_CASE(sound_key_gives_the_sound_editor_id) {
  CHECK(vrSoundKeyEditorId("sound: OBJComputerTerminalForward") == "OBJComputerTerminalForward");
  CHECK(vrSoundKeyEditorId("Sound: OBJComputerTerminalBackward\r") == "OBJComputerTerminalBackward");
  CHECK(vrSoundKeyEditorId("Sound: WPNPistol10mmReloadOut Bip01 L Hand") == "WPNPistol10mmReloadOut");
  CHECK(vrSoundKeyEditorId("start").empty());
  CHECK(vrSoundKeyEditorId("Sound:").empty());
  CHECK(vrSoundKeyEditorId("Enum: StopSounds ").empty());
  CHECK(vrSoundKeyEditorId(nullptr).empty());
}
