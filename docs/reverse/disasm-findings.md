# M1 — disasm findings (routine `0x80011214`)

Read with the new `disasm` command. See `tooling.md` and `camera-map.md`.

## Call graph

- The dominant-cluster routine is **`0x80011214`**, called by
  **`JAL 0x80011214` at `0x80010D5C`**; it returns to **`0x80010D64`** — exactly
  the ring's `caller_ra`. So the RTPT is issued inside this routine (or a callee).
- The caller (`0x80010D00…0x80010D9C`) is a **per-element loop**:
  `s3` = scratchpad base, `s7` = element pointer (`ADDIU $s7, $v0, 16` /
  `ADDU $s7, $v0, $zero`), re-entering at `0x80010D38`.

## What `0x80011214` does

- Feeds the GTE from memory (`MTC2` loads, sources `LW …($t3)` where
  `$t3 = element + $s1`), runs a GTE command at `0x80011258` (cmd 6 = NCLIP),
  then **reads GTE outputs back with `MFC2` and stores them to scratchpad**:
  `SH $t3, 26($s3)` / `SW $t0, 44($s3)` / `SH $t4, 34($s3)` / `SW $t1, 48($s3)` /
  `SH $t5, 42($s3)` / `SW $t2, 52($s3)` → offsets `0x1A, 0x22, 0x2A, 0x2C, 0x30, 0x34`.
- It also stages element fields at `0x1F8003C0/3C2/3C4` (`SH`/`LHU` of `s7[0]`,
  `s7[4]`).

### Correction to the earlier reading

The scratchpad slot at `0x1F80001A+` is a **projected-value staging cache (GTE
*outputs*)**, not a transform matrix. The packed pairs are the routine's own
stored results, not a `MATRIX` struct.

### Where the matrices actually come from

The GTE matrices are **loaded via `MTC2` from memory pointers** — base pointer
`s1 = 0x8013650C` plus per-element offsets (`ADDU $t3, $t3, $s1`). That pointer
table (read earlier as fixed-point pairs) is the next thing to follow: it is
where the game's transform/view data lives.

## Tooling limitation found (next tooling task)

The `disasm` shim mis-decodes **COP2** instructions:

- `MFC2/MTC2/CFC2/CTC2` print bogus register operands (the real operand is the
  **cop2 data/control register number** in the low bits).
- GTE commands render as `GTE …, N` instead of names (`RTPS`, `RTPT`, `NCLIP`, …).

ALU/load/store/branch decode correctly (verified). Fixing COP2 formatting is the
next step to make these routines fully readable.
