#include <algorithm>
#include <cctype>
#include <cstdlib>

#include "vr_options.h"

namespace dxvk {

  namespace {

    constexpr char SmoothTurnName[] = "smoothturn";

    std::string trim(const std::string& text) {
      size_t begin = 0;
      size_t end   = text.size();

      while (begin < end && std::isspace(static_cast<unsigned char>(text[begin])))
        begin++;

      while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1])))
        end--;

      return text.substr(begin, end - begin);
    }


    std::string lower(std::string text) {
      std::transform(text.begin(), text.end(), text.begin(),
        [] (unsigned char c) { return char(std::tolower(c)); });
      return text;
    }


    bool parseBool(const std::string& text, bool& value) {
      if (text.empty())
        return false;

      char* end = nullptr;
      long number = std::strtol(text.c_str(), &end, 10);

      if (*end)
        return false;

      value = number != 0;
      return true;
    }

  }


  void vrParseOptions(const std::string& text, VrOptions& options) {
    size_t position = 0;

    while (position <= text.size()) {
      size_t lineEnd = text.find('\n', position);

      if (lineEnd == std::string::npos)
        lineEnd = text.size();

      std::string line = trim(text.substr(position, lineEnd - position));
      position = lineEnd + 1;

      if (line.empty() || line[0] == ';' || line[0] == '#' || line[0] == '[')
        continue;

      size_t separator = line.find('=');

      if (separator == std::string::npos)
        continue;

      std::string name  = lower(trim(line.substr(0, separator)));
      std::string value = trim(line.substr(separator + 1));

      if (name == SmoothTurnName)
        parseBool(value, options.smoothTurn);
    }
  }


  std::string vrFormatOptions(const VrOptions& options) {
    std::string text = "; Written by the VR options page in the game's Settings menu.\n[VR]\n";
    text += "SmoothTurn=";
    text += options.smoothTurn ? "1" : "0";
    text += "\n";
    return text;
  }

}
