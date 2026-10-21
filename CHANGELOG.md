# Changelog

## Unreleased (since `fb87675`, 2026-10-16)

### Manual reload

Weapons are now reloaded with the hands instead of the reload button. It is on by default and can be turned off in
**Settings > VR > Manual reload** (or with `d3d9.vrManualReload` in `dxvk.conf`; the in-game choice is saved as
`ManualReload` in `fnvvr/vr_options.ini` and takes precedence).

- **Controls changed.** The **left grip** now holds every weapon with both hands, firearms and melee weapons alike. The
  **left trigger** takes and handles magazines, rounds and moving parts. New ammunition is always taken **behind the
  left shoulder**. Letting go of the left trigger drops what the hand holds without losing any round.
- **X no longer reloads** the weapons that are reloaded by hand, and the automatic reload is off for them: an empty
  weapon clicks. Equipping a weapon still refills it, and ejecting a partly used magazine loses no round.
- **Detachable magazines and energy cells**: pull the magazine out of its well, take a new one behind the shoulder and
  push it in. The magazine slides along its well, the hand holds it as the game's reload animation does, and the
  game's own sounds play when it goes out and in.
- **Chambering**: a weapon with a slide, charging handle or bolt keeps a round chambered. When nothing is chambered,
  the part is drawn locked back and a new magazine does nothing until the part is pulled back with the left hand and
  released.
- **Jams**: a weapon reloaded by hand no longer plays the jam animation. It stays jammed (the trigger clicks) and its
  rack part rests partly back until it is racked; a weapon without a rack part is cleared by taking its magazine out.
  V.A.T.S. keeps the game's own jam.
- **Tube and lever weapons** (shotguns, Cowboy repeater, brush gun, ...): a handful of rounds is taken behind the
  shoulder and held between the thumb and the index finger. Held at the loading port, the rounds go in one by one at
  the rhythm and with the sounds of the game's loading animation, until the weapon is full or the hand moves away. A
  full weapon refuses rounds, and a lever weapon that fired its last round needs its lever worked.
- **Break actions** (single shotgun, sawed-off, Caravan shotgun): the left trigger near the barrel breaks the action
  open, the barrel tips down and the spent shells are thrown out. New shells, held between the fingers, are brought to
  the open breech; the left trigger there again closes it. The weapon cannot fire while it is open.
- **Revolvers**: a flick of the right wrist swings the cylinder out (throwing the cases out) and a flick the other way
  closes it. Swing-out revolvers take a speed loader from behind the shoulder. The .357 Magnum revolver and Lucky open
  their loading gate with the same flick and are loaded round by round, like the tube weapons. A revolver cannot fire
  while it is open.
- **Highlighted parts**: the part that needs working next (the empty or jammed magazine or cell, the rack part, a break
  action's barrel, the loading port while rounds are held) glows the way V.A.T.S. highlights a limb, wherever the
  hand is. Outside V.A.T.S. only; V.A.T.S. keeps its own highlight.
- **Heavy weapons**: the ammunition boxes of the minigun, CZ57 Avenger, K9000 and grenade machineguns, the shoulder
  mounted machine gun's box, the Red Glare's canister and the Tesla cannon's cell are taken out and put in like
  magazines (the Gatling laser and plasma casters already were).
- **Launchers** (missile launcher, Annabelle, Fat Man, Esther): after the shot the tube is empty; a missile or mini-nuke
  taken behind the shoulder is pushed into the tube.
- **Flamers and incinerators**: fuel is taken behind the shoulder and the left hand held half a second where the game's
  reload animation puts it on the weapon.
- **Not yet covered**: weapons whose model has no recognised part keep the game's reload (the X button).

**Bindings**: a new SteamVR action, "Take out, take and insert a magazine (left hand)", is bound to the left trigger,
and the two-handed grip moved from the left trigger to the left grip. Players with custom bindings need to bind the
new action and move the grip.

### Console

- The game's console can be opened from the headset: a **Console** button in the Settings list, after VR, in game only
  (it is left out of the main menu's Settings list).
- The console is shown on the game's own **terminal**: the terminal monitor, its font and its screen effects (blur,
  scan lines, flicker), standing in 3D in front of the player at eye height, in both eyes.
- Its keyboard is the keyboard of the world's terminals: it unfolds below the monitor with the terminal's own
  animation and sounds when the console opens, and folds back when it closes. Keys are typed with the lasers, and the
  keyboard's orange light shows Shift and Caps Lock.
- Typed commands run exactly as if typed on a physical keyboard. The right stick scrolls the output; B, ESC and EXIT
  close the console.
- While the console is shown, the menu panel is hidden, the lasers no longer drive the game's cursor and the gamepad
  is at rest. A flat keyboard panel remains as a fallback.

### Finger tracking

- The fingers of a free hand follow the player's fingers, read from SteamVR's hand skeleton: each finger bends from
  the relaxed open hand to the game's fist. This covers the left hand with one-handed weapons, both hands with no
  weapon and the Pip-Boy forearm; a hand on a weapon keeps its grip, and a hand holding a magazine or rounds keeps the
  reload grip.
- With no weapon, the right hand now sits at its controller; it orbited around it before.
- SteamVR may need to reload the default bindings once for the new hand skeleton actions.

### Camera

- The view is kept in first person: a save made in third person, an Alt+Tab or the POV control no longer leave the
  player in third person (outside V.A.T.S., while alive).

### Fixes

- The Settings list is now sized for all its entries; the sixth entry was cut off.
