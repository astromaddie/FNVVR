#pragma once

namespace dxvk {

  /**
   * \brief How a crouch of the player's body is told from standing
   */
  struct VrCrouchConfig {
    /// How far the head goes below its standing height to crouch, in metres
    float depth      = 0.35f;
    /// How much higher the head comes back up to stand, in metres
    float hysteresis = 0.10f;
  };

  /**
   * \brief Tells from the head's height whether the player crouches
   *
   * The standing height is the height the head was last held at upright:
   * it starts at the first height and follows the head up, slowly, so that
   * a jump or standing on tiptoe barely moves it, and standing up after
   * the first height was taken seated raises it within a few seconds. It
   * never follows the head down, so a long crouch stays a crouch.
   *
   * The player crouches once the head is \c depth below the standing
   * height, and stands again once it is back within \c depth minus
   * \c hysteresis of it. Heights are measured from any fixed level, the
   * floor of the tracking space for instance: only their differences are
   * used, so a floor set too high or too low changes nothing.
   */
  class VrPhysicalCrouch {

  public:

    /// How fast the standing height follows the head up, in metres per second
    static constexpr float StandingRiseSpeed = 0.1f;

    explicit VrPhysicalCrouch(const VrCrouchConfig& config)
    : m_config(config) { }

    void setConfig(const VrCrouchConfig& config) {
      m_config = config;
    }

    /**
     * \brief Takes the head's height for a frame
     *
     * \param [in] headHeight Height of the head, in metres
     * \param [in] dt Duration of the frame in seconds
     * \returns Whether the player crouches
     */
    bool update(float headHeight, float dt);

    /**
     * \brief Forgets the standing height
     *
     * The next height is taken as the standing height again.
     */
    void reset();

    bool crouched() const {
      return m_crouched;
    }

    /**
     * \brief The standing height, in metres, valid once a height was taken
     */
    float standingHeight() const {
      return m_standing;
    }

  private:

    VrCrouchConfig m_config;
    bool           m_hasStanding = false;
    float          m_standing    = 0.0f;
    bool           m_crouched    = false;

  };


  /**
   * \brief Presses the game's sneak control until the game sneaks as wanted
   *
   * The game's control toggles sneaking when it is pressed, so it is held
   * for \ref PressTime, then released for \ref SettleTime, after which the
   * game's state is compared again. The game refuses to toggle at times
   * (during some animations, sitting, ...): the control is then pressed
   * again once the wait is over.
   */
  class VrSneakToggle {

  public:

    /// How long the control is held, in seconds
    static constexpr float PressTime  = 0.1f;
    /// How long the control is left released after a press, in seconds
    static constexpr float SettleTime = 0.4f;

    /**
     * \brief Decides whether the control is held for a frame
     *
     * \param [in] wanted Whether the player should sneak
     * \param [in] sneaking Whether the game sneaks
     * \param [in] allowed Whether the control may be pressed, such as in
     *    game and not in a menu; a press is cut short when it is not
     * \param [in] dt Duration of the frame in seconds
     * \returns Whether the control is held
     */
    bool update(bool wanted, bool sneaking, bool allowed, float dt);

    void reset();

  private:

    float m_pressTime = 0.0f;
    float m_waitTime  = 0.0f;

  };


  /**
   * \brief Keeps the view at the standing height while the game sneaks
   *
   * The game lowers its first-person camera while the player sneaks. When
   * the player sneaks by crouching, the head is already lower, and the
   * game's own drop would lower the view a second time. The camera's
   * height above the player's position is learned while the game does not
   * sneak (smoothed, so that the walking motion averages out); while it
   * sneaks, and until the camera is back up after it stops, the camera is
   * raised by what the game took off. The game's drop and rise over a
   * transition are cancelled frame by frame, so the view does not move.
   */
  class VrSneakCamera {

  public:

    /// Time constant of the standing height's smoothing, in seconds
    static constexpr float StandingSmoothing = 1.0f;
    /// Largest raise, in game units (about 0.65 m)
    static constexpr float MaxRaise = 45.0f;
    /// Raise under which the camera counts as back up, in game units
    static constexpr float BackUpRaise = 0.5f;
    /// Longest wait for the camera to come back up, in seconds
    static constexpr float MaxRiseTime = 1.5f;

    /**
     * \brief Takes the camera's height for a frame
     *
     * \param [in] height Height of the game's camera above the player's
     *    position in game units, or \c nullptr when the camera does not
     *    follow the player, such as in V.A.T.S. or on furniture: nothing
     *    is learned and the raise is kept for when it follows again
     * \param [in] sneaking Whether the game sneaks
     * \param [in] dt Duration of the frame in seconds
     * \returns How far the camera is raised, in game units
     */
    float update(const float* height, bool sneaking, float dt);

    void reset();

    float raise() const {
      return m_raise;
    }

  private:

    bool  m_hasStanding = false;
    float m_standing    = 0.0f;
    float m_raise       = 0.0f;
    bool  m_rising      = false;
    float m_riseTime    = 0.0f;

  };

}
