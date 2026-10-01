# M1 — Where the view transform lives (findings)

Status: **map complete**; one ambiguity remains (see *Open question*).

## Method

1. Ghidra 12.1.4 headless + JDK 21.
2. **Key technique:** `read_ram addr=0x80000000 len=0x200000` from the running
   game, decode to a raw binary (`hex2bin.py`), and import the whole 2 MB live
   image at `0x80000000` (`MIPS:LE:32:default`, `BinaryLoader`). Ghidra then
   analyzes **all** running code — main EXE *and* every loaded overlay — in one
   pass, with full xrefs and decompilation. No per-overlay captures needed.
3. Corroborated at runtime with `fn_entry_dump` (caller return addresses) and
   live-RAM `JAL` xref scans.

## Render pipeline (addresses)

```
FUN_8006d2e4 / FUN_8006d460          scene/entity render drivers
        |
        v
FUN_800824d0                          per-MODEL render entry
        |  branches on (entity+0x74) and model type
        +--> FUN_80081cdc | FUN_8008080c     (model variants)
        |
        v
FUN_80082948                          RENDER DISPATCHER
        |  vtable (&PTR_DAT_8009d678)[entity+0x54]
        |  if (*(entity+0x6c) == 0x80) -> FUN_80080dd4
        |  else                        -> FUN_800814c4
        v
FUN_800814c4 / FUN_80080dd4            GEOMETRY / PRIMITIVE RENDERERS
        |  push current GTE RT+TR (0x20 bytes) onto the transform stack
        |  (ptr at DAT_8009d320, storage DAT_8009d324)
        v
FUN_80084718(entity, model, x, y)     PER-ENTITY TRANSFORM SETUP
        |  builds a translation-only matrix on the stack:
        |     RT = identity (0x1000 Q12), TR = entity+0x98/0x9a/0x9c << 19
        |  i.e. the entity's WORLD POSITION
        v
FUN_80013ae4(node, matrix, out, ent)  RECURSIVE NODE TRANSFORMER
        |  node = word0 index, words1..3 = position, +0x18/+0x20 = children
        |  loads matrix RT into COP2 control, MVMVA the node position,
        |  builds the child matrix, RTPS-projects vertices
        |  writes screen coords to entity+0x94, packets to (out)
        |  recurses:  FUN_80013ae4(child, param_2)
        |             FUN_80013ae4(child2, local_20)   // composed matrix
        v
projection: FUN_80013698 (RTPS loop; H from 0x80096974, set 400 by default)

display/projection SETUP: FUN_8005f89c (overlay 0x8005F000)
        H = (int)(width * sqrt(3) * 0.5)  -> 443 at width 512
```

## Hard facts established

- **The only COP2 RT/TR loaders in the entire running image are `FUN_80013AE4`
  and `FUN_80013E58`.** Nothing anywhere loads a standalone "view matrix".
- `_DAT_80099428` has exactly **one** reference in the whole image: the READ in
  the projection function. The camera object is `*_DAT_80099428`, keyed by
  `+0x388` and `+0x9c` bit 0x800; it has a child entity at `+0x100`.
- `entity+0x84` holds the **model/node list** passed into `FUN_80084718`.
- The matrix handed to the transformer is built **locally** in `FUN_80084718`
  from the entity's position and is **translation-only (identity rotation)**.

## Conclusion

There is **no separate view matrix to find or override**. The engine does not
compose a view x model product through a matrix load; instead each entity's
geometry is transformed by *its own world position* and projected directly.

The leading explanation consistent with all of the above: **the engine keeps
coordinates camera-relative** — the camera "moves the world" rather than
transforming the world into a separate view space. That is why every search for
a written view matrix came up empty, and why the only matrix loaders are the
node transformer and its helper.

## Open question (one probe left)

Confirm camera-relative storage by checking whether entity `+0x98/0x9a/0x9c`
values track the *player* or produce a moving world. If they are camera-relative,
M1 is closed: `+0x98/0x9a/0x9c` are the per-entity offsets **in camera space**.

## What this means for VR stereo

Not a matrix override. The injection points, in order of preference:

