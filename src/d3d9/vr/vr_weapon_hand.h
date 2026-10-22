#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "vr_game_nodes.h"
#include "vr_magazine.h"
#include "vr_melee.h"
#include "vr_pipboy_size.h"
#include "vr_reload_animation.h"
#include "vr_weapon_aim.h"

namespace dxvk {

  /**
   * \brief What the first-person model shows in a frame
   */
  enum class VrFirstPersonMode {
    /// The weapon in the right hand, the left hand free or on the weapon
    Hands,
    /// The left forearm carries the Pip-Boy, the right hand is hidden
    PipBoy,
    /// Nothing is drawn: the controllers point at a menu with their lasers,
    /// and the game would otherwise animate the arms in front of its camera
    Hidden,
  };

  /**
   * \brief Puts the first-person weapon in the hand and shoots where it points
   *
   * Two calls of the game are redirected, see findings/weapon.md:
   *
   * - Main::PlaceCamera draws the first-person model (arms and weapon)
   *   around the origin and updates it right before it is culled. After
   *   that update the whole model is moved so that its weapon node lands
   *   on the hand, turned from the game's camera to the hand, and updated
   *   again. The first-person camera is moved to the eye, so that the model
   *   is seen with the right parallax in each eye. The left forearm is
   *   then moved so that the left hand lands on the left controller, unless
   *   the left hand holds the weapon: the left hand then stays where the
   *   game animates it on the weapon, and a gun points along the line
   *   between the hands. A two-handed melee weapon still follows the right
   *   hand alone, which swings it. The meshes skinned to the upper arms
   *   are culled, so that only the hands are drawn.
   * - TESObjectWEAP::Fire creates the player's projectiles from the muzzle
   *   the game animated in front of its camera. They start from the muzzle
   *   drawn in the hand instead, along the barrel as it is drawn, with the
   *   game's spread kept. The game's auto-aim, which would turn them
   *   towards the point under the centre of its camera, the head, leaves
   *   them alone.
   * - AnimData::MorphOrBlendToSequence starts the game's animations. When
   *   the other hand supports a pistol, the game's aim animation starts
   *   without blending from the previous pose, so the left hand is on the
   *   weapon at once instead of reaching for it.
   * - While the Pip-Boy is shown, the model stays where the game puts it,
   *   the left forearm, which carries the Pip-Boy, is moved to the left
   *   controller as the free hand is, and the right hand and the weapon
   *   are hidden. RenderPipBoy updates the model again before it draws the
   *   Pip-Boy screen, so the forearm is moved again after that update.
   * - While a menu is pointed at with the lasers, the whole model is
   *   hidden.
   * - The weapon's magazine, a part of its model, is drawn in the left
   *   hand while the left hand holds it, sliding along its well while it
   *   is in it, and hidden while it is neither in the hand nor in the
   *   weapon, see VrMagazineHand. The game's reload animation of the
   *   weapon tells how the hand holds it and along which direction it
   *   comes out, see vrReadReloadAnimation. A weapon loaded round by
   *   round, or a break action, has the round its reload animation
   *   carries in in the place of the magazine: it is drawn between the
   *   thumb and the index finger of the left hand, shown even if the game
   *   hides it, and goes into the place the animation puts it.
   * - The weapon's rack part (slide, charging handle, bolt) is drawn back
   *   while the left hand pulls it, see VrRackHand, and locked back while
   *   nothing is chambered, along the way the reload animation pulls it,
   *   the left hand holding it as the animation does.
   *
   * Only used on the game's main thread.
   */
  class VrWeaponHand {

  public:

    ~VrWeaponHand();

    /**
     * \brief Redirects the game's calls
     * \returns \c nullptr if the executable is not the supported version
     */
    static std::unique_ptr<VrWeaponHand> install();

    /**
     * \brief Sets the poses for the frame about to be drawn
     *
     * \param [in] gameCamera Camera the game placed the first-person model
     *    for, before the VR layer replaced it
     * \param [in] hand Pose of the hand holding the weapon in the game world,
     *    or \c nullptr if the weapon stays where the game puts it
     * \param [in] otherHand Pose of the other hand in the game world, or
     *    \c nullptr if it is not tracked
     * \param [in] grip Whether the other hand's grip on the weapon is held:
     *    it takes the weapon once it comes near it, and lets go on release
     * \param [in] mode What the model shows. \c hand is ignored unless it
     *    is VrFirstPersonMode::Hands.
     * \param [in] dt Time since the previous frame, in seconds
     */
    void beginFrame(
      const VrGameCameraPose&     gameCamera,
      const VrGameCameraPose*     hand,
      const VrGameCameraPose*     otherHand,
            bool                  grip,
            VrFirstPersonMode     mode,
            float                 dt);

