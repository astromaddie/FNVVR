#include <cstdint>

#include "vr_game_addresses.h"
#include "vr_game_memory.h"
#include "vr_pipboy_control.h"

namespace dxvk {

  namespace {

    using Callback     = void (__cdecl*)();
    using OpenPipBoyFn = void (__thiscall*)(void* interfaceManager, Callback callback, int32_t menu);
    using ClosePipBoyFn = void (__thiscall*)(void* interfaceManager, Callback callback);

    uint8_t* interfaceManager() {
      return *reinterpret_cast<uint8_t* const*>(VrGame::InterfaceManager);
    }

  }


  bool VrPipBoyControl::initialize() {
    m_available = VrGameMemory::matches(VrGame::OpenPipBoy,
        VrGame::OpenPipBoyPrologue, sizeof(VrGame::OpenPipBoyPrologue))
      && VrGameMemory::matches(VrGame::ClosePipBoy,
        VrGame::ClosePipBoyPrologue, sizeof(VrGame::ClosePipBoyPrologue))
      && VrGameMemory::matches(VrGame::InterfaceManagerGetSingleton,
        VrGame::InterfaceManagerGetSingletonBytes, sizeof(VrGame::InterfaceManagerGetSingletonBytes))
      && VrGameMemory::readable(VrGame::InterfaceManager, sizeof(uintptr_t));

    return m_available;
  }


  VrPipBoyState VrPipBoyControl::state() const {
    uint8_t* manager = m_available ? interfaceManager() : nullptr;

    if (!manager)
      return VrPipBoyState::Changing;

    switch (*reinterpret_cast<const uint32_t*>(manager + VrGame::InterfacePipBoyState)) {
      case 0:  return VrPipBoyState::Closed;
      case 3:  return VrPipBoyState::Open;
      default: return VrPipBoyState::Changing;
    }
  }


  void VrPipBoyControl::open() const {
    if (uint8_t* manager = m_available ? interfaceManager() : nullptr)
      reinterpret_cast<OpenPipBoyFn>(VrGame::OpenPipBoy)(manager, nullptr, 0);
  }


  void VrPipBoyControl::close() const {
    if (uint8_t* manager = m_available ? interfaceManager() : nullptr)
      reinterpret_cast<ClosePipBoyFn>(VrGame::ClosePipBoy)(manager, nullptr);
  }

}
