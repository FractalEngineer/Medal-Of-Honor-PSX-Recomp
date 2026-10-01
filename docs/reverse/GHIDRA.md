# Ghidra (headless) — setup, use, findings

Ghidra is installed **locally** (outside the repo), so this is reproducible:

- JDK 21 — `C:\Users\titan\tools\jdk-21.0.12.1+1` (Ghidra 12.1.4 requires
  `application.java.min=21`)
- Ghidra 12.1.4 PUBLIC — `C:\Users\titan\tools\ghidra_12.1.4_PUBLIC`

## Re-run

1. Strip the PS-X EXE header (payload starts at file offset `0x800`):
   `python extract_payload.py disc\SLUS_009.74 payload.bin`
2. `set JAVA_HOME=C:\Users\titan\tools\jdk-21.0.12.1+1`
3. ```text
   analyzeHeadless <projdir> MOH -import payload.bin `
     -processor MIPS:LE:32:default `
     -loader BinaryLoader -loader-baseAddr 0x80010000 `
     -scriptPath <scrdir> -postScript DecompFunc.java 0x80013698
   ```

## Gotchas (learned the hard way)

- `MIPS:LE:32:R3000` is **not** a valid Ghidra language id → use
  `MIPS:LE:32:default`.
- `analyzeHeadless.bat` ends with `pause` **on error**, which hangs a
  non-interactive run (looks like a hang). Success does not pause.
- Script `println` output goes to
  `%APPDATA%\ghidra\ghidra_12.1.4_PUBLIC\application.log`, not stdout.

## Findings — `FUN_80013698` (world projection)

Body `80013698..80013ae3`. Ghidra reports **no callers and no callees** — which
agrees with our `J`/`JAL` scan — so it is entered by **fall-through or an indirect
jump**, and it only does GTE ops + memory stores.

Decompilation saved as `ghidra_FUN_80013698.c`. Highlights:

- `setCopControlWord(2,0xd000,400)` → **CTC2 H = 400** (hardcoded world
  projection distance). Ghidra encodes COP2 control registers at `0xd000+`.
- Per-vertex loop: `setCopReg(2,0,…)` → VXY0, `setCopReg(2,0x800,…)` → VZ0, then
  `copFunction(2,0x180001)` = **RTPS** (cmd 0x01), then `getCopReg(2,0x7000)`
  reads the result; output is written to `DAT_800a94e4` (a packet buffer).
- Ends with `setCopControlWord(2,0xd000,_DAT_80096974)` → CTC2 H from **RAM
  `0x80096974`** (the same address we poked via `write_ram` earlier).
- Per-object data comes through a **global pointer chain**:
  `*(int *)(*(int *)(_DAT_80099428 + 0x388) + 200)` — the resulting struct
  (`+0x6c/0x70/0x74`, `+0x88..0x90`, `+0xc0..`) is where the transform/camera data
  lives. **This is the next thing to map.**

## Caveat — overlays

Ghidra analyzes the **static EXE only**. MOH's render path also runs **overlay**
code loaded into RAM at runtime (e.g. `SetGeomScreen`'s caller at `0x8005FAF0`),
so overlay dumps must be imported separately for full coverage.

## Reference analysis (Ghidra `-process`)

| target | references found |
|---|---|
| `0x80013698` (world projection) | **0** — no code and no data reference anywhere → entered by **fall-through or a computed/indirect jump** |
| `_DAT_80099428` (object → camera chain) | **1** — `READ` from **`0x800136A0`**, *inside* `FUN_80013698`. Nothing in the static image writes it |
| `0x80096974` (H RAM variable) | **1** — `READ` from **`0x80013A7C`**, *inside* `FUN_80013698` |

So `FUN_80013698` is a **self-contained projection routine**, and the object it reads
through `_DAT_80099428` is **not written by any static (EXE) code**.

## Consequence — the camera data is overlay-managed

Since nothing in the main EXE writes `_DAT_80099428`, that object (and the camera /
view chain behind it) is populated by **overlay** code loaded at runtime. This is
consistent with the earlier discovery (`SetGeomScreen`'s only caller is at
`0x8005FAF0`, above the EXE text end).

**Therefore mapping the camera requires importing the overlay dumps into Ghidra**,
not only the main EXE. The runtime already produces overlay captures
(`overlay_captures.json` next to the game), which is the source for those dumps.

## Overlay import (works)

`decode_overlays.py` turns `overlay_captures.json` entries into raw binaries
(`bytes_b64` decoded, written at `load_addr`). Importing one:

```text
ov_80037000_0.bin  4100 bytes @ 0x80037000
Ghidra functions:
  FUN_80037060 (616)  FUN_80037318 (84)   FUN_80037370 (156)  FUN_8003740c (224)
  FUN_800374ec (1312) FUN_80037a0c (424)  FUN_80037bb4 (188)  FUN_80037c70 (284)
  references to 0x80013698: none