    /**
     * \brief Sets how far the fingers of a hand are curled
     *
     * Only a free hand follows them: the left hand away from the weapon and
     * the Pip-Boy forearm, the right hand with no weapon at all. Without
     * them the left hand stays open and the right hand keeps the game's
     * fist.
     *
     * \param [in] hand The hand
     * \param [in] curls The fingers' curls, or \c nullptr if they are not
     *    tracked
     */
    void setFingers(VrHand hand, const VrFingerCurls* curls) {
      m_hasFingers[uint32_t(hand)]  = curls != nullptr;
      m_fingerCurls[uint32_t(hand)] = curls ? *curls : VrFingerCurls();
    }

    /**
     * \brief Called before the game places the first-person camera for an eye
     *
     * \param [in] main The game's Main object
     * \param [in] eyePosition Position of the eye being drawn, in the game world
     */
    void beforePlaceCamera(
            void*                 main,
      const VrVector3&            eyePosition);

    /**
     * \brief Whether the game's aim control should be pressed
     *
     * True while the other hand supports a one-handed weapon: the game
     * only holds those with both hands while it aims.
     */
    bool wantsGameAim() const {
      return m_braced;
    }

    /**
     * \brief Where the Pip-Boy's screen is relative to the left controller
     *
     * The centre of the screen when the left forearm takes its free pose
     * around the controller, as it does while the Pip-Boy is shown. Read
     * from the model each time it is drawn, so it is known before the
     * Pip-Boy is first opened.
     *
     * \returns The point in metres along the controller's forward, up and
     *    right axes, or \c nullptr if the screen was not found
     */
    const VrVector3* pipBoyScreen() const {
      return m_hasPipBoyScreen ? &m_pipBoyScreen : nullptr;
    }

    /**
     * \brief Sets the size of the Pip-Boy on the left forearm
     *
     * The Pip-Boy grows about the point of the forearm's axis below the
     * centre of its screen, so that it stays around the forearm. Applied
     * when the model is next drawn.
     *
     * \param [in] factor Size as a factor of the game's size
     */
    void setPipBoyScale(float factor) {
      m_pipBoyScale = factor;
    }

    /**
     * \brief Blade of the melee weapon in the right hand
     *
     * Measured from the weapon's bound once the game has settled the weapon
     * in its idle pose, see vrMeasureBlade and VrHeldRotationSettle, and
     * given in the hand's frame, so
     * that it can be placed on the hand pose of a later frame. The weapon
     * is then held still in the hand, so the blade does not change.
     *
     * \returns The blade, or \c nullptr if the hand holds no one- or
     *    two-handed melee weapon
     */
    const VrBlade* blade() const {
      return m_hasBlade ? &m_blade : nullptr;
    }

    /**
     * \brief Whether the melee weapon of the blade is automatic
     *
     * Automatic melee weapons (chainsaw, thermic lance...) run while the
     * attack control is held, see findings/melee.md §11.
     */
    bool bladeIsAutomatic() const {
      return m_hasBlade && m_bladeAutomatic;
    }

    /**
     * \brief Sets where the magazine of the weapon in the hand is drawn
     *
     * Applied when the model is next drawn. In the left hand, the magazine
     * is held as the game's reload animation holds it, with the fingers
     * closed on it. Between the hand and the well it moves smoothly.
     *
     * \param [in] state Where the magazine is
     * \param [in] seat How far out of its place it is along its well, in
     *    metres, while it is in the well
     */
    void setMagazine(VrMagazineState state, float seat);

    /**
     * \brief Sets how the left hand loads a weapon loaded round by round
     *
     * Applied when the model is next drawn. A hand holding a handful holds
     * its round between the thumb and the index finger, at its controller.
     * While it loads, a round goes from there into the loading port in
     * each loop of the game's loading animation, and comes to its place
     * when the loop's round does; the next one is in the fingers when the
     * loop starts again.
     *
     * \param [in] state Where the handful is
     * \param [in] time Time in the loading loop, while it loads
     */
    void setRoundLoop(VrRoundState state, float time);

