#pragma once

#include <atomic>
#include <cstdint>
#include <memory>

#include "../../util/thread.h"

#include "vr_magazine.h"
#include "vr_reload_animation.h"

namespace dxvk {

  /**
   * \brief What the hands do in a frame, for the manual reload
   *
   * Poses in the game world. A pose is \c nullptr when it is not tracked,
   * or when the game is not in play (menus, Pip-Boy, loading).
   */
  struct VrManualReloadInput {
    const VrGameCameraPose* head      = nullptr;
    const VrGameCameraPose* rightHand = nullptr;
    const VrGameCameraPose* leftHand  = nullptr;
    /// The left hand holds or supports the weapon
    bool                    leftOnWeapon = false;
    /// The magazine action (the left trigger) is held
    bool                    grip = false;
    /// Where the magazine sits in the weapon, in the right hand's frame
    /// (VrWeaponHand::magazineWell), or \c nullptr without a magazine
    const VrVector3*        well = nullptr;
    /// Direction the magazine comes out along, in the right hand's frame
    /// (VrWeaponHand::magazineAxis), valid with the well
    VrVector3               axis;
    /// Where a magazine held by the left hand has its centre, in the left
    /// hand's frame (VrWeaponHand::magazineHeld), valid with the well
    VrVector3               held;
    /// The magazine is the round the weapon fires, a launcher's missile or
    /// mini-nuke (VrWeaponHand::magazineFired), valid with the well
    bool                    fired = false;
    /// Where the rack part's centre is home, in the right hand's frame
    /// (VrWeaponHand::rackPart), or \c nullptr without one
    const VrVector3*        rack = nullptr;
    /// Direction it is pulled back along, in the right hand's frame, and
    /// how far in metres, valid with the rack part
    VrVector3               rackAxis;
    float                   rackTravel = 0.0f;
    /// Direction the barrel of a break action points along from its breech
    /// (the well), in the right hand's frame (VrWeaponHand::actionBarrel),
    /// or \c nullptr if the weapon is not a break action
    const VrVector3*        barrel = nullptr;
    /// Where the left controller is as the loading loop puts its round in
    /// its place, in the right hand's frame (VrWeaponHand::roundPort), or
    /// \c nullptr if the weapon is not loaded round by round
    const VrVector3*        port = nullptr;
    /// Where the lock over the magazine turns about, in the right hand's
    /// frame (VrWeaponHand::lockPivot), or \c nullptr without one
    const VrVector3*        lock = nullptr;
    /// Axis it turns about to open, from the pivot to its middle (in game
    /// units) and how far it turns, in radians, valid with the lock
    VrVector3               lockAxis;
    VrVector3               lockArm;
    float                   lockAngle = 0.0f;
    /// Axis a revolver is flicked open about, in the right hand's frame
    /// (VrWeaponHand::actionRoll), or \c nullptr if the weapon is not a
    /// revolver
    const VrVector3*        roll = nullptr;
    /// Time since the last frame, in seconds
    float                   frameTime = 0.0f;
    /// The game's reload animation of the weapon, for its sounds, or
    /// \c nullptr
    const VrReloadAnimation* animation = nullptr;
  };

  /**
   * \brief What the left hand did in a frame, for the manual reload
   */
  struct VrReloadEvents {
    VrMagazineEvent magazine = VrMagazineEvent::None;
    VrRackEvent     rack     = VrRackEvent::None;
    VrBreakEvent    action   = VrBreakEvent::None;
    VrRoundEvent    round    = VrRoundEvent::None;
    VrLockEvent     lock     = VrLockEvent::None;
    /// The weapon jammed since the last frame
    bool            jammed   = false;
  };

