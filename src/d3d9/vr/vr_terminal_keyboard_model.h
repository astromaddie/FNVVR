#pragma once

#include <memory>

#include "../d3d9_include.h"

#include "vr_terminal_keyboard_mount.h"

namespace dxvk {

  /**
   * \brief The world terminal's keyboard, unfolded below the console's monitor
   *
   * The keyboard and its bracket are those of the terminals of the world
   * (meshes\\terminals\\terminal01.nif): a copy of that model is hung from
   * the console's monitor with its own monitor, base and screen texts
   * hidden, placed as VrTerminalKeyboardMount describes. The game's own
   * animation of the model unfolds the keyboard when the console opens
   * (its "Open" sequence) and folds it back when the console closes
   * ("Close"), played by the model's controller manager as the game plays
   * the animations of its menus' 3D models. Their sounds are played as
   * the interface's sounds.
   *
   * The keyboard is drawn with the game's terminal texture, shared with
   * the terminals of the world, so the console's copy of that texture is
   * given to the game only while the console's terminal is drawn. See
   * findings/console.md §11.
   *
   * Only used on the game's main thread.
   */
  class VrTerminalKeyboardModel {

  public:

    enum class State {
      /// Not hanging from the monitor
      Detached,
      Opening,
      /// Unfolded, ready to be typed on
      Open,
      /// Folding, detached once folded
      Closing,
    };

    /**
     * \brief Checks that the game's functions are the expected ones
     * \returns \c nullptr if the executable is not the supported version
     */
    static std::unique_ptr<VrTerminalKeyboardModel> create();

    ~VrTerminalKeyboardModel();

    VrTerminalKeyboardModel(const VrTerminalKeyboardModel&) = delete;
    VrTerminalKeyboardModel& operator = (const VrTerminalKeyboardModel&) = delete;

    /**
     * \brief Hangs the keyboard from the monitor and unfolds it
     *
     * The model is read the first time and kept.
     *
     * \param [in] monitor The model of the console's monitor (NiNode*),
     *    attached to the menu scene
     * \returns \c false if the model could not be read or used
     */
    bool attach(void* monitor);

    /**
     * \brief Folds the keyboard, then takes it off the monitor
     */
    void close();

    /**
     * \brief Takes the keyboard off the monitor at once
     */
    void detach();

    /**
     * \brief Plays the animation and its sounds
     *
     * \param [in] dt Duration of the frame, in seconds
     */
    void update(float dt);

    State state() const {
      return m_state;
    }

    /**
     * \brief Transform of the keyboard node in the menu's space
     *
     * Only meaningful while the keyboard is attached.
     */
    VrGameScaledTransform keyboardTransform() const;

    /**
     * \brief Gives the game another terminal texture until endDraw
     *
     * \param [in] texture The texture the keyboard is drawn with, which
     *    must be a copy of the terminal texture
     */
    void beginDraw(IDirect3DBaseTexture9* texture);

    /**
     * \brief Gives the game its terminal texture back
     */
    void endDraw();

  private:

    VrTerminalKeyboardModel() = default;

    // The copy of the model (NiPointer), kept from the first attach on
    void*    m_root     = nullptr;
    uint8_t* m_manager  = nullptr;
    uint8_t* m_open     = nullptr;
    uint8_t* m_close    = nullptr;
    uint8_t* m_sequence = nullptr;
    uint8_t* m_keyboard = nullptr;
    uint8_t* m_plate    = nullptr;
    void*    m_monitor  = nullptr;

    // Where the game's Direct3D texture was swapped, and with what
    void**   m_textureSlot  = nullptr;
    void*    m_savedTexture = nullptr;

    bool     m_failed = false;
    State    m_state  = State::Detached;
    float    m_time   = 0.0f;

    bool load(void* monitor);

    bool place();

    void play(uint8_t* sequence, uint8_t* previous);

    bool sequenceEnded() const;

    void playSounds();

  };

}