    /**
     * \brief Where a round enters the loading port
     *
     * Its place, out along the way it goes in by a few centimetres, in
     * the right hand's frame (see vrPointFromHand), measured each time the
     * model is drawn.
     *
     * \returns The point, or \c nullptr if the weapon in the hand is not
     *    loaded round by round with a loading loop
     */
    const VrVector3* roundPort() const {
      return m_hasRoundPort ? &m_roundPort : nullptr;
    }

    /**
     * \brief Where the magazine sits in the weapon in the right hand
     *
     * The centre of the magazine's meshes where the game draws it, measured
     * each time the model is drawn, given in the right hand's frame (see
     * vrPointFromHand) so that it can be placed on the hand pose of a
     * later frame.
     *
     * \returns The point, or \c nullptr if the weapon in the hand has no
     *    magazine
     */
    const VrVector3* magazineWell() const {
      return m_hasMagazine ? &m_magazineWell : nullptr;
    }

    /**
     * \brief Direction the magazine comes out of the weapon along
     *
     * A unit vector in the right hand's frame (forward, up and right),
     * valid with magazineWell.
     */
    const VrVector3& magazineAxis() const {
      return m_magazineAxis;
    }

    /**
     * \brief Where the centre of a magazine held by the left hand is
     *
     * In the left controller's frame (see vrPointFromHand), valid with
     * magazineWell. A round is held between the thumb and the index
     * finger.
     */
    const VrVector3& magazineHeld() const {
      return m_magazineHeld;
    }

    /**
     * \brief Whether the magazine is the round the weapon fires
     *
     * A launcher's missile or mini-nuke, valid with magazineWell: it is
     * never taken out, and leaves the weapon when it is fired.
     */
    bool magazineFired() const {
      return m_magazineFired;
    }

    /**
     * \brief Sets where the rack part of the weapon in the hand is drawn
     *
     * Applied when the model is next drawn.
     *
     * \param [in] held The left hand holds it
     * \param [in] pull How far back the hand holds it, in metres
     * \param [in] rest How far back it rests while the hand does not hold
     *    it, in metres: all the way back while nothing is chambered, part
     *    of the way while the weapon is jammed
     */
    void setRack(bool held, float pull, float rest);

    /**
     * \brief Where the rack part's centre is when it is home
     *
     * In the right hand's frame (see vrPointFromHand), measured each time
     * the model is drawn.
     *
     * \returns The point, or \c nullptr if the weapon in the hand is not
     *    reloaded by hand or has no rack part
     */
    const VrVector3* rackPart() const {
      return m_hasRack ? &m_rackCentre : nullptr;
    }

    /**
     * \brief Direction the rack part is pulled back along
     *
     * A unit vector in the right hand's frame, valid with rackPart.
     */
    const VrVector3& rackAxis() const {
      return m_rackAxis;
    }

    /**
     * \brief How far the rack part is pulled back, in metres
     */
    float rackTravel() const {
      return m_rackTravel;
    }

    /**
     * \brief Sets how far the lock over the magazine is turned open
     *
     * Applied when the model is next drawn, from 0 (shut, as the game's
     * reload animation starts with it) to 1 (as open as it turns it).
     *
     * \param [in] turn How far it is turned
     * \param [in] driven The weapon is reloaded by hand: the lock is drawn
     *    as it is turned, the game's pose of it is left as it is otherwise
     */
    void setLock(float turn, bool driven) {
      m_lockTurn   = turn;
      m_lockDriven = driven;
    }

    /**
     * \brief Where the lock over the magazine turns about
     *
     * In the right hand's frame (see vrPointFromHand), measured each time
     * the model is drawn.
     *
     * \returns The point, or \c nullptr if the weapon in the hand is not
     *    reloaded by hand or has no lock its reload animation turns
     */
    const VrVector3* lockPivot() const {
      return m_hasLock ? &m_lockPivot : nullptr;
    }

    /**
     * \brief Axis the lock turns about to open
     *
     * A unit vector in the right hand's frame, valid with lockPivot.
     */
    const VrVector3& lockAxis() const {
      return m_lockAxis;
    }

    /**
     * \brief From the lock's pivot to its middle, shut
     *
     * In the right hand's frame, in game units, valid with lockPivot.
     */
    const VrVector3& lockArm() const {
      return m_lockArm;
    }

    /**
     * \brief How far the lock turns open, in radians
     */
    float lockAngle() const {
      return m_reloadAnimation.lock.angle;
    }

