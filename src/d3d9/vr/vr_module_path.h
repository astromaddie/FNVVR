#pragma once

#include <string>

namespace dxvk {

  /**
   * \brief Returns the folder that holds d3d9.dll, with a trailing backslash
   *
   * Files the VR layer ships or writes (the action manifest, the options
   * file) live in a \c fnvvr folder there, not in the current directory.
   * Returns an empty string if the folder cannot be determined.
   */
  std::wstring vrModuleDirectory();

}
