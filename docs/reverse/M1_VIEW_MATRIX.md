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

### What is needed to close it

- The correct pad mapping for in-game movement (or a save state that is in
  actual, controllable gameplay), then repeat the walk-forward test above: if the
  node data at 0x801D3xxx changes while the world should be stationary, storage
  is camera-relative.
- Alternatively: observe the values passed to FUN_80084718 for `entity+0x98/0x9a/0x9c`
  across frames while the camera turns.

### Status of the hypothesis

Supported by the decompilation (no view matrix exists anywhere; every entity
matrix is translation-only; RT/TR are loaded only by the node transformer), but
**unconfirmed empirically**.

## Provenance warning: which findings came from an intro snapshot

The full live-RAM image (`ram_all.json`) used for the `0x8008xxxx` analysis was
captured while the game was on the **intro** (DreamWorks logo), not in gameplay.
Save slot confusion and an active-low pad meant gameplay was never actually
reached during that phase.

- **Save-state independent (still valid):** everything read from static bytes -
  the main EXE analysis (FUN_80013698, FUN_80013AE4, FUN_80013E58), the CTC2
  enumeration, the 58-overlay decode, the display/projection setup FUN_8005f89c.
- **Needs re-anchoring against a gameplay snapshot (verify):** FUN_800814c4,
  FUN_80080dd4, FUN_80082948, FUN_800824d0, and the `fn_entry_dump` caller
  results - these are resident-overlay code and may be intro-resident rather than
  the gameplay render path.

Gameplay RAM snapshots are now capturable (`docs/reverse/LIVE_TESTING.md`); re-import
one at 0x80000000 and re-run the ref/decompile queries to re-anchor.

## Camera-relative storage: still open

The walk test now genuinely moves the player (136,080 bytes changed, nproj
1187 -> 204), but the specific addresses probed for positions (entity+0x98 via
0x800EEF3C, and the 0x8009Axxx table) showed no position-like deltas - 0x800EEF3C
was byte-identical, and the 0x8009Axxx changes look like list/packet buffers.
Identify the real entity table before repeating the test.

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

