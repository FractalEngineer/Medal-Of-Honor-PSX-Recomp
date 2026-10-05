# Working through the VR backlog

[VR_ALPHA_TODO.md](VR_ALPHA_TODO.md) is the single task list.
[VR_HANDOFF.md](VR_HANDOFF.md) records the current release, build layout and
working rules. This file describes execution, without duplicating the backlog.

1. Choose the next unchecked task in todo order, respecting explicit user
   deferrals. Jitter remains shelved; do not resume it as part of weapon/HUD work.
2. Read only the relevant implementation notes and receipts. Define the native
   control and observable acceptance criteria before changing behavior.
3. Work on a fix/feature branch from the current baseline. Keep each candidate
   reviewable and preserve accepted settings and native fallback.
4. Build the actual launcher target and run focused desktop/native controls.
   Record exact binary/settings, nonzero rollback checks and unresolved limits.
5. Prepare several independent candidates when useful, then offer a single
   headset batch. Ask whether Quest/VDXR is ready. Gameplay runs default to
   30 seconds with movement enabled; keep synthetic and hardware evidence distinct.
6. Record the user's assessment, update the todo and current handoff, then commit
   or publish at the authorized checkpoint. Unaccepted candidates stay out of releases.

Older execution hypotheses remain in Git history at the v0.1.3 tag and in
[VR_PHASE9_STATUS.md](VR_PHASE9_STATUS.md); they are not another active task list.
