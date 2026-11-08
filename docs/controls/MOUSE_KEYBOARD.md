# Mouse and keyboard controls

Enable **Mouse and keyboard** in the Mods menu. The feature is off by default.
Click once in campaign gameplay to capture the mouse; that click does not fire.
Press Escape to release it.

| Control | Gameplay | Menus |
| --- | --- | --- |
| Mouse motion | Look with supported campaign weapons | — |
| W/A/S/D | Move / strafe | Navigate |
| Left mouse | Fire | Confirm |
| Right mouse | Firearm / grenade zoom, or sniper scope | — |
| R / E | Reload / use | E confirms |
| Q | Next weapon | Back |
| Wheel up / down | Previous / next weapon while captured | — |
| Left Ctrl | Crouch | — |
| Space | Jump | — |
| P | Pause | Start / resume |
| Tab | Objectives / select | — |
| Arrow keys | — | Navigate |
| Enter / Backspace | Fire / jump alternatives | Confirm / back |

Gameplay and menu actions are editable in the mod options, with up to three
alternatives per action. Weapon cycling also accepts wheel bindings. Global and
aiming sensitivity and aimed zoom are configurable. Grenades use the same
zoom and reticle while retaining their native throwing physics; the sniper uses
its native scope zoom.
Ordinary saved keyboard and controller profiles are not changed.

This feature targets single-player campaign play. The user confirmed the
reported aiming, sniper, weapon-switching and throwable issues were fixed,
including right-click grenade aiming. Front-end menu bindings have automated
coverage but still need gameplay checks. See the
[handoff](MOUSE_KEYBOARD_HANDOFF.md) for validation details and remaining limits.

For a clear flat-play picture, enable **Image Filtering**, set **Smoothing** to
**Sharp**, **Texture filtering** to **Nearest**, and **Dithering** to **Off**;
keep **CRT Filter** disabled. These settings avoid the framework's default
smooth scaling and native-grid dithering.