    /**
     * \brief Opens or closes the action of the weapon in the hand
     *
     * From the next frame on, the barrel of a break action swings down
     * about the hinge, the stock staying in the hand, and a revolver's
     * cylinder or loading gate swings out, as far as the game's reload
     * animation opens them.
     */
    void setActionOpen(bool open) {
      m_actionOpen = open;
    }

    /**
     * \brief Sets the parts of the weapon in the hand that glow
     *
     * Applied when the model is next drawn, see vrPartsToWork. A part
     * glows only if the weapon has it and it is drawn.
     */
    void setHighlight(const VrReloadParts& parts) {
      m_highlight = parts;
    }

    /**
     * \brief Takes the parts to glow in the eye being drawn
     *
     * Each placement of the model, once per eye, finds them again in the
     * model, so they are only taken once.
     *
     * \param [out] root The first-person model's root
     * \param [out] parts The parts that glow
     * \param [out] drawn The model's world transforms as it was drawn:
     *    once it is drawn, the game updates it again where it animates it
     * \returns \c false if nothing glows in this eye
     */
    bool takeHighlight(uint8_t*& root, std::vector<uint8_t*>& parts, VrGameWorldTransforms& drawn);

    /**
     * \brief Direction the barrel of a break action points along from its
     *    breech
     *
     * A unit vector in the right hand's frame, measured each time the
     * model is drawn, as it is drawn (open or closed). The breech is
     * magazineWell.
     *
     * \returns The direction, or \c nullptr if the weapon in the hand is
     *    not a break action reloaded by hand
     */
    const VrVector3* actionBarrel() const {
      return m_hasMagazine && m_hasHinge && !m_swingAction ? &m_actionBarrel : nullptr;
    }

    /**
     * \brief Axis a revolver in the hand turns about when it is flicked
     *    open
     *
     * Along its barrel, the way its cylinder or loading gate swings out: a
     * unit vector in the right hand's frame, measured each time the model
     * is drawn.
     *
     * \returns The axis, or \c nullptr if the weapon in the hand is not a
     *    revolver reloaded by hand
     */
    const VrVector3* actionRoll() const {
      return m_hasMagazine && m_hasHinge && m_swingAction ? &m_actionRoll : nullptr;
    }

    /**
     * \brief The game's reload animation of the weapon in the hand
     *
     * \returns The animation, or \c nullptr if it was not read
     */
    const VrReloadAnimation* reloadAnimation() const {
      return m_hasMagazine && m_hasReloadAnimation ? &m_reloadAnimation : nullptr;
    }

    /**
     * \brief Where the left hand loads a weapon with no part to take out
     *
     * A flamer or an incinerator: where the game's reload animation puts
     * the left hand on the weapon, for the left controller's hand bone
     * (see fuelHeld), in the right hand's frame (see vrPointFromHand),
     * measured each time the model is drawn.
     *
     * \returns The point, or \c nullptr if the weapon in the hand is not
     *    loaded that way
     */
    const VrVector3* fuelPort() const {
      return m_hasFuelPort ? &m_fuelPort : nullptr;
    }

    /**
     * \brief Where the left hand bone is in the left controller's frame
     *
     * The point brought to fuelPort.
     */
    VrVector3 fuelHeld() const;

    /**
     * \brief Where the reload animation loads a weapon with no part to take
     *    out, and its sounds
     *
     * \returns The animation, or \c nullptr without a fuelPort
     */
    const VrFuelAnimation* fuelAnimation() const {
      return m_hasFuelPort ? &m_fuelAnimation : nullptr;
    }

  private:

    /**
     * \brief How the action of the weapon opens to be loaded
     */
    enum class ActionKind {
      None,           ///< It does not open: a magazine, or a loading port
      Break,          ///< A break action: the barrel turns down about the breech
      Swing,          ///< A revolver: its cylinder or loading gate swings out
    };

    enum class GripKind {
      None,           ///< The other hand does not hold the weapon
      TwoHanded,      ///< Held with both hands, the line between them aims
      TwoHandedMelee, ///< Held with both hands, the right hand alone swings it
      Braced,         ///< One-handed, supported while the game aims
    };

    /**
     * \brief Nodes of the left forearm that carry the Pip-Boy
     *
     * The Pip-Boy is a rigid model attached to the forearm's twist bone,
     * its screen one of its parts, see findings/pipboy.md §3.8.
     */
    struct PipBoyNodes {
      uint8_t* hand    = nullptr;
      uint8_t* forearm = nullptr;
      uint8_t* twist   = nullptr;
      /// Root of the Pip-Boy model, a child of the twist bone
      uint8_t* device  = nullptr;
      uint8_t* screen  = nullptr;
    };

