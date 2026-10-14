#pragma once

#include <array>
#include <cstdint>

#include "vr_types.h"

namespace dxvk {

  /**
   * \brief Rectangle in the game's interface
   *
   * In interface units or pixels, the vertical axis grows downwards.
   */
  struct VrInterfaceRect {
    float left   = 0.0f;
    float top    = 0.0f;
    float right  = 0.0f;
    float bottom = 0.0f;

    float width() const {
      return right - left;
    }

    float height() const {
      return bottom - top;
    }

    bool isEmpty() const {
      return right <= left || bottom <= top;
    }
  };

  /**
   * \brief Size of the game's interface in its own units
   *
   * The interface is laid out in units where the shorter side of the
   * screen is 960 (landscape) or 1280 (portrait) units long, whatever the
   * resolution, see findings/hud.md.
   *
   * \param [in] width Width of the render target, in pixels
   * \param [in] height Height of the render target, in pixels
   */
  VrVector2 vrComputeInterfaceSize(uint32_t width, uint32_t height);

  /**
   * \brief Parts of the interface image cut out for the hand HUD panels
   */
  struct VrHandHudCrops {
    /// Health bar and compass, for the back of the right hand
    VrInterfaceRect wrist;
    /// Interaction prompt, for the fingertip
    VrInterfaceRect prompt;
  };

  /**
   * \brief Computes where the hand HUD elements are in the interface image
   *
   * Both elements are drawn in the same interface pass, so each crop only
   * has to hold its own element and must not reach into the other. The
   * game puts the health bar and compass in the bottom left corner and the
   * prompt at the bottom centre. The crop of the health bar is its group's
   * rectangle with a margin. The prompt's text is wrapped to its group's
   * width but its lines reach above and below the group, so its crop is
   * taller, stays centred on the group and is narrowed so that it does not
   * overlap the health bar's crop.
   *
   * \param [in] hitPoints Rectangle of the HitPoints group, in interface units
   * \param [in] info Rectangle of the Info group, in interface units
   * \param [in] size Size of the interface, in interface units
   */
  VrHandHudCrops vrComputeHandHudCrops(
    const VrInterfaceRect&      hitPoints,
    const VrInterfaceRect&      info,
    const VrVector2&            size);

  /**
   * \brief Where the hand HUD panels are placed and how large they are
   *
   * Offsets are in metres along the hand's forward, up and right axes, in
   * the frame the hands are drawn with (vrComputeControllerPoint).
   */
  struct VrHandHudLayout {
    /// The health bar and compass are on the back of the right hand, which
    /// faces right when the controller is held upright
    constexpr static VrVector3 WristOffset = { -0.02f, 0.02f, 0.05f };
    /// The display is turned from the hand's right axis towards its up
    /// axis by this angle, in radians (30 degrees), so that a slight turn of
    /// the wrist shows it
    constexpr static float WristFaceUpAngle = 0.5235988f;
    /// Panel size per interface unit, in metres: the health bar's group is
    /// about 13 cm wide
    constexpr static float WristMetresPerUnit = 0.00035f;

    /// The prompt is above the index fingertip of the hand nearest the target
    constexpr static VrVector3 FingertipOffset = { 0.08f, 0.0f, 0.0f };
    /// Height of the prompt's centre above the fingertip, in metres
    constexpr static float PromptLift = 0.07f;
    /// Panel size per interface unit, in metres: about the angular size of
    /// the HUD panel in front of the head, for a hand half a metre away
    constexpr static float PromptMetresPerUnit = 0.0006f;
  };

  /**
   * \brief Pose of the health bar and compass panel on the back of a hand
   *
   * The panel's up axis runs along the hand towards the fingers, like the
   * dial of a watch, and the panel faces outwards from the back of the hand.
   */
  VrPose vrComputeWristPanelPose(const VrControllerState& hand);

  /**
   * \brief Pose of the interaction prompt panel at a hand's fingertip
   *
   * The panel is above the fingertip, upright and facing the head.
   */
  VrPose vrComputePromptPanelPose(const VrControllerState& hand, const VrPose& head);

  /**
   * \brief Shows the wrist display while the player looks at it
   *
   * The display must be in the middle of the view, close to the head, and
   * turned towards the eyes, so that it stays hidden while the weapon is
   * aimed. It appears after \ref ShowDelay and disappears once the player
   * has looked away for \ref HideDelay.
   */
  class VrWristGaze {

  public:

    /// Largest angle between the head's forward axis and the display, in radians (30 degrees)
    constexpr static float MaxAngle    = 0.5235988f;
    /// Largest distance between the head and the display, in metres
    constexpr static float MaxDistance = 0.8f;
    /// Largest angle between the display's normal and the line to the head, in radians (60 degrees)
    constexpr static float MaxFacingAngle = 1.0471976f;
    /// How long the display is looked at before it appears, in seconds
    constexpr static float ShowDelay   = 0.1f;
    /// How long the player looks away before it disappears, in seconds
    constexpr static float HideDelay   = 0.4f;

    /**
     * \brief Updates the display's visibility for a frame
     *
     * \param [in] head Head pose, \c nullptr if it is not tracked
     * \param [in] panel Display pose in the same space, \c nullptr if the
     *    hand is not tracked
     * \param [in] dt Time since the previous frame, in seconds
     * \returns \c true if the display is shown
     */
    bool update(const VrPose* head, const VrPose* panel, float dt);

    void reset();

  private:

    bool  m_visible  = false;
    float m_lookTime = 0.0f;
    float m_awayTime = 0.0f;

  };

  /**
   * \brief Chooses the hand the interaction prompt is shown at
   *
   * The hand nearest the target is chosen. The prompt only moves to the
   * other hand once that one is clearly nearer, so that it does not jump
   * between the hands while they are about as far.
   */
  class VrPromptHand {

  public:

    /// How much nearer the other hand must be before the prompt moves, in metres
    constexpr static float SwitchMargin = 0.05f;

    /**
     * \brief Chooses the hand for a frame
     *
     * \param [in] controllers Hand controllers, in tracking space
     * \param [in] target Target in tracking space
     * \param [out] hand The chosen hand
     * \returns \c false if no hand is tracked
     */
    bool update(
      const std::array<VrControllerState, VrHandCount>& controllers,
      const VrVector3&            target,
            VrHand&               hand);

  private:

    VrHand m_hand = VrHand::Right;

  };

}
