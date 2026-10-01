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
