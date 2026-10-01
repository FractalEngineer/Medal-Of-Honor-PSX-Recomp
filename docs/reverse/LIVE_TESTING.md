# Live testing -- reliably reaching gameplay

The single biggest time sink in M1 was testing against the wrong screen. This is
the workflow that actually works.

## Getting into gameplay

Boot headless, then **load save slot 0**:

```
savestate slot=0 op=load
```

- State file: `saves/openbios/state_8001DFD4_slot00.pst` (`memcard_dir = "saves"`
  in `game.toml`; states live in the `openbios` subdirectory).
- **Slots are created by saving.** Slot 0 is gameplay; slots 1 and 2 were added later in the session (see below). Loading a slot that does not exist returns
  ok:true and silently does nothing, leaving the game running from boot.


## Save slots (current)

| slot | scene |
|---|---|
| 0 | night terrain over water (dark, low contrast, huge arm in frame) |
| 1 | **interior room** - walls, two windows, doorway. High contrast, strong depth structure |
| 2 | exterior beside a building |

Slot 1 is the best scene for visual work: a world shift there is unmistakable,
whereas slot 0 is close to the worst case (dark, few depth cues, and the rifle
covers the centre of frame).

Slot 1 does NOT exist until someone saves it. It was created during this
session, which is why an earlier note here said only slot 0 existed.

## Save-state thumbnails (.thumb) - no emulator needed

Each `state_*.pst` has a sibling `.thumb`, and it decodes without booting
anything:

```text
header (12 bytes): 50 53 54 48 | 80 00 00 00 | 60 00 00 00
                   "PSTH"        width 0x80=128  height 0x60=96
payload:           128 * 96 * 4 bytes = 49152  (4 bytes/pixel, RGB in the low 3)
```

So a thumbnail is 128x96 RGBA. Read bytes `[12:]` as `uint8`, reshape to
`(96, 128, 4)`, and use channels `[0:3]`.

```python
a = np.frombuffer(open(p, "rb").read()[12:], np.uint8)[:128*96*4].reshape(96, 128, 4)
Image.fromarray(a[:, :, :3]).resize((384, 288), Image.NEAREST).save(out)
```

Getting this wrong is silent: a wrong shape still produces a plausible-looking
image (an early attempt used `128*128*3` and rendered red/blue stripes). The
`0x80`/`0x60` pair in the header is what pins width and height - and 128*96*4
is the only arrangement that equals the 49152-byte payload exactly.