    VrWeaponHand() = default;

    VrGameCameraPose m_gameCamera;
    VrGameCameraPose m_hand;
    bool             m_hasHand = false;
    VrGameCameraPose m_otherHand;
    bool             m_hasOtherHand = false;

    std::array<VrFingerCurls, VrHandCount> m_fingerCurls = { };
    std::array<bool, VrHandCount>          m_hasFingers  = { };
    bool             m_grip = false;
    VrFirstPersonMode m_mode = VrFirstPersonMode::Hands;

    // Whether the other hand holds the weapon
    bool             m_twoHanded = false;
    bool             m_braced = false;

    // Whether the aim animation of a new brace still has to skip its blend:
    // only the first one, so that returning to the aim after a shot keeps
    // the game's blend. Cleared at the frame after it was skipped.
    bool             m_snapAim = false;
    bool             m_snappedAim = false;

    // Set before PlaceCamera, used by the update it makes
    uint8_t*         m_main = nullptr;
    VrVector3        m_rootPosition;
    VrVector3        m_eyePosition;

    // Offset from the world to the model's drawing space, set when the
    // forearm carrying the Pip-Boy was moved for the eye being drawn, for
    // RenderPipBoy's update
    VrVector3        m_pipBoyShift;
    bool             m_pipBoyPlaced = false;

    // Centre of the Pip-Boy's screen in the left controller's frame
    VrVector3        m_pipBoyScreen;
    bool             m_hasPipBoyScreen = false;

    // Size asked for the Pip-Boy, and the model it was applied to with the
    // model's own placement, compared but only dereferenced once found in
    // the model again. The written placement tells whether the game has
    // placed the model again since.
    float            m_pipBoyScale = 1.0f;
    uint8_t*         m_pipBoyDevice = nullptr;
    VrNodeScale      m_pipBoyOwnPlacement;
    VrNodeScale      m_pipBoyWrittenPlacement;
    float            m_pipBoyAppliedScale = 1.0f;

    // Muzzle relative to the first-person root, and the direction the
    // hand pointed, when the weapon was last drawn in the hand
    VrVector3        m_muzzleOffset;
    VrVector3        m_aimDirection;
    bool             m_hasAim = false;

    // Melee weapon held still in the hand, see placeModel: the weapon node
    // and form it was drawn with, compared but not dereferenced, and its
    // rotation relative to the hand once the game has settled it. The model
    // is placed once per eye, the settling is followed once per frame.
    const uint8_t*       m_heldWeapon = nullptr;
    const uint8_t*       m_heldForm   = nullptr;
    VrHeldRotationSettle m_heldSettle;
    bool                 m_heldSettleUpdated = false;
    float                m_frameTime = 0.0f;

    // Blade of the held melee weapon, in the hand's frame, and whether the
    // weapon is in the hand this frame
    VrBlade          m_blade;
    bool             m_bladeMeasured = false;
    bool             m_hasBlade = false;
    bool             m_bladeAutomatic = false;

    // Magazine of the weapon in the hand: where it is drawn, where it sits
    // in the weapon in the right hand's frame, the centre of its meshes in
    // its own frame, its rotation relative to the left controller while
    // held (for the weapon form it was taken from, compared but not
    // dereferenced) and the magazine the VR layer culled, compared but only
    // touched once found in the weapon again
    VrMagazineState  m_magazineState = VrMagazineState::InWeapon;
    float            m_magazineSeat = 0.0f;
    VrVector3        m_magazineWell;
    VrVector3        m_magazineAxis;
    VrVector3        m_magazineHeld;
    bool             m_hasMagazine = false;
    bool             m_magazineFired = false;
    VrVector3        m_magazineCentre;
    bool             m_hasMagazineCentre = false;
    const uint8_t*   m_magazineForm = nullptr;
    VrGameRotation   m_magazineGrip;
    bool             m_hasMagazineGrip = false;
    uint8_t*         m_hiddenMagazine = nullptr;
    // A round the game hides, shown by the VR layer while it is held
    uint8_t*         m_shownRound = nullptr;
    bool             m_loggedMagazine = false;

    // Where the magazine was last drawn out of the weapon, in the game
    // world, and the blend from there when it moves between the hand and
    // the well (1 when done)
    VrGameTransform  m_magazineDrawn;
    bool             m_hasMagazineDrawn = false;
    VrGameTransform  m_magazineFrom;
    float            m_magazineBlend = 1.0f;

