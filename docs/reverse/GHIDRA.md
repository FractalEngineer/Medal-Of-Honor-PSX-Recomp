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
