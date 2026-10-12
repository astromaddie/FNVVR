#pragma once

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
    /// Turn continuously instead of by steps
    bool smoothTurn = false;
  };

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