  /**
   * \brief Reloading the weapons with the hands
   *
   * Every reload of the player goes through one entry of the
   * PlayerCharacter vtable, which is replaced, see findings/reload.md §3.
   * Each reload is logged with its caller, its arguments and the clip
   * before and after, and the game's jams of the player's weapon are
   * logged where StartAttack looks up their animation.
   *
   * Phase 1 (findings/reload.md §8): weapons with a detachable magazine or
   * cell are reloaded by hand, see VrMagazineHand. For them the reload
   * control and the automatic reload when a shot empties the clip are
   * refused, so an empty clip clicks. Taking the magazine out empties the
   * clip, which loses no round (the clip is a view on the inventory), and
   * pushing one in fills it through the game's own reload with no
   * animation. The magazine goes out and in with the sounds of the
   * weapon's reload animation. Every other reload (equipping, V.A.T.S., an ammunition
   * swap...) is the game's, and puts the magazine back in the weapon.
   *
   * Phase 2 (findings/reload.md §8.3): a weapon with a rack part has a
   * round chambered while its clip is not empty. Taking its magazine out
   * leaves the chambered round (a clip of one). A magazine pushed in with
   * nothing chambered fills the clip only once the weapon is racked, see
   * VrRackHand; until then the rack part is locked back. Racking with the
   * magazine out takes the chambered round out (it stays in the
   * inventory).
   *
   * Phase 3 (findings/reload.md §8.4): when the game rolls a jam of a
   * weapon reloaded by hand, its jam animation is not played and the
   * weapon stays jammed: the game sees an empty clip when an attack
   * starts, so the trigger only clicks, and the rack part rests part of
   * the way back. Racking clears the jam; without a rack part, taking
   * the magazine out does. In V.A.T.S. the game's own jam is kept.
   *
   * Phase 5a (findings/reload.md §8.7, §8.10): a weapon loaded round by
   * round (tube or lever action, and a revolver's loading gate) is loaded
   * from a handful of rounds taken behind the back: brought to the
   * loading port, the hand plays the game's loading loop, and each round
   * the loop puts in its place is added by the game's own looping reload
   * with no animation, see VrRoundLoader. The loop's sounds play as the
   * game plays them. A full weapon refuses the round and the loop stops.
   * When a weapon with a rack part (its lever) fires its last round,
   * nothing is chambered: it does not fire until rounds are loaded and its
   * action is worked. Its jams are held as above; one with nothing to rack
   * keeps the game's jam.
   *
   * Phase 5b (findings/reload.md §8.8): a break action is opened and closed
   * with the left trigger near its barrel, see VrBreakHand. Opening it
   * throws its shells out (the clip is emptied, which loses no round, and
   * clears a jam); while it is open it does not fire. Its shells are taken
   * behind the back and pushed into the open breech as a magazine is,
   * where the game's own reload with no animation fills the clip.
   *
   * Phase 5c (findings/reload.md §8.9): a revolver's cylinder is swung out
   * and closed with a flick of the right wrist, see VrFlickHand. Swinging
   * it out throws its cases out (the clip is emptied, which loses no
   * round, and clears a jam); while it is open it does not fire. Its speed
   * loader is taken behind the back and pushed into the open cylinder as
   * a magazine is. A revolver loaded round by round opens its loading gate
   * instead, which empties nothing: its rounds only go in while it is
   * open.
   *
   * Phase 5d (findings/reload.md §8.12): the heavy weapons' ammunition
   * boxes, cells and canisters are magazines. A launcher's magazine is the
   * round it fires: once it is fired the tube is empty, and a new missile or
   * mini-nuke taken behind the back is pushed into it as a magazine is.
   * Nothing is taken out of a launcher, and its jams are the game's.
   * Flamers and incinerators have no part to take out and keep the game's
   * reload (user, 2026-10-22). The lock
   * over the minigun's and the grenade machineguns' box is turned open
   * before the box comes out, and shut once one is in, see VrLockHand:
   * while it is open the weapon does not fire, and while it is shut no
   * box goes out or in.
   *
   * When the hand reaches behind the back with no rounds left, the game's
   * no ammo message of V.A.T.S. is shown on the HUD, at most once while it
   * is shown (findings/reload.md §4.1).
   *
   * The reloads may be started from the game's other threads, so the clip
   * left to watch is shared under a lock, and the weapon reloaded by hand
   * is atomic.
   */
  class VrManualReload {

  public:

    ~VrManualReload();

    /**
     * \brief Replaces the player's reload and watches the jams
     * \returns \c nullptr if the executable is not the supported version
     */
    static std::unique_ptr<VrManualReload> install();

    /**
     * \brief Logs the clip of a recent reload when it changes
     *
     * The game fills the clip through its equipment queue a frame or so
     * after the reload starts. Called once per frame on the game's main
     * thread.
     */
    void update();

