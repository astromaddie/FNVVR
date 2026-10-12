#pragma once

#include "vr_types.h"

namespace dxvk {

  /**
   * \brief The game's Pip-Boy, as far as opening and closing it goes
   */
  enum class VrPipBoyState {
    Closed,
    Open,
    /// Opening or closing: the arm moves or a request waits for the game
    Changing,
  };

  /**
   * \brief What the gaze asks the game to do with the Pip-Boy
   */
  enum class VrPipBoyRequest {
    None,
    Open,
    Close,
  };

  /**
   * \brief Inputs of the gaze for one frame
   *
   * Poses are in tracking space. \c head or \c screen is \c nullptr when
   * it is not known, which counts as not looking.
   */
  struct VrPipBoyGazeInput {
    const VrPose*     head     = nullptr;
    const VrVector3*  screen   = nullptr;
    VrPipBoyState     state    = VrPipBoyState::Closed;
    /// The player is in game and the hand carrying the Pip-Boy is free
    bool              canOpen  = false;
    /// No menu is open over the Pip-Boy
    bool              canClose = false;
    float             dt       = 0.0f;
  };

  /**
   * \brief Whether a point is in front of the head, close enough to look at
   *
   * \param [in] head Head pose, -Z forward
   * \param [in] point Point in the same space
   * \param [in] maxAngle Largest angle from the head's forward axis, in radians
   * \param [in] maxDistance Largest distance from the head, in metres
   */
  bool vrIsLookingAt(
    const VrPose&               head,
    const VrVector3&            point,
          float                 maxAngle,
          float                 maxDistance);

  /**
   * \brief Places a point given in a controller's frame in tracking space
   *
   * The frame is the one the hands are drawn with: the grip's position and
   * the aim's orientation.
   *
   * \param [in] controller Tracked controller
   * \param [in] offset Point in metres along the controller's forward, up
   *    and right axes
   */
  VrVector3 vrComputeControllerPoint(
    const VrControllerState&    controller,
    const VrVector3&            offset);

  /**
   * \brief Opens the Pip-Boy when the player looks at it
   *
   * The Pip-Boy opens once its screen has stayed in the middle of the view
   * for \ref OpenDelay, and closes once the player has looked away for
   * \ref CloseDelay. After the Pip-Boy was open, the player must look away
   * before the gaze opens it again, so that closing it with a menu button
   * while still looking at the wrist does not reopen it. A request is
   * repeated each frame until the game's state changes: the game may
   * refuse to open the Pip-Boy for a while (an animation, V.A.T.S., ...).
   */
  class VrPipBoyGaze {

  public:

    /// Largest angle between the head's forward axis and the screen, in radians (25 degrees)
    constexpr static float MaxAngle    = 0.436332f;
    /// Largest distance between the head and the screen, in metres
    constexpr static float MaxDistance = 0.7f;
    /// How long the screen is looked at before the Pip-Boy opens, in seconds
    constexpr static float OpenDelay   = 0.3f;
    /// How long the player looks away before the Pip-Boy closes, in seconds
    constexpr static float CloseDelay  = 0.6f;

    VrPipBoyRequest update(const VrPipBoyGazeInput& input);

    bool isLooking() const {
      return m_looking;
    }

  private:

    bool  m_looking  = false;
    bool  m_armed    = false;
    float m_lookTime = 0.0f;
    float m_awayTime = 0.0f;

  };

}
