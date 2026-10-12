#pragma once

#include <cstdint>
#include <string>

namespace dxvk {

  /**
   * \brief Settings the player changes from the game's VR options page
   *
   * The starting values come from dxvk.conf. The file written by the
   * options page (\c fnvvr\\vr_options.ini next to d3d9.dll) overrides
   * them for the options it contains.
   */
  struct VrOptions {
    /// Speed of smooth turning when none is configured, in degrees per second
    static constexpr float DefaultSmoothTurnSpeed = 120.0f;

    /// Turn continuously instead of by steps
    bool  smoothTurn      = false;
    /// Speed of smooth turning at full deflection, in degrees per second
    float smoothTurnSpeed = DefaultSmoothTurnSpeed;
  };

  /// Slowest smooth turning speed of the options page, in degrees per second
  constexpr float    VrSmoothTurnSpeedMin   = 30.0f;
  /// Difference between two positions of the options page's slider
  constexpr float    VrSmoothTurnSpeedStep  = 30.0f;
  /// Number of positions of the options page's slider
  constexpr uint32_t VrSmoothTurnSpeedSteps = 12;

  /**
   * \brief Returns the slider position that is closest to a speed
   *
   * \param [in] speed Speed in degrees per second, which does not have
   *    to be one of the slider's positions
   */
  uint32_t vrSmoothTurnSpeedToStep(float speed);

  /**
   * \brief Returns the speed of a slider position
   *
   * \param [in] step Position, at most \c VrSmoothTurnSpeedSteps - 1
   */
  float vrSmoothTurnStepToSpeed(uint32_t step);

  /**
   * \brief Reads the options from the content of an options file
   *
   * The format is one \c name=value line per option, with \c [section]
   * lines, blank lines and lines starting with \c ; or \c # ignored. Names
   * are not case sensitive. Options that are missing or have a value that
   * is not a number keep the value \c options holds.
   *
   * \param [in] text Content of the file
   * \param [in,out] options Options to update
   */
  void vrParseOptions(const std::string& text, VrOptions& options);

  /**
   * \brief Writes the options as the content of an options file
   */
  std::string vrFormatOptions(const VrOptions& options);

}