    /**
     * \brief Turns the reload with the hands on or off
     *
     * Off, every weapon is reloaded as in the game.
     */
    void setEnabled(bool enabled);

    bool enabled() const {
      return m_enabled;
    }

    /**
     * \brief Follows the hands and reloads the weapon in the right hand
     *
     * Called once per frame on the game's main thread, while the game
     * is not running its update.
     *
     * \returns What the left hand did with the magazine and the rack part
     */
    VrReloadEvents updateHands(const VrManualReloadInput& input);

    /**
     * \brief Where the magazine is drawn, see VrWeaponHand::setMagazine
     */
    VrMagazineState magazineState() const {
      return m_loadsRounds
        ? (m_rounds.state() == VrRoundState::Empty ? VrMagazineState::InWeapon : VrMagazineState::InHand)
        : m_magazine.state();
    }

    /**
     * \brief Where the handful of rounds is, see VrWeaponHand::setRoundLoop
     */
    VrRoundState roundState() const {
      return m_rounds.state();
    }

    /**
     * \brief Time in the loading loop, see VrWeaponHand::setRoundLoop
     */
    float roundTime() const {
      return m_rounds.time();
    }

    /**
     * \brief How far the magazine is out along its well, see VrMagazineHand::seat
     */
    float magazineSeat() const {
      return m_magazine.seat();
    }

    /**
     * \brief Whether the left hand holds the rack part, see VrWeaponHand::setRack
     */
    bool rackHeld() const {
      return m_rack.held();
    }

    /**
     * \brief How far back the left hand holds the rack part, see VrRackHand::pull
     */
    float rackPull() const {
      return m_rack.pull();
    }

    /**
     * \brief Whether the action of the weapon in the hand is open, see
     *    VrWeaponHand::setActionOpen
     */
    bool actionOpen() const {
      return m_action.open() || m_flick.open();
    }

    /**
     * \brief How far the lock over the magazine is turned open, see
     *    VrWeaponHand::setLock
     */
    float lockTurn() const {
      return m_lock.turn();
    }

    /**
     * \brief Whether the left hand holds a magazine or rounds, or
     *    works the rack part or the lock
     */
    bool leftHandBusy() const {
      VrMagazineState magazine = m_magazine.state();

      return magazine == VrMagazineState::InHand || magazine == VrMagazineState::Seating
          || m_rounds.state() != VrRoundState::Empty || m_rack.held() || m_lock.held();
    }

    /**
     * \brief Whether the weapon in the hand has a lock over its magazine
     *    turned by hand, see VrWeaponHand::setLock
     */
    bool lockDriven() const {
      return m_hasLockPart;
    }

    /**
     * \brief How far back the rack part rests, see VrWeaponHand::setRack
     */
    float rackRest() const {
      return m_rackRest;
    }

    /**
     * \brief Whether the left hand holds a magazine or rounds taken behind
     *    the back, which \ref cycleAmmo can change
     */
    bool holdsAmmo() const;

    /**
     * \brief The ammunition the left hand holds, valid with \ref holdsAmmo
     *
     * The clip's type while rounds of it are left, as the hand took it; it
     * goes into the weapon with the magazine or rounds, see
     * findings/quick-menu.md §6, decision 3.
     */
    const uint8_t* heldAmmo() const {
      return m_heldAmmo;
    }

    /**
     * \brief Makes the ammunition in the left hand the next type with rounds left
     *
     * \returns \c false if nothing changed
     */
    bool cycleAmmo();

    /**
     * \brief Where the reload of the weapon in the hand stands, for the
     *    parts that glow, see vrPartsToWork
     */
    VrReloadSituation situation() const;

  private:

    VrManualReload() = default;

    dxvk::mutex     m_mutex;

    // Frames left during which the clip of the last reload is watched, and
    // the clip count logged last
    uint32_t        m_watchFrames = 0;
    int32_t         m_watchedClip = 0;

    bool            m_enabled = true;

    // Weapon reloaded by hand (TESObjectWEAP*), compared but not
    // dereferenced by the reload hook
    std::atomic<const uint8_t*> m_handWeapon = { nullptr };

    // Weapon equipped at the last frame, and the last one a magazine was
    // found on, compared but not dereferenced
    const uint8_t*  m_lastWeapon = nullptr;
    const uint8_t*  m_wellWeapon = nullptr;