    // Parts of the weapon to glow, and those found at the last placement
    // of the model, in the model below its root, with the model as placed
    VrReloadParts         m_highlight;
    uint8_t*              m_highlightRoot = nullptr;
    std::vector<uint8_t*> m_highlightParts;
    VrGameWorldTransforms m_highlightDrawn;

    // The left hand loading a weapon round by round: where its handful
    // is, the time in the loading loop, and where a round enters the port
    VrRoundState     m_roundState = VrRoundState::Empty;
    float            m_roundTime  = 0.0f;
    VrVector3        m_roundPort;
    bool             m_hasRoundPort = false;

    // The magazine is a round, held between the thumb and the index
    // finger: the round in the left hand bone's frame then, in units of
    // the hand's scale
    bool             m_pinched = false;
    VrGameTransform  m_pinchInHand;
    bool             m_hasPinch = false;

    // Rack part of the weapon in the hand: how the hand holds it, where it
    // is home in the right hand's frame, the way back and its length, and
    // its rack part the name was logged for, compared but not dereferenced
    bool             m_rackHeld = false;
    float            m_rackPull = 0.0f;
    float            m_rackRest = 0.0f;
    VrVector3        m_rackCentre;
    VrVector3        m_rackAxis;
    float            m_rackTravel = 0.0f;
    bool             m_hasRack = false;
    const uint8_t*   m_loggedRack = nullptr;

    // Action that opens in the hand: whether it is to be open, how far it
    // is open (0 closed, 1 open), whether the weapon has one, whether it
    // is a revolver's, the direction of a break action's barrel and the
    // axis a revolver is flicked open about, in the right hand's frame,
    // and the turn of the hinge part in the weapon node's frame as drawn
    bool             m_actionOpen = false;
    float            m_actionSwing = 0.0f;
    bool             m_hasHinge = false;
    bool             m_swingAction = false;
    VrVector3        m_actionBarrel;
    VrGameRotation   m_actionTurn;
    VrVector3        m_actionRoll;

    // The shells of a break action besides the one found as its round: the
    // caravan shotgun has one part per shell. Each one's pose relative to
    // that round, for the weapon form they were measured on (compared but
    // not dereferenced), and those the VR layer culled, compared but only
    // touched once found in the weapon again.
    struct OtherShell {
      uint8_t*        part;
      VrGameTransform fromRound;
    };

    std::vector<OtherShell> m_otherShells;
    const uint8_t*   m_otherShellsForm = nullptr;
    std::vector<uint8_t*> m_hiddenShells;

    // The game's reload animation of the weapon form it was read for,
    // compared but not dereferenced, how many times it was tried for it and
    // how many draws to wait before the next try
    VrReloadAnimation m_reloadAnimation;
    bool             m_hasReloadAnimation = false;
    const uint8_t*   m_reloadForm = nullptr;
    uint32_t         m_reloadTries = 0;
    uint32_t         m_reloadWait = 0;

    // The same for a weapon loaded at a point with no part (a flamer), and
    // that point in the right hand's frame
    VrFuelAnimation  m_fuelAnimation;
    bool             m_hasFuelAnimation = false;
    const uint8_t*   m_fuelForm = nullptr;
    uint32_t         m_fuelTries = 0;
    uint32_t         m_fuelWait = 0;
    VrVector3        m_fuelPort;
    bool             m_hasFuelPort = false;

    // Lock over the magazine: how far it is to be turned open and whether
    // it is drawn so, its middle in its own frame, and where it turns
    // about, its axis and its arm (shut) in the right hand's frame
    float            m_lockTurn = 0.0f;
    bool             m_lockDriven = false;
    VrVector3        m_lockCentre;
    bool             m_hasLockCentre = false;
    VrVector3        m_lockPivot;
    VrVector3        m_lockAxis;
    VrVector3        m_lockArm;
    bool             m_hasLock = false;
    bool             m_loggedLock = false;

    // Set while the game creates a projectile aimed with the hand, so that
    // its auto-aim leaves it alone
    bool             m_handAimedShot = false;

    // Weapon node whose barrel angle was last logged, compared but not
    // dereferenced
    const uint8_t*   m_loggedBarrelWeapon = nullptr;

    // Weapon whose parts were last logged, compared but not dereferenced,
    // and the number of objects of its model then
    const uint8_t*   m_loggedPartsForm    = nullptr;
    uint32_t         m_loggedPartsObjects = 0;

