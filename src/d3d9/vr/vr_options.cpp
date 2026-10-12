#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>

#include "vr_options.h"

namespace dxvk {

  namespace {

    constexpr char SmoothTurnName[]      = "smoothturn";
    constexpr char SmoothTurnSpeedName[] = "smoothturnspeed";

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


    bool parseFloat(const std::string& text, float& value) {
      if (text.empty())
        return false;

      char* end = nullptr;
      double number = std::strtod(text.c_str(), &end);

      if (*end || !(number > 0.0))
        return false;

      float low  = VrSmoothTurnSpeedMin;
      float high = vrSmoothTurnStepToSpeed(VrSmoothTurnSpeedSteps - 1);
      value = std::min(std::max(float(number), low), high);
      return true;
    }

  }


  uint32_t vrSmoothTurnSpeedToStep(float speed) {
    float step = (speed - VrSmoothTurnSpeedMin) / VrSmoothTurnSpeedStep + 0.5f;

    if (!(step > 0.0f))
      return 0;

    return std::min(uint32_t(step), VrSmoothTurnSpeedSteps - 1);
  }


  float vrSmoothTurnStepToSpeed(uint32_t step) {
    return VrSmoothTurnSpeedMin + float(std::min(step, VrSmoothTurnSpeedSteps - 1)) * VrSmoothTurnSpeedStep;
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
      else if (name == SmoothTurnSpeedName)
        parseFloat(value, options.smoothTurnSpeed);
    }
  }


  std::string vrFormatOptions(const VrOptions& options) {
    std::string text = "; Written by the VR options page in the game's Settings menu.\n[VR]\n";
    text += "SmoothTurn=";
    text += options.smoothTurn ? "1" : "0";
    text += "\n";

    char speed[32];
    std::snprintf(speed, sizeof(speed), "%g", options.smoothTurnSpeed);
    text += "SmoothTurnSpeed=";
    text += speed;
    text += "\n";
    return text;
  }

}
