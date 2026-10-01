# Live testing — reliably reaching gameplay

The single biggest time sink in M1 was testing against the wrong screen. This is
the workflow that actually works.

## Getting into gameplay

Boot headless, then **load save slot 0**:

```
savestate slot=0 op=load
```

- State file: `saves/openbios/state_8001DFD4_slot00.pst` (`memcard_dir = "saves"`
  in `game.toml`; states live in the `openbios` subdirectory).
- **Slot 0, not slot 1.** Loading a slot that does not exist returns
  `{"ok": true}` and silently does nothing — the game just keeps running from
  boot.
- After loading, the display is **512x240** in this title, and the frame shows the
  first-person rifle view, ammo HUD (8 / 24) and the compass.

### Boot order trap

Without loading a state, the game sits in the **intro** (DreamWorks logo, then
the publisher reels). Those screens are 3D-ish enough that an `nproj > 1000`
heuristic reads as "gameplay" — it is not. Every movement/telemetry test run
against the intro is vacuous: the scene is static, no pad bit changes anything,
and only ~80 bytes of the 2 MB change over several seconds.

## Seeing the screen

```
screenshot_file <path>     # works headless: native VRAM frame, 512x240 PNG
present_shot <path>        # FAILS headless: "no present readback"
```

Use `screenshot_file`. `present_shot` requires a real display backend.

## Driving input -- THE PAD IS ACTIVE-LOW

This is the single biggest trap. `pad_buttons` idles at **`0xFFFF`**, and a
**0 bit means PRESSED**. To press exactly one button, CLEAR its bit; to release
everything, write `0xFFFF`.

```
input 0xFFFF               # idle: all buttons RELEASED   <-- use this to release
input 0xFFEF               # Up     only  (0xFFFF & ~0x0010)
input 0xFFF7               # Start  only  (0xFFFF & ~0x0008)
input 0xBFFF               # Cross  only  (0xFFFF & ~0x4000)
clear_input
press <buttons_hex> [frames]
```

Getting this wrong is actively harmful: `input 0x0010` does NOT press Up -- it
presses every button *except* Up, which includes Start, so the game **pauses**.
An earlier session of tests was invalidated by exactly this: `input 0000` at the
end of a test (meant as "release") actually pressed every button.

Verify what the pad sees by reading it back:

```
history                    # -> newest index N
get_frame N                # -> "pad_buttons": "0xFFEF"  (echoes the override)
```

Pad bits: `0x0001` Select, `0x0008` Start, `0x0010` Up, `0x0020` Right,
`0x0040` Down, `0x0080` Left, `0x0100` L2, `0x0200` R2, `0x0400` L1, `0x0800` R1,
`0x1000` Triangle, `0x2000` Circle, `0x4000` Cross, `0x8000` Square.
**Start (`0x0008`) opens the pause menu.**

### Verified working

Holding Up-only (`0xFFEF`) for 6 s in gameplay:

- `pad_buttons` read back `0xFFEF`
- **136,080 bytes changed** across RAM (vs ~623 idle, ~29k for the pause menu)
- `nproj` went **1187 -> 204** (a different part of the level)
- the background behind the pause overlay was a different location

So movement works. Watch out: the pause menu freezes the world, so RAM diffs
taken while paused are meaningless.

## Limits hit during M1

- `_DAT_80099428` is **0** in gameplay and the whole `0x80099400..7F` region is
  zero; `FUN_80013698` (which dereferences it) is therefore off the active path.
- `ts <start> <end>` caps at 200 frames per request.
- Ghidra's `application.log` retains prior runs — slice only the lines written by
  the current invocation, or old targets get re-printed.
