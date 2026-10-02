# VR proof retention

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
