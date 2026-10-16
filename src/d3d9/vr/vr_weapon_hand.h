#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "vr_melee.h"
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

  private:

    enum class GripKind {
      None,           ///< The other hand does not hold the weapon
      TwoHanded,      ///< Held with both hands, the line between them aims
      TwoHandedMelee, ///< Held with both hands, the right hand alone swings it
      Braced,         ///< One-handed, supported while the game aims
    };

    VrWeaponHand() = default;

    VrGameCameraPose m_gameCamera;
    VrGameCameraPose m_hand;
    bool             m_hasHand = false;
    VrGameCameraPose m_otherHand;
    bool             m_hasOtherHand = false;
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

    // Set while the game creates a projectile aimed with the hand, so that
    // its auto-aim leaves it alone
    bool             m_handAimedShot = false;

    // Weapon node whose barrel angle was last logged, compared but not
    // dereferenced
    const uint8_t*   m_loggedBarrelWeapon = nullptr;

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
    bool             m_loggedScreen = false;
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

    void measurePipBoyScreen(uint8_t* root);

    void updateGrip(GripKind kind, bool canHold, const VrVector3& support);

    void measureBlade(const uint8_t* weapon, const VrGameCameraPose& hand);

    void placeOtherHand(
            uint8_t*              root,
            uint8_t*              otherHand,
      const VrVector3&            shift,
            void*                 updateData);

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
