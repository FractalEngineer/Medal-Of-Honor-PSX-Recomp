# Build and proof retention

Updated: 2026-10-06. Current status: [VR_HANDOFF.md](VR_HANDOFF.md).
Cleanup reclaimed 11.71 GiB. [VR_CLEANUP_RECEIPT.json](VR_CLEANUP_RECEIPT.json)
records the compact result; full manifests and hashes remain local under
`analysis/cleanup-20261006/`.
The published v0.1.3 ZIP, original saves/disc, current Release build, registered
worktrees, shelved jitter branch/build, Ghidra project and accepted evidence remain.

## Current layout

- `build-release` is the shared normal/weapon-control target, using root pinned
  submodules. Separate candidate builds require an explicit `-BuildDirectory`.
- `dist/` retains the exact published `moh-0.1.3-windows-x64.zip`; extracted
  smoke/staging copies are generated outputs and can be recreated from it.
- `analysis/weapon-capture/` retains accepted headset controls, final native/
  synthetic controls, profile fixtures, native inventory, release/master receipts
  and the Ghidra project. Receipts keep their historical source/binary hashes.
- `analysis/vr-proof/` retains current color/visibility and HUD/compass discovery.
- `analysis/archive/20261006/` contains historical-evidence.zip,
  older-release-files.zip and root-runtime-logs.zip. Each was read back and
  checked against a SHA-256 manifest before its loose source files were removed.
- `analysis/archive/20261006/access-restricted/` retains two old readback
  temporary-directory remnants intact. Windows denied access to their children;
  they were moved without changing permissions or deleting unreadable contents.
- Registered worktrees and branch refs were not deleted. The old release
  worktree has local changes and provides the verified packaging emitters.
  Unrelated main-framework edits remain untouched.

## Retrieve historical evidence

Archive member paths start at the game repository root, such as
`analysis/vr-proof/jitter-controls-20261005/...`. `CLEANUP_MANIFEST.json` inside
an archive records each file's bytes and SHA-256. A historical path in a receipt
may refer to an archived member, not a currently loose file.

Use Python's `zipfile` or an archive manager to extract only the required
case into a separate analysis directory. Do not restore old save copies over
`saves/`, and do not bulk-unpack old runtime caches into the active build.
The original relative paths and measurements remain available in the archive.
Do not substitute a later executable or regenerate a historical receipt's hashes.

Future captures belong under ignored `analysis/`, with exact build/settings,
native control, measured result and a compact receipt. Keep selected source
images needed to recheck a measurement; archive superseded runs after validating
retention. Do not bulk-read proof for general session context.

## Earlier tracked-proof cleanup

Completed 2026-10-02 after user confirmation. Removed 57 redundant, exploratory
or large Release PNGs. The proof working tree fell from 118.86 MiB to 6.51 MiB:
13 representative PNGs, 170 JSON receipts and seven existing evidence READMEs.
No runtime, framework or measurement results changed.

## Reading in a new session

Start with [VR_EXECUTION_PLAN.md](VR_EXECUTION_PLAN.md) for completed milestones
and next work, then the relevant portion of
[VR_PHASE9_STATUS.md](VR_PHASE9_STATUS.md) for measurements and corrections.
Use [the proof index](../../vr/proof/README.md) only when checking evidence.
Do not bulk-read proof JSON or inspect every image for general session context.

## Retained material

- Four original level-translation images referenced by the matrix notes and brief.
- Both Debug zero-offset eye pairs for independently checking pixel equality.
- Both Debug offset-24 eye pairs for rechecking pixel counts and parallax.
- One offset-24 composed-present screenshot demonstrating actual SBS output.
- All raw TCP receipts, fingerprints, save provenance, manifests, comparisons
  and reproduction instructions, available for targeted investigations.

## Archived material

Old no-op/temporal coverage PNGs, exploratory slot-2/slot-3 captures, duplicate
SBS composites, additional presentation screenshots, watchdog-run PNGs and
large Release textures are removed from the current checkout. Their recorded
findings, limitations and corrections remain in the docs and JSON receipts.
The historical source images are available in game commit `231650447bdc219629559a16f0dd7d229ee1ce49`.
Use an isolated checkout of that revision if an old image comparison needs to
be recomputed. A receipt alone does not substitute for its source images.

Future bulk captures should go to the already ignored `/analysis/vr-proof/`
directory. Promote only selected useful evidence into `/vr/proof/`.
This cleanup reduces the working tree; existing Git history is unchanged.

## Validation

After cleanup, the retained zero-offset pairs again decoded to zero differing
pixels; the offset pairs again measured 78,026/77,961 differing pixels and
wall/ground correspondence at -5px/-13px. All 170 retained JSON receipts parse,
and updated Markdown links resolve. No game instance was launched.