    bool             m_loggedActive = false;
    bool             m_loggedFrame  = false;
    bool             m_loggedArms   = false;
    bool             m_loggedOtherHand = false;
    bool             m_loggedTwoHanded = false;
    bool             m_loggedTwoHandedMelee = false;
    bool             m_loggedBraced = false;
    bool             m_loggedSnapAim = false;
    bool             m_loggedPipBoy = false;
    bool             m_loggedRightHand = false;
    bool             m_loggedFist = false;
    bool             m_loggedScreen = false;
    bool             m_loggedPipBoyScale = false;
    bool             m_loggedAutoAim = false;

    // Objects culled by the VR layer, compared but not dereferenced until
    // they are found in the model again, and whether the right hand and the
    // weapon are among them
    std::vector<uint8_t*> m_hiddenObjects;
    bool             m_hidesRightHand = false;
    bool             m_hidesModel = false;
    bool             m_loggedHidden = false;

    static void __fastcall updateHook(
            uint8_t*              object,
            void*                 unused,
            void*                 updateData);

    static void __fastcall pipBoyUpdateHook(
            uint8_t*              object,
            void*                 unused,
            void*                 updateData);

    static void* __cdecl createProjectileHook(
            void* projectile, void* shooter, void* target, void* weapon,
            float x, float y, float z, float heading, float pitch,
            uint32_t a9, uint32_t a10, uint32_t a11, uint32_t a12,
            float spreadHeading, float spreadPitch, uint32_t a15);

    static void __fastcall autoAimHook(
            uint8_t*              player,
            void*                 unused,
            void*                 projectile,
            void*                 releaseNode,
            float*                heading,
            float*                pitch,
            void*                 origin,
            float                 startHeading,
            float                 startPitch);

    static void* __fastcall playSequenceHook(
            uint8_t*              animData,
            void*                 unused,
            void*                 sequence,
            uint32_t              groupId,
            int32_t               sequenceType);

    void placeModel(uint8_t* root, void* updateData);

    void placePipBoyArm(uint8_t* root, const VrVector3& shift, void* updateData);

    static PipBoyNodes findPipBoy(uint8_t* root);

    void measurePipBoyScreen(const PipBoyNodes& nodes);

    void scalePipBoy(const PipBoyNodes& nodes);

    void updateGrip(GripKind kind, bool canHold, const VrVector3& support);

    void measureBlade(const uint8_t* weapon, const VrGameCameraPose& hand);

    void findHighlight(uint8_t* root, uint8_t* weapon, uint8_t* hinge, uint8_t* lock, bool swing);

    /**
     * \brief A part of the weapon the left hand holds as the reload
     *    animation does
     */
    struct HeldPart {
      /// Where the part is drawn
      VrGameTransform                pose;
      /// The part in the hand bone's frame, in units of the hand's scale
      const VrGameTransform*         inHand;
      /// Rotations of the bones below the hand
      const std::vector<VrBonePose>* fingers;
    };

    /**
     * \brief Puts the free left hand around its controller
     *
     * \param [in] held The part the hand holds where it is drawn, or
     *    \c nullptr
     */
    void placeOtherHand(
            uint8_t*              root,
            uint8_t*              otherHand,
      const VrVector3&            shift,
            void*                 updateData,
      const HeldPart*             held);

    /**
     * \brief Finds how the action of the weapon opens
     *
     * A break action is not loaded round by round, has no magazine part
     * and has a hinge part. A revolver (a one-handed pistol) has a speed
     * loader, or is loaded round by round through a loading gate.
     *
     * \param [out] kind How it opens
     * \returns The hinge part or the loading gate, or \c nullptr if there
     *    is none or the reload animation tells which part swings out
     */
    static uint8_t* findAction(uint8_t* weapon, const uint8_t* form, ActionKind& kind);

    /**
     * \brief Draws the action as far open as it is
     *
     * The hinge part turns in the weapon as the reload animation turns
     * it, and the weapon stays in the hand: a break action's hinge part,
     * named as a stock, carries the barrel, the shells and the muzzle,
     * and a revolver's swings its cylinder out or is its loading gate.
     *
     * \param [in] hinge The part found by findAction
     */
    void openAction(
            uint8_t*              weapon,
            uint8_t*              hinge,
            ActionKind            kind,
      const uint8_t*              form,
            void*                 updateData);

