#include <windows.h>

#include <vector>

#include "vr_module_path.h"

namespace dxvk {

  std::wstring vrModuleDirectory() {
    HMODULE module = nullptr;

    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&vrModuleDirectory), &module))
      return std::wstring();

    std::vector<WCHAR> buffer(MAX_PATH);
    DWORD length = 0;

    while ((length = GetModuleFileNameW(module, buffer.data(), DWORD(buffer.size()))) == buffer.size())
      buffer.resize(buffer.size() * 2);

    if (!length)
      return std::wstring();

    std::wstring path(buffer.data(), length);
    path.resize(path.find_last_of(L"\\/") + 1);
    return path;
  }

}
