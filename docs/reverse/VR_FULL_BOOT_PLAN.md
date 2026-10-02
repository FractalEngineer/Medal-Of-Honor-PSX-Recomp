# Normal boot VR checkpoint

1. Start XR native-screen submission at application startup; copy fresh native
   content before host OSD. Completed and headset accepted.
2. Navigate all native UI with table-independent controller mapping. Completed
   and user accepted; no save state required.
3. Hand off to genuine per-eye gameplay at the existing scene hook, then back
   to native screens when scene activity stops. Measured without any save load.
   Revisit the four-VBlank inactivity policy for unusually long gameplay stalls.
4. Measure video pacing with actual swap interval and turbo state. Completed:
   VR-only desktop VSync 0 preserves guest cap and restores near-real-time guest
   cadence. User confirms sound/framerate fixed.
5. Assess movie picture reconstruction separately. Bicubic trial tested and
   rejected by user preference; original presentation restored. Higher quality
   video work is deferred and requires native-source/oracle comparison first.
6. Preserve compact receipts and upstream inventory. User authorized commit and
   push: framework checkpoint 5bafeebf pushed, game pin updated, generation
   rerun with unchanged C output. Release and strict input checks pass; game
   checkpoint includes launcher, menu controls, pacing fix and compact receipts.

Accepted calibration and tracked-rifle settings are unchanged. Wrist HUD and
physical shot/barrel alignment remain separate future tasks. Close owned test
processes at completion. See VR_PHASE9_STATUS.md and VR_LAUNCH_RECEIPT.json.