```

So Ghidra can analyze the **overlay code** too — the missing half of the picture.

**Only one overlay was captured** (4 KB) because our headless runs were short
(mostly title). `overlay_captures.json` grows as more of the game is played.

## Next

Play **deeper into gameplay** (to capture the render/camera overlays), decode them,
import into Ghidra, and decompile their functions to find the view matrix.
Mapping the camera needs those overlays — the static EXE alone does not contain
the code that writes `_DAT_80099428`.

## Overlay corpus — the missing half, found

`overlay_captures.json.d/` holds **103 per-overlay captures**; decoding yields
**58 unique overlays spanning `0x80030000`–`0x80079000`** (4–8 KB each). The main
`overlay_captures.json` is only the *latest* snapshot — the `.d` directory is the
additive history.

Decode: `decode_all_overlays.py <overlay_captures.json.d> <outdir>` → `ov_<addr>.bin`,
then import each at its `load_addr` (same Ghidra command as before).

## Finding — `FUN_8005f89c` = display / projection setup (overlay `0x8005F000`)

Saved as `ghidra_FUN_8005f89c.c`. By mode `param_1` (0..3) it picks a display size
(width `0x200`/`0x140`/`0x100`, height `0xF0`/`0x100`) and calls:

- `func_0x8001baa0()` → **`InitGeom`**
- `func_0x8001ba78(w>>1, h>>1)` → **`SetGeomOffset`**
- `func_0x8001ba68(H)` → **`SetGeomScreen`**

and **H is computed**, not a constant:

```text
H = (int)( width * 1.7320508f * 0.5f )      // 0x3fddb3d7 = sqrt(3), 0x3f000000 = 0.5
   via func_0x8001d590 / 0x8001d450 / 0x8001e11c
