#include <algorithm>

#include "vr_terminal_text.h"

namespace dxvk {

  std::vector<std::string> vrWrapTerminalLines(
    const std::vector<std::string>& lines,
          uint32_t                  columns,
          uint32_t                  rows) {
    std::vector<std::string> wrapped;

    if (!columns)
      return wrapped;

    for (std::string line : lines) {
      while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
        line.pop_back();

      std::replace(line.begin(), line.end(), '\t', ' ');

      while (line.size() > columns) {
        size_t space = line.rfind(' ', columns);

        if (space != std::string::npos && space > 0) {
          wrapped.push_back(line.substr(0, space));
          line.erase(0, space + 1);
        } else {
          wrapped.push_back(line.substr(0, columns));
          line.erase(0, columns);
        }
      }

      wrapped.push_back(line);
    }

    if (wrapped.size() > rows)
      wrapped.erase(wrapped.begin(), wrapped.end() - rows);

    return wrapped;
  }


  void vrLayoutTerminalCommand(
    const std::string&              input,
          uint32_t                  cursor,
          uint32_t                  columns,
          std::string&              text,
          uint32_t&                 cursorColumn) {
    const std::string prompt = VrTerminalTextLayout::Prompt;

    // The cursor stands on the column after the last character too
    uint32_t visible = columns > prompt.size() + 1 ? columns - uint32_t(prompt.size()) - 1 : 1;

    cursor = std::min(cursor, uint32_t(input.size()));
    uint32_t start = cursor > visible ? cursor - visible : 0;

    text = prompt + input.substr(start, visible);
    cursorColumn = uint32_t(prompt.size()) + cursor - start;
  }

}