    VrMagazineHand  m_magazine;
    VrRoundLoader   m_rounds;
    // The weapon in the hand is loaded by m_rounds
    bool            m_loadsRounds = false;
    VrRackHand      m_rack;
    VrBreakHand     m_action;
    VrFlickHand     m_flick;
    VrLockHand      m_lock;
    // The weapon in the hand has a lock over its magazine
    bool            m_hasLockPart = false;

    // The right hand at the last frame, to tell how fast it turns
    VrGameCameraPose m_lastRight;
    bool            m_hasLastRight = false;

    // A magazine went in with nothing chambered: racking fills the clip.
    // The clip count at the last frame, to tell when the game fills it, and
    // whether the rack part is locked back.
    bool            m_pendingFill = false;
    int32_t         m_lastClip = 0;
    bool            m_lockedBack = false;

    // Frames left until the clip a rack filled is counted: the rack part
    // is not locked back meanwhile
    uint32_t        m_fillFrames = 0;

    // The weapon reloaded by hand is jammed, and it jammed since the last
    // frame; set by the game's attack, which may run on another thread
    std::atomic<bool> m_jammed     = { false };
    std::atomic<bool> m_jamStarted = { false };

    // The jams of the weapon reloaded by hand are held by the VR layer
    std::atomic<bool> m_holdsJam   = { false };

    // A weapon loaded round by round fired its last round and its action
    // was not worked since: it does not fire, as if jammed
    std::atomic<bool> m_unchambered = { false };

    // The action in the hand, or the lock over its magazine, is open: it
    // does not fire
    std::atomic<bool> m_actionOpen  = { false };

    // How far back the rack part rests, in metres
    float           m_rackRest = 0.0f;

    // How the weapon in the hand is reloaded and whether it has a rack
    // part, as seen at the last frame
    VrReloadKind    m_kind = VrReloadKind::None;
    bool            m_hasRackPart = false;

    // Clip count at which the weapon in the hand last refused a round, or
    // -1: it is full while its clip holds that many
    int32_t         m_refusedClip = -1;

    // The game's no ammo message can be shown, and how long the last one
    // shown has left, in seconds
    bool            m_showsNoAmmo = false;
    float           m_noAmmoTime  = 0.0f;
    bool            m_loggedNoAmmoSound = false;

    // Ammunition of the magazine or rounds taken behind the back
    // (TESAmmo*), compared but not dereferenced
    const uint8_t*  m_heldAmmo = nullptr;

    void loadHeldAmmo();

    void clearJam(const uint8_t* weapon, const char* how);

    void showNoAmmo();

    void watchClip(int32_t clip);

    void ejectMagazine(uint8_t* player, const uint8_t* weapon, const VrAnimationSound* sound,
      const VrVector3& well, bool chamber);

    void insertMagazine(uint8_t* player, uint8_t* weapon, const VrAnimationSound* sound,
      const VrVector3& well, bool chambered);

    void rackWeapon(uint8_t* player, uint8_t* weapon, const VrAnimationSound* sound, const VrVector3& rack);

    VrRoundEvent updateRounds(const VrManualReloadInput& input, uint8_t* player, uint8_t* weapon,
      bool active, bool closed);

    bool loadRound(uint8_t* player, uint8_t* weapon);

    void playLoopSounds(const VrRoundLoop& loop, const VrVector3& port) const;

    void cycleAction(uint8_t* player, uint8_t* weapon, const VrAnimationSound* sound, const VrVector3& rack);

    void openAction(uint8_t* player, const uint8_t* weapon, const VrAnimationSound* sound, const VrVector3& breech,
      bool cylinder);

    bool fillClip(uint8_t* player, uint8_t* weapon);

    static uint32_t __fastcall reloadHook(
            uint8_t*              player,
            void*                 unused,
            uint8_t*              weapon,
            uint32_t              animation,
            uint32_t              useMod,
            uint32_t              fromInventory);

    static int32_t __fastcall clipCountHook(
            uint8_t*              entry,
            void*                 unused);

    static uint32_t __fastcall jamGroupHook(
            uint8_t*              actor,
            void*                 unused,
            uint32_t              group,
            void*                 weaponEntry,
            uint32_t              checkHandToHand,
            void*                 animation);

  };

}