```

For `width = 512` → **H = 443**, exactly the value the GTE ring reported at the
title — independent confirmation that this is the live projection setup.

H is cached in `_DAT_8007bf78`; the mode index in `_DAT_8007bf50`. Caller:
`FUN_8005fe04`. This was invisible to static analysis because it is **overlay code**.

## GTE transform library is in the MAIN EXE (from `Refs.java` over all overlays)

`Refs.java` run across all 58 overlays **and** the main EXE: every `CTC2` matrix
load is in the **main EXE**; none in the overlays. Structure:

| func | body | role |
|---|---|---|
| `FUN_80013698` | 80013698..80013ae3 | projection (RTPS loop); writes `CTC2 H` (hardcoded 400, then from `0x80096974`) |
| `FUN_80013AE4` | 80013ae4..80013e57 | **recursive transform routine** |
| `FUN_80013E58` | — | partial matrix load; called from `0x80013B3C` (inside the transform routine) |

`FUN_80013AE4(ushort* vertex, undefined4* matrix, …)` loads `matrix[0..4]` (5 packed
words = the 3×3 `RT`) plus translation into the GTE control registers and runs
`MVMVA` (`0x41e012` / `0x49e012`). Its only references are **its own recursive
calls** (`JAL` at `0x80013DD0` / `0x80013DE8`) → it is a **scene-graph traversal
that accumulates transforms** down the node hierarchy.

Neither it nor `FUN_80013698` has an external caller → both are entered
**indirectly (computed/fall-through)**.

## Conclusion

- **Rendering pipeline (main EXE):** scene-graph transform walk (`FUN_80013AE4`) →
  vertex projection (`FUN_80013698`) → GPU packet build.
- **Overlay code** holds the display/projection *setup* (`FUN_8005f89c`, computes
  `H = width·√3/2`) and game logic.
- The camera/**view matrix enters as the `matrix` argument** to the recursive
  transform routine; its top-level caller is indirect, so the next step is to
  catch it at runtime (a `wtrace`/`fntrace` on `0x80013AE4`, or watch the matrix it
  loads).

## Found the transform walker's top-level caller (live RAM)

`fn_entry_dump addr_lo=0x80013AE4` returns 64 entries; **exactly one has a
non-recursive `ra`**: `ra = 0x800847BC` (all others are `0x80013DF0` /
`0x80013DD8`, i.e. the walker's own recursive calls). So the caller is
**`FUN_80084718`** (`ADDIU $sp,$sp,-56`), entry `0x80084718`, JAL at `0x800847B4`,
and it passes `a1 = $sp+16` (the matrix) and `a3 = $t2` (the object).

`FUN_80084718` **builds a matrix on the stack** from the object's fields
(`+152`, `+2`, `+4`, each `<<19`) and calls the walker. Per-object transform setup.

### Capability: code above the captured overlay range is readable from live RAM

`0x800847B4` is above our captured overlays (which top out at `0x80079000`), yet
`disasm addr=0x80084718` and `read_ram` render it fine **while the game runs** — no
overlay capture needed. This removes the earlier capture bottleneck entirely.

## Next

Walk up from `FUN_80084718` (its caller) toward the scene root — the view matrix is
the transform accumulated at the root of the walk. Use `fn_entry_dump` filtered to
`0x80084718`, or read live RAM around that function's callers.

## Full live-RAM image in Ghidra — the decisive technique

`read_ram addr=0x80000000 len=0x200000` -> `hex2bin.py` -> import the WHOLE 2 MB
running image at 0x80000000 (MIPS:LE:32:default, BinaryLoader). Ghidra then
analyzes ALL code - main EXE AND every loaded overlay - in one pass, with full
xrefs and decompilation. No per-overlay captures needed.

## FUN_80084718 decompiled - per-entity transform setup

Saved as ghidra_FUN_80084718.c. It builds a translation-only matrix (identity
rotation, Q12) from the entity's world position at entity+0x98/0x9a/0x9c
(each <<19) and calls the scene-graph walker FUN_80013AE4. So FUN_80084718 is
per-entity world placement.

Callers (live-RAM xref + Ghidra): FUN_800814c4, FUN_80046dd4, FUN_80080dd4.

The view matrix is at the ROOT of the walk - one step further up this chain, and
the full-RAM Ghidra project makes each hop a decompile away.

## The camera is an entity in the same scene-graph walk (full live image)

Refs across the WHOLE live image (main EXE + all overlays):

- `_DAT_80099428` still has exactly ONE reference: the READ at 0x800136a0 in the
  projection fn FUN_80013698. So the camera pointer is stored through a COMPUTED
  address. The camera object is `*_DAT_80099428`, keyed by `+0x388` and
  `+0x9c` bit 0x800; it has a child entity at `+0x100`.
- `DAT_8009d320` = the GTE TRANSFORM STACK POINTER (60 refs: matched read/write
  pairs). The stack storage is DAT_8009d324. Push/pop sites in fns at
  0x8007e1xx, 0x80047cxx, 0x800481xx, 0x8008dbxx, 0x8008e0xx, 0x80042axx.
- FUN_800814c4 and FUN_80080dd4 are the GEOMETRY / PRIMITIVE RENDERERS: they
  `getCopControlWord(2,0..0x3800)` (RT+TR, 0x20 bytes) to push the current matrix
  onto DAT_8009d320, call FUN_80084718 (entity transform), pop, then RTPT the
  vertices straight into packets. FUN_800814c4 references
  `data_msn1_lvl1_tsp0_1_1_c` (mission 1 level 1).
- FUN_80046dd4 operates on the CAMERA object (`param_1+0x388`) and transforms its
  child entity (`param_1+0x100`).

## Conclusion for VR

There is NO separate static view matrix: the "view" is simply the transform at the
ROOT of the same scene-graph walk every object goes through. The camera is an
entity whose transform is pushed first and accumulated downward.

=> The VR lever is the camera ENTITY: its position (entity+0x98/0x9a/0x9c) and
rotation. Stereo can be injected either by perturbing the camera entity transform,
or by running the geometry pass (FUN_800814c4 / FUN_80080dd4) twice with a per-eye
transform at the root of the walk.

## Render dispatcher and the matrix pointer (full live image)

- FUN_80082948 = RENDER DISPATCHER. Looks up a vtable at
  `(&PTR_DAT_8009d678)[entity+0x54]`, then:
  `if (*(entity+0x6c) == 0x80) FUN_80080dd4(entity, list, idx, flag);
   else FUN_800814c4(entity, list, idx, flag);`
  So `entity+0x6c == 0x80` selects the render path.
- Call chain: FUN_800824d0 -> FUN_80082948 -> {FUN_800814c4 | FUN_80080dd4}.
- FUN_80082ca8 = texture/CLUT row blit (calls FUN_80013f98).
- `entity+0x84` holds a **pointer to the matrix** that is passed straight into the
  walker via FUN_80084718. That matrix is the world->screen candidate
  (view x model, or model in a pre-set view frame).

## Key consequence

The ONLY COP2 control-register (RT/TR) loaders in the whole image are
FUN_80013AE4 and FUN_80013E58. So nothing "sets a view matrix" directly: the
transform fed to the walker is COMPOSED upstream and stashed per-entity at
`entity+0x84`.

## Next step

Find the matrix multiply that builds the `entity+0x84` matrix each frame - that is
where the view (camera) transform enters. Then the stereo hook is: add
+/-IPD/2 to the camera-right component of that matrix (or render the geometry pass
twice with a per-eye matrix).