1. **`FUN_80084718`** — the single choke point where every entity's transform is
   built. A per-eye offset here (or on the camera entity's own transform) gives
   stereo for all geometry.
2. **`FUN_800814c4` / `FUN_80080dd4`** (geometry pass) or **`FUN_800824d0`**
   (per-model entry) — run the scene twice with a per-eye camera perturbation.
3. If coordinates are camera-relative, the cleanest lever is the **camera
   entity** itself: perturb its position/rotation per eye and re-run the frame.

## Tooling notes

- `fn_entry_dump` only covers instrumented functions (the transformer is; the
  projection function and `FUN_80084718` are not) — fall back to live-RAM `JAL`
  xref scans for uninstrumented functions.
- When extracting from Ghidra's `application.log`, slice only the lines written
  by the current run; the log retains previous runs and will otherwise
  re-print stale targets.

## Attempt to confirm camera-relative storage (inconclusive - blocked)

Direct tests run against the running game:

1. `_DAT_80099428` read back as **0** at "gameplay", and the entire region
   0x80099400..0x8009947F is **zero** in the saved live-RAM dump. So that global
   is not part of the active path - the projection function FUN_80013698 that
   dereferences it is not the code doing the on-screen projection (the node
   transformer FUN_80013AE4 does its own RTPS). Treat FUN_80013698 as off the
   active path.

2. Movement test: sampled 0x801D3400, 0x801FFC00, 0x80147400 (0x400 bytes each)
   before and after driving inputs - **all byte-identical**.

3. Liveness check (two full 2 MB samples 3 s apart):
   - frames advancing: 12897 -> 12899 -> 12901
   - only **160 differing nibbles (~80 bytes) in the whole 2 MB**
   - `nproj` CONSTANT at 1301, `nsat`/`nflat` constant

Conclusion: the emulator is live, but the **scene is completely static** - the
inputs are not moving the player. So the movement test was vacuous and the
camera-relative hypothesis is **not yet confirmed**.

### RESOLVED — that was a methodology failure, not a save-state problem

Both vacuous runs above (zero delta at 0x80099428; ~80 static bytes at the
intro) had one root cause: **the pad is active-low**. `input 0x0010` presses
every button *except* Up, which includes Start, so the game paused and the world
froze. `input 0000` — written as "release" — pressed the entire pad. The save
state was a separate, smaller mistake (slot 1 does not exist; slot 0 is
gameplay). Corrected convention: `docs/reverse/LIVE_TESTING.md`.

With slot 0 plus active-low input, movement is real and reproducible (136,080
bytes changed, nproj 1187 -> 204). Everything below is the corrected record.

## Camera-relative storage: still open

The RAM-diff approach could not settle this: the addresses probed (entity+0x98
via 0x800EEF3C, and the 0x8009Axxx table) showed no position-like deltas -
0x800EEF3C was byte-identical and the 0x8009Axxx changes look like list/packet
buffers. **The entity table had to come from the live hook instead** (see the
hook section at the end of this file), which is why the diff route was retired.

## RE-ANCHORED against a real gameplay snapshot (result: identical)

A second full live-RAM image was captured in genuine gameplay (slot 0, first-person
level) and imported into a fresh Ghidra project, then the same queries re-run.
Comparison against the intro snapshot:

- FUN_80013AE4        body 80013ae4..80013e57, refs 3 (adds explicit CALLER
                      FUN_80084718)                       -> same
- FUN_80084718        refs 3; CALLERs FUN_800814c4, FUN_80080dd4, FUN_80046dd4 -> same
- FUN_800814c4        refs 1 (from 80082948); CALLER FUN_80082948 -> same
- FUN_80080dd4        refs 2 (80082aa8, 80082d38); CALLERs FUN_80082948,
                      FUN_80082ca8                        -> same
- FUN_80082948        refs 1 (from 80082548); CALLER FUN_800824d0 -> same
- FUN_800824d0        refs 2 (8006d334, 8006d4c0); CALLERs FUN_8006d460,
                      FUN_8006d2e4                        -> same
- DAT_8009d320        60 refs   -> same
- DAT_8009d324        30 refs   -> same
- _DAT_80099428       1 ref     -> same

**Conclusion: the render pipeline is the SAME code in the intro and in gameplay**
(the intro's 3D scenes run through the same engine). Nothing analyzed in the
0x8008xxxx set was intro-specific. The provenance concern is resolved - the map
above stands as-is, and the save-state mixup cost no analysis.

Also confirmed in this snapshot: the transformer's caller chain is intact
(FUN_80013AE4 <- FUN_80084718 <- {FUN_800814c4, FUN_80080dd4, FUN_80046dd4}).

## Camera-relative test: idle vs turn vs walk (best evidence so far)

Full-RAM diffs, three conditions, same session (slot 0 gameplay):

| condition      | changed bytes | clusters | notable regions |
|----------------|---------------|----------|-----------------|
| idle (2.5 s)   | 38,338        | 119      | 0x800A0149 (40,979) |
| turn in place  | 75,639        | 183      | + 0x80155B58 (18,778), 0x801422D8 (18,634) |
| walk forward   | 80,563        | 360      | + 0x80142588 (20,518), 0x80155E08 (20,518), 0x800C4378 (4,623) |

### What this does and does not show

- The engine rebuilds a large (~41 KB) buffer at `0x800A0149` **every frame even
  when idle** - so a large diff is NOT by itself evidence of anything.
- **Turning in place** (pure rotation, zero translation) changes ~37 KB *more*
  than idle, in ~19 KB contiguous blocks at `0x80155B58` / `0x801422D8`.
- **Walking** changes ~42 KB more than idle, in ~20 KB blocks at `0x80142588` /
  `0x80155E08`, plus `0x800C4378` (4.6 KB) which responds to translation only.

Camera rotation alone rewriting ~19 KB contiguous regions is what a
camera-relative engine looks like (coordinates re-derived over large tables).
It is **consistent with** camera-relative storage and hard to explain with a
32-byte view matrix - but it is not proof, because per-frame output buffers also
churn.

### Why this does not block VR

The verdict changes only *how* a per-eye offset is computed (a world-space delta
vs a view-space delta), not *where* it goes. Both cases inject at the same place:
`FUN_80084718` (per-entity transform) or the geometry pass. Determine it
empirically when building the hook - set a per-eye offset and see which sign and
axis produces correct stereo separation.

### What would settle it

Find one static world object whose coordinates are read (not written) per frame,
and watch them across a pure turn. If they change, storage is camera-relative.
The walker's arguments did not expose this: its 4th register is a constant global
(`0x800EEF3C`) and its node pointers are stack addresses (`0x801D3xxx`), so the
per-entity positions are not at a simple static address.

## Entity table via the live hook (breakthrough)

The function-entry hook at `FUN_80084718` is built and working (see
`vr/psx_vr_stereo.c`, package `moh.vr.stereo`). It fires through the dirty-RAM
interpreter, so **no regeneration was needed** - `0x80084718` is overlay code
above the main-EXE text end (`0x8003A000`), and `[recompiler]
mod_function_entry_funcs` never emitted it. This matches the documented
contract: `dirty_ram_interp.c` calls `psx_mod_function_entry` on every
interpreted entry whenever hooks are active, and the hook table is built from
plugin registration, not the generated config list.

### What it revealed

`FUN_80084718` IS a general per-entity transform builder. The entity pointer
arrives in `$a0` and varies. Observed over one session (idle -> walk -> turn),
sampled every 20 calls:

| entity | position samples |
|---|---|
| `800EEF3C` | `(-1,481,-126)` stable, high up (y=481) |
| `800BDAEC` | `(-5,-155,-3)` -> `(-6,-155,-12)` -> `(21,-152,-8)` -> `(32,-125,-5)` -> `(12,-141,6)` - **moves** |
| `800BB2D8` | `(-1,-148,-6)` -> `(0,-147,-9)` -> `(-2,-150,-3)` |
| `800B8AC4` | `(-17,-60,5)` -> `(-17,-59,6)` |

These are exactly the pointers the RAM-diff approach could not surface.

### Correction to the earlier "constant a0" reading

An earlier probe concluded `FUN_80084718` was always called with a fixed global
`0x800EEF3C`. That was an artifact: `0x80084718` is not instrumented by the fn
ring, and the first 200 calls in a fresh boot all belong to the idle/intro
object. Over a longer window four distinct entities appear.

### Why this matters for camera-relative

The positions are **small integers** (tens), not large world coordinates. The
moving object (`800BDAEC`) shifts by single digits per sample. That is at least
consistent with camera-relative storage, and the hook now gives a direct
instrument to settle it: watch a known static world entity across a pure turn.

### Status

- Hook: **working**, activation confirmed, entity table observable.
- Open: identify which entity is the camera; then apply the per-eye offset there.
- Framework gap (unchanged): rendering twice per frame is still needed for real
  stereo. The hook is the injection point, not the whole mechanism.


## Entity struct layout (from the live hook)

Dumped on first sighting of each entity. Confirmed against `FUN_80084718`'s
reads, so the position fields are the ones M1 named:

```text
entity + 0x00   pointer to model/asset data   (800C6988 / 800C5680 per entity)
entity + 0x04   id-like                       (000002A5)
entity + 0x80   pointer                        (801E0EE0)
entity + 0x84   pointer to transform/matrix    (801D3A44 - SHARED by entities)
entity + 0x88   pointer to asset pair          (800C6988 / 800C5680)
entity + 0x8C   pointer                        (800C6A38 / 800C5730)
entity + 0x98   int16 world X   <- read by FUN_80084718 (<<0x13)
entity + 0x9a   int16 world Y
entity + 0x9c   int16 world Z
entity + 0x388  scalar pair                    (000A0000 000A0000)
entity + 0x390  32-bit scalar                  (FFFFEC0E / FFFFECCB - Q16.16-like)
entity + 0x394  32-bit                         (00000164 / 00000092)
entity + 0x398  pointer to related entity      (800BDAEC - self-link observed)
entity + 0x39C  32-bit                         (00003D2A / 00003141)
```

Verification: for `800BDAEC`, `+0x98/9a/9c` decode to `(-5,-155,-3)`, exactly the
position the hook independently read - so the offsets are right and the position
is genuinely `int16 x, y, z`.

`entity + 0x84` is **shared** (`0x801D3A44`) across entities, consistent with
M1's finding that the composed matrix lands in a common staging buffer rather
than living per entity.

### Still to determine

Which of the four entities is the camera. The candidate evidence so far:
`800EEF3C` is the only one with a large Y (`481`) and is the only entity
transformed during long stretches; `800BDAEC` / `800BB2D8` move with the player.
The decisive experiment is the injection itself - offset one candidate and see
whether the rendered view shifts.


## Injection works â€” offset produces a HUD-independent view shift

The hook can now patch an entity's position at the moment `FUN_80084718` reads
it. Applied as **patch-then-restore**: the original value is written back at the
start of that entity's next call before re-patching, so the stored position never
drifts across frames (the function body still reads the patched value).

Interface (environment, all optional; inert without them):

```text
PSX_VR_TARGET=0xADDR   entity to offset (0/unset = every entity)
PSX_VR_AXIS=0|1|2      0=X (default), 1=Y, 2=Z
PSX_VR_OFFSET=N        signed delta on that axis (0 = disabled)
```

### Result (slot 0 gameplay, static scene, no input)

| run | target | offset | screenshot |
|---|---|---|---|
| A | none | 0 | C1EE8F70... baseline |
| B | `800EEF3C` | X +200 | F231E08A... |
| C | all | X +200 | 8605F69A... |

Log confirms the patch fires: `PATCH ent=800EEF3C axis=0 -1 -> 199`.

**Visually:** baseline shows the rifle centred at the bottom with the tree line
and water behind it. With the offset, **the rifle moves to the bottom-right and
the terrain shifts with it, while the compass and the ammo readout (8 / 24) do
not move at all.** The 3D scene and the 2D HUD separate cleanly.

That is precisely the behaviour stereo needs: a view-space displacement that
leaves the HUD alone. It is the first end-to-end proof that the injection point
is real and reaches the rendered image.

### Open questions

- **Parallax vs flat shift.** A screenshot alone cannot distinguish a true
  viewpoint change (near objects move more than far ones) from a uniform 2D
  translation. Measuring that is the next step, and it is what separates real
  stereo from the "weak 3-D-TV" strategy this plan rejects.
- **B and C look the same.** Offsetting *every* entity by the same delta should
  be a no-op relative to a camera that is itself an entity - so either not every
  entity is actually patched, or the camera is not among the patched set. Worth
  resolving before choosing the per-eye target.
- Screenshot hashes are not proof on their own: this scene animates (water), so
  two baselines would also differ. The comparison above is visual, and the
  A/B/C difference is far larger than the animation.

