#include <cstdint>

#include "../../util/log/log.h"

#include "vr_game_addresses.h"
#include "vr_game_memory.h"
#include "vr_npc_gaze.h"

namespace dxvk {

  namespace {

    // PlayerCharacter::GetLookingAtLocation is __thiscall with the result
    // on the stack, and returns the result
    using LookingAtFn = float* (__fastcall*)(uint8_t* player, void* unused, float* result);

    VrNpcGaze* g_npcGaze = nullptr;

  }


  VrNpcGaze::~VrNpcGaze() {
    // The table entry stays in place and falls through to the game's function
    g_npcGaze = nullptr;
  }


  std::unique_ptr<VrNpcGaze> VrNpcGaze::install() {
    if (g_npcGaze)
      return nullptr;

    if (!VrGameMemory::redirectVirtual(VrGame::PlayerLookingAtSlot, VrGame::PlayerLookingAt,
        reinterpret_cast<const void*>(&VrNpcGaze::lookingAtHook)))
      return nullptr;

    std::unique_ptr<VrNpcGaze> gaze(new VrNpcGaze());
    g_npcGaze = gaze.get();
    return gaze;
  }


  void VrNpcGaze::setHead(const VrVector3* head) {
    std::lock_guard<dxvk::mutex> lock(m_mutex);
    m_hasHead = head != nullptr;

    if (head)
      m_head = *head;
  }


  bool VrNpcGaze::head(VrVector3& head) {
    std::lock_guard<dxvk::mutex> lock(m_mutex);

    if (!m_hasHead)
      return false;

    head = m_head;

    if (!m_loggedHead) {
      Logger::info("VR: Actors look at the headset instead of the game's first-person camera");
      m_loggedHead = true;
    }

    return true;
  }


  float* __fastcall VrNpcGaze::lookingAtHook(
          uint8_t*              player,
          void*                 unused,
          float*                result) {
    VrVector3 position;

    // In third person the game looks for the head of the drawn body
    if (g_npcGaze && !player[VrGame::PlayerCurrentThirdPerson] && g_npcGaze->head(position)) {
      result[0] = position.x;
      result[1] = position.y;
      result[2] = position.z;
      return result;
    }

    return reinterpret_cast<LookingAtFn>(VrGame::PlayerLookingAt)(player, nullptr, result);
  }

}
