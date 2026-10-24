#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "vr_weapon_aim.h"

namespace dxvk {

  /**
   * \brief Rotation of a bone in its parent, by the bone's name
   */
  struct VrBonePose {
    std::string    name;
    VrGameRotation rotate;
  };

  /**
   * \brief A node's transform in an ancestor's frame
   */
  struct VrSequencePose {
    VrGameRotation rotate;
    VrVector3      translate;
    float          scale = 1.0f;
  };

  /**
   * \brief A child's transform followed by its parent's
   */
  VrSequencePose vrComposeSequencePose(const VrSequencePose& parent, const VrSequencePose& child);

  /**
   * \brief The transform tracks of an animation sequence, by the name of
   *    the object they move
   *
   * Reads a BSAnimGroupSequence of the game without playing it: a node
   * is posed as the sequence would pose it at a time.
   */
  class VrSequenceTracks {

  public:

    explicit VrSequenceTracks(const uint8_t* sequence);

    /**
     * \brief A node's transform in its parent at a time
     *
     * Where the sequence has no track for it, or its track has no
     * value, the node's current transform is used.
     *
     * \param [out] rotated Whether the sequence gave the rotation
     */
    VrSequencePose local(uint8_t* node, float time, bool* rotated = nullptr) const;

    /**
     * \brief A node's transform in one of its ancestors' frame at a time
     *
     * \returns \c false if \p ancestor is not an ancestor of \p node
     */
    bool toAncestor(uint8_t* node, const uint8_t* ancestor, float time, VrSequencePose& pose) const;

    /**
     * \brief Whether the sequence moves a node
     */
    bool moves(const uint8_t* node) const;

  private:

    struct Track {
      const char* name;
      uint8_t*    interpolator;
    };

    std::vector<Track> m_tracks;

    uint8_t* find(const char* name) const;

  };

  /**
   * \brief The inventory entry of an actor's equipped weapon
   *
   * \returns The entry, or \c nullptr without a weapon or a process
   */
  uint8_t* vrActorWeaponEntry(uint8_t* actor);

  /**
   * \brief The first-person sequence the game would play for the player
   *
   * \param [in] player The player character
   * \param [in] group The animation group before the game turns it into
   *    the equipped weapon's (Actor::GetAnimGroup), such as the weapon's
   *    reload or attack animation
   * \returns The sequence, or \c nullptr if the player's first-person
   *    animations have none for it
   */
  const uint8_t* vrFindPlayerSequence(uint8_t* player, uint32_t group);

  /**
   * \brief The rotations a sequence gives the bones below a hand at a time
   *
   * \param [in] hand The hand bone
   * \returns The bones the sequence rotates, by name
   */
  std::vector<VrBonePose> vrReadSequenceFingers(const VrSequenceTracks& tracks, uint8_t* hand, float time);

}
