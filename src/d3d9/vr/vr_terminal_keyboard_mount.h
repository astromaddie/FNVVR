#pragma once

#include "vr_menu_pointer.h"
#include "vr_terminal_keyboard.h"
#include "vr_weapon_aim.h"

namespace dxvk {

  /**
   * \brief Transform of a node of the game's scene graph, scale included
   *
   * A point p of the node's frame is at scale * rotate * p + translate in
   * its parent's frame, as the game's NiTransform places it.
   */
  struct VrGameScaledTransform {
    VrGameRotation rotate;
    VrVector3      translate;
    float          scale = 1.0f;
  };

  VrVector3 vrApplyTransform(const VrGameScaledTransform& transform, const VrVector3& point);

  /**
   * \brief Composes two transforms: \c child first, then \c parent
   */
  VrGameScaledTransform vrComposeTransforms(const VrGameScaledTransform& parent, const VrGameScaledTransform& child);

  VrGameScaledTransform vrInverseTransform(const VrGameScaledTransform& transform);

  /**
   * \brief How the world terminal's keyboard is fitted to the console's monitor
   *
   * The console is shown on the monitor of the game's rendered terminals
   * (Terminals\\TerminalInterface01.NIF), which has no keyboard. The
   * keyboard and its bracket come from the terminals of the world
   * (meshes\\terminals\\terminal01.nif), where they hang from that model's
   * own monitor, the node T01Screen01. Its monitor is hidden, and the model
   * is placed so that the glass of its monitor lies on the glass of the
   * console's monitor: same centre, same width, facing the same way. The
   * keyboard then unfolds below the console's screen as it does below the
   * world terminal's. Measured on both models, see findings/console.md §11.
   */
  struct VrTerminalKeyboardMount {
    /// Centre of the front of the world terminal's glass, in the frame of
    /// its monitor (T01Screen01), which faces +X with +Z up
    constexpr static VrVector3 WorldGlassCentre   = { 24.45676f, -1.68813f, 12.68093f };
    constexpr static float     WorldGlassWidth    = 30.77638f;
    /// Centre of the front of the console monitor's glass, in its model's
    /// frame, which faces -Y with +Z up
    constexpr static VrVector3 ConsoleGlassCentre = { -0.00081f, 0.15883f, -0.10028f };
    constexpr static float     ConsoleGlassWidth  = 6.19688f;

    /**
     * \brief Places the world terminal's model under the console's monitor
     *
     * \param [in] screenLocal Transform of the world terminal's monitor
     *    node (T01Screen01) relative to the model's root
     * \returns Transform of the world terminal's root relative to the
     *    console monitor's model
     */
    static VrGameScaledTransform computeRoot(const VrGameScaledTransform& screenLocal);
  };

  /**
   * \brief The keyboard plate on the world terminal's keyboard
   *
   * The key tops of the keyboard's mesh (T01Keyboard01:0) are one flat
   * quad whose texture coordinates are the keyboard rectangle of the
   * terminal texture, the rectangle the keys of VrTerminalKeyboard were
   * measured on. The plane of that quad is given in the keyboard node's
   * frame by a texel and the steps of one texel along the texture's axes,
   * measured on its corner vertices (findings/console.md §11).
   */
  struct VrTerminalKeyboardPlate {
    /// Texel of the key tops' top left corner, and where it is
    constexpr static float     OriginU     = 46.0643f;
    constexpr static float     OriginV     = 294.9954f;
    constexpr static VrVector3 Origin      = { -2.86965f, -16.88709f, 4.16388f };
    /// One texel to the right and one texel down on the texture
    constexpr static VrVector3 TexelRight  = { 0.0f, 0.166829f, 0.0f };
    constexpr static VrVector3 TexelDown   = { 0.166722f, 0.0f, -0.021101f };

    /**
     * \brief Where a texel of the terminal texture is on the key tops' plane
     *
     * \param [in] u Horizontal texel coordinate
     * \param [in] v Vertical texel coordinate
     * \returns The point in the keyboard node's frame
     */
    static VrVector3 point(float u, float v);

    /**
     * \brief Where the keyboard rectangle is in the room, for the lasers
     *
     * \param [in] keyboardWorld Transform of the keyboard node in the
     *    menu's space, see vrComputeMenuSceneCameraPose
     * \param [in] anchor Where the menu's target is shown in the room
     * \param [in] target Point of the menu's space shown at the anchor
     * \param [in] unitsPerMetre Menu units per metre of the room
     * \returns VrTerminalKeyboard::Plate as a panel: (0, 0) is its top
     *    left texel and (1, 1) its bottom right one
     */
    static VrPanelPlacement placement(
      const VrGameScaledTransform& keyboardWorld,
      const VrPose&                anchor,
      const VrVector3&             target,
            float                  unitsPerMetre);
  };

}
