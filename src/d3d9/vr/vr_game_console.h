#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace dxvk {

  /**
   * \brief The game's console, driven without a keyboard
   *
   * The console is the game's own: it is opened through the request
   * TESMain::OnIdle reads each frame, and it is given keys at the point
   * where the interface reads the keyboard, one key per frame, so that a
   * command runs where a typed one does. Its output and command line are
   * read back from the game for the terminal to show. See
   * findings/console.md.
   *
   * Only used on the game's main thread.
   */
  class VrGameConsole {

  public:

    /**
     * \brief Redirects the call the console reads its keys from
     * \returns \c false if the executable is not the supported version
     */
    static bool install();

    /**
     * \brief Whether the console can be opened
     *
     * Opening only needs the game's request flag, so the console can be
     * opened even when the keys cannot be given to it.
     */
    static bool canOpen();

    /**
     * \brief Asks the game to open the console on its next frame
     */
    static void requestOpen();

    /**
     * \brief Whether the console is shown
     */
    static bool isOpen();

    /**
     * \brief Queues a key for the console
     *
     * \param [in] key A character, or a VrConsoleKey code
     */
    static void pressKey(uint32_t key);

    /**
     * \brief Closes the console on the next frame the interface reads keys
     *
     * Keys still queued are dropped.
     */
    static void requestClose();

    /**
     * \brief Reads the console's output
     *
     * \param [out] lines The last lines up to the one shown at the bottom,
     *    oldest first; the output scrolled up with Page Up ends earlier
     * \param [in] count How many lines to read at most
     */
    static void readOutput(std::vector<std::string>& lines, uint32_t count);

    /**
     * \brief Reads the command being typed
     *
     * \param [out] text The command
     * \param [out] cursor Position of the text cursor in \c text
     */
    static void readInput(std::string& text, uint32_t& cursor);

  };

}