    /**
     * \brief Finds where the left hand holds a round between its thumb
     *    and its index finger
     *
     * The middle of their last joints, with the fingers posed as they are
     * on a held part, in the hand bone's frame: the round keeps the
     * rotation in the hand the reload animation gives it, its centre goes
     * there. Sets m_pinchInHand.
     *
     * \param [in] centre The centre of the round's meshes in its frame
     * \param [in] scale The round's scale over the hand's
     */
    void pinchRound(uint8_t* otherHand, const VrVector3& centre, float scale);

    /**
     * \brief The magazine, or round, in the left hand bone's frame while
     *    the hand holds it
     */
    const VrGameTransform& magazineInHand() const {
      return m_pinched && m_hasPinch ? m_pinchInHand : m_reloadAnimation.magazineInHand;
    }

    /**
     * \brief Where the magazine held by the left hand is drawn, the hand
     *    taking its free pose around its controller
     *
     * \param [in] handScale The left hand bone's scale
     * \param [in] shift Offset from the world to the model's drawing space
     */
    VrGameTransform magazineAtController(float handScale, const VrVector3& shift) const;

    /**
     * \brief Culls the cartridges in a revolver's cylinder while it is
     *    emptied
     *
     * Its live rounds and spent cases, not those of its speed loader, from
     * the time it is swung open until a speed loader is pushed home.
     */
    void hideCylinder(uint8_t* weapon);

    /**
     * \brief Draws the other shells of a break action with its round
     *
     * In the weapon they are where the game draws them, culled while the
     * action is emptied, and in the hand they keep their place next to the
     * round.
     *
     * \param [in] round The round part
     * \param [in] pose Where the round is drawn, or \c nullptr if the game
     *    draws it in the weapon or it is not drawn
     */
    void placeOtherShells(
            uint8_t*              weapon,
            uint8_t*              round,
      const uint8_t*              form,
      const VrGameTransform*      pose,
            void*                 updateData);

    /**
     * \brief Finds the weapon's magazine and where it is drawn
     *
     * The magazine of a break action (with a hinge) is its shells, that of
     * a revolver its speed loader, or its round with a loading gate.
     * \param [out] pose Where the magazine is drawn, in the model's
     *    drawing space
     * \returns The magazine to draw at the pose, or \c nullptr if the
     *    game draws it in the weapon or it is not drawn
     */
    uint8_t* placeMagazine(
            uint8_t*              root,
            uint8_t*              weapon,
            ActionKind            action,
            uint8_t*              hinge,
            uint8_t*              leftHand,
            bool                  handPlaced,
      const VrGameCameraPose&     hand,
      const uint8_t*              form,
      const VrVector3&            shift,
            VrGameTransform&      pose);

    /**
     * \brief Finds the weapon's rack part and where it is drawn
     *
     * \param [out] pose Where the rack part is drawn, in the model's
     *    drawing space
     * \returns The rack part to draw at the pose, or \c nullptr if the
     *    game draws it
     */
    uint8_t* placeRack(
            uint8_t*              weapon,
      const VrGameCameraPose&     hand,
            VrGameTransform&      pose);

    /**
     * \brief Finds where the left hand loads a heavy weapon with no part
     *    to take out or put in
     */
    void placeFuelPort(
            uint8_t*              root,
            uint8_t*              weapon,
      const uint8_t*              form,
            uint8_t*              leftHand,
      const VrGameCameraPose&     hand);

    /**
     * \brief Finds the lock over the weapon's magazine and draws it as far
     *    open as it is turned
     *
     * \returns The lock, or \c nullptr if the weapon has none to turn by
     *    hand
     */
    uint8_t* placeLock(
            uint8_t*              weapon,
      const VrGameCameraPose&     hand,
            void*                 updateData);

    /**
     * \brief Draws a part of the weapon at a pose for this update only
     */
    void drawPart(
            uint8_t*              part,
      const VrGameTransform&      pose,
            void*                 updateData);

    bool holdsMagazine() const;

    bool holdsRack() const;

    void hideArms(uint8_t* root, bool rightHand);

    void hideModel(uint8_t* root);

    void hide(uint8_t* object, bool log);

    void showArms(uint8_t* root);

    bool holdsWeapon(const uint8_t* player) const;

    const uint8_t* weaponForm(const uint8_t* player) const;

    int8_t weaponType(const uint8_t* player) const;

    static bool isMelee(const uint8_t* weapon);

    GripKind weaponGrip(const uint8_t* player) const;

  };

}
