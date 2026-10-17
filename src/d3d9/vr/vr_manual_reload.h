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
    /// The left grip is held
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
    /// Where the rack part's centre is home, in the right hand's frame
    /// (VrWeaponHand::rackPart), or \c nullptr without one
    const VrVector3*        rack = nullptr;
    /// Direction it is pulled back along, in the right hand's frame, and
    /// how far in metres, valid with the rack part
    VrVector3               rackAxis;
    float                   rackTravel = 0.0f;
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
      return m_magazine.state();
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
     * \brief Whether nothing is chambered in a weapon with a rack part
     */
    bool rackLockedBack() const {
      return m_lockedBack;
    }

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
    VrRackHand      m_rack;

    // A magazine went in with nothing chambered: racking fills the clip.
    // The clip count at the last frame, to tell when the game fills it, and
    // whether the rack part is locked back.
    bool            m_pendingFill = false;
    int32_t         m_lastClip = 0;
    bool            m_lockedBack = false;

    // Frames left until the clip a rack filled is counted: the rack part
    // is not locked back meanwhile
    uint32_t        m_fillFrames = 0;

    void watchClip(int32_t clip);

    void ejectMagazine(uint8_t* player, const uint8_t* weapon, const VrReloadSound* sound,
      const VrVector3& well, bool chamber);

    void insertMagazine(uint8_t* player, uint8_t* weapon, const VrReloadSound* sound,
      const VrVector3& well, bool chambered);

    void rackWeapon(uint8_t* player, uint8_t* weapon, const VrReloadSound* sound, const VrVector3& rack);

    bool fillClip(uint8_t* player, uint8_t* weapon);

    static uint32_t __fastcall reloadHook(
            uint8_t*              player,
            void*                 unused,
            uint8_t*              weapon,
            uint32_t              animation,
            uint32_t              useMod,
            uint32_t              fromInventory);

    static uint32_t __fastcall jamGroupHook(
            uint8_t*              actor,
            void*                 unused,
            uint32_t              group,
            void*                 weaponEntry,
            uint32_t              checkHandToHand,
            void*                 animation);

  };

}
