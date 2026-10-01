# M1 — disasm findings (routine `0x80011214`)

Read with the `disasm` command (`tooling.md`). Supersedes earlier guesses.

## Call graph

- Routine **`0x80011214`** is called by `JAL 0x80011214` at **`0x80010D5C`**,
  returning to **`0x80010D64`** — the ring's `caller_ra`.
- Caller (`0x80010C00…0x80010D9C`) is a **primitive-submission loop**.

## COP2 / GTE naming (done — fork `c2ed6e55`)

Verified output: `MFC2 $t9, $LZCR`, `MTC2 $t3, $VXY0`, `MFC2 $t3, $SZ1`,
`MFC2 $t0, $SXY0`, `RTPT 0x0280030`, `NCLIP 0x1400006`, plus `CTC2 $t, $OFX/H/TRX…`
control names.

## The loop is a primitive submitter

- **`s1` is a vertex buffer** of 8-byte records `{ VXY (u32), VZ (low 16) }`.
  Indexed as `SLL $t0,$s4,3 ; ADDU $t0,$t0,$s1 ; LW $t0,0($t0) ; LW $t3,4($t0)`.
  (Matches the s1=0x8013650C reads: pairs of 8-byte records.)
- It reads an element (`LW $v1,0($v0)`, `LW $v0,24($v1)`, `LW $fp,12($v1)` = count),
  pulls **three vertex indices** (`$s4`,`$s5`,`$s6` from `$v0`), loads
  `VXY0/VZ0/VXY1/VZ1/VXY2/VZ2`, then executes **`RTPT 0x0280030` at `0x80010D34`**.
- `0x80011214` then reads back `SZ1/SZ2`, `SXY0/SXY1`, `MAC0` and stores them to
  scratchpad (`0x1F80001A+`) — a **projected-value cache**, and runs `NCLIP`.

### Correction

Earlier readings were wrong on two counts: the scratchpad slot is a GTE-output
cache (not a matrix), and `s1` is a vertex buffer (not a transform table).

## Where the matrix (RT/TR) comes from — open

There is **no `CTC2` (rotation/translation load)** anywhere in
`0x80010C00–0x80010D64`, so the GTE matrix is set elsewhere. Prime candidate:
**`JAL 0x80011AB0` at `0x80010C88`** (called before the vertex loop; argv/global
setup around `0x800AA1xx`, `0x8009Cxxx`).

**Next:** `disasm addr=0x80011AB0 count=…` to find the `CTC2 $RT… / $TRX…` matrix
load — that is the view/model transform the camera live.
