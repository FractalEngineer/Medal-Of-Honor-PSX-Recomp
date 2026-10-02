# Exploratory replay captures: scene identity correction

## Retention update: 2026-10-02

The exploratory and confirmed temporal PNGs were removed. Their raw receipts, comparisons and this scene-identity correction remain. Any visual reinspection requires historical images or a fresh measured capture.

The complete pre-cleanup image bundle is in game commit `231650447bdc219629559a16f0dd7d229ee1ce49`.
See [the proof index](../README.md). The run descriptions below record the original measurements.

These files predate the verified paired-eye bundle in `../stereo-pairs/`.
After the user corrected the scene/slot selection, TCP slot 3 was explicitly
reloaded and completed status was saved. The earlier `draw/`, gameplay images
and fingerprint are retained as exploratory artifacts; they are not used to
claim animated-scene coverage, parallax or timeline restoration.

`confirmed/` holds later ordinary temporal replay dumps. Their decoded baseline/
replay comparisons are non-identical (including 50 pixels in generation 5).
This comparison does not isolate whether a difference comes from temporal
generation selection, a changed scene checkpoint or incomplete draw coverage.
No explanation is asserted. The new paired zero-offset control removes the
temporal baseline comparison and gives identical eye pixels with both enemies.

See `docs/reverse/VR_PHASE9_STATUS.md` for the appended correction and the
verified paired bundle's README for save provenance and reproduction.
