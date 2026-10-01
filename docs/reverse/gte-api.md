# GTE API + projection control (M1)

Identified by scanning the EXE text for `CTC2` and confirmed by `disasm`.

## PsyQ libgte entry points (confirmed)

| addr | function | evidence |
|---|---|---|
| `0x8001BA98` | **`InitGeom`** | `ZSF3=341`, `ZSF4=256`, `H=1000`, `DQA=-4194`, `DQB=0x140`, `OFX=OFY=0` |
| `0x8001BA68` | **`SetGeomScreen(h)`** | `CTC2 $a0, $H ; JR $ra` |
| `0x8001BA78` | **`SetGeomOffset(ofx, ofy)`** | `SLL <<16` → `CTC2 $OFX/$OFY ; JR $ra` |

## Where MOH's `H` actually comes from (disasm)

| addr | code | role |
|---|---|---|
| `0x80013764` | `ADDIU $t8, $zero, 400 ; CTC2 $t8, $H` | **world path — H is a hardcoded constant 400** (matches the ring's dominant cluster) |
| `0x80013A84` | `LW $t8, 8($v0)` (`v0 = 0x8009696C`) → `CTC2 $t8,$H` | H read from **RAM `0x80096974`** |

## Live test — RAM poke does NOT control world FOV

- Ring `H` before: `{133, 400}`. `0x80096974` read as `0x00000000`.
- Wrote `0x80096974 = 0x00000320` (800) byte-by-byte via `write_ram`; the write
  **persisted** (read back `20030000`).
- Ring `H` after: `{400}` — 800 never appeared, and the 133 cluster stopped.
- **Conclusion:** the world projection uses the hardcoded immediate at
  `0x80013764`; the RAM site is not the world path. Live RAM poking cannot change
  world FOV.

## So Phase 6 needs a code patch + a mod plugin

Widening world FOV means changing the `ADDIU` immediate at `0x80013764`. The
supported mechanism is a **trusted plugin** using
`psx_mod_write_code_word(0x80013764, ADDIU $t8, newH)` at activation (it is
save-safe and goes through the executable-RAM path). That plugin must be:

1. compiled into the game target (`target_sources(psx-runtime PRIVATE …)` —
   `psxrecomp_add_game_runtime` does not parse `EXTRAS_SOURCES`);
2. declared in a `mods/preloaded/packages/<id>/<version>/manifest.toml`
   `[[plugin]]` so the plan activates it;
3. (for entry hooks) listed in `[recompiler] mod_function_entry_funcs`.

This is an **enhancement-phase shim on a proven LLE foundation** — the
CLAUDE.md carve-out — not a faithfulness hack.

`fov ≈ 2·atan(screen_w / (2·H))`; scaling that constant scales FOV.

## Matrix (RT/TR) sites — view-matrix hunt

75 `CTC2` matrix-load sites. The *view* matrix is one of them; find it by
`watch`-ing which site's source changes with camera rotation but not object
animation. Clusters: `0x80013BE4–0x80014348`.

## Scan method (reusable)

`read_ram` the EXE text (`0x80010000`, `len=0x2A000`) then match
`(w & 0xFE000000)==0x48000000 && ((w>>21)&0x1F)==6`; cop2 ctrl = `(w>>11)&0x1F`
(0-4 RT, 5-7 TR, 24 OFX, 25 OFY, 26 H, 27 DQA, 28 DQB). Script: `scan_ctc2.py`.
