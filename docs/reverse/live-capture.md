# First live capture (GTE ring) — verified

Verified end-to-end on the Debug build. The framework's always-on GTE projection
ring already provides camera-space geometry; no capture code of our own is needed
for this phase. (A separate `gte_capture` seam was added then reverted — it
duplicated this ring.)

## Reproduce

```powershell
cd C:\Users\titan\Desktop\Github_Projects\Mine\Medal-Of-Honor-PSX-Recomp
.\build-debug\Medal_of_Honor__Recompiled.exe --headless `
  --game game.toml --disc "Input\medal-of-honor\medal-of-honor.cue"

# in another shell (client lives in the framework)
python psxrecomp\tools\debug_client.py --port 4370 ping
python psxrecomp\tools\debug_client.py --port 4370 gte_state
python psxrecomp\tools\debug_client.py --port 4370 gte_ring_dump count=16
python psxrecomp\tools\debug_client.py --port 4370 gte_frame_stats
```

`--headless` boots with no launcher/UI and prints
`debug server LISTENING on 127.0.0.1:4370`. Reaching 3D may need Start/Cross
input (`input 0008`, `input 0000`, `input 4000`).

## Observed (first successful run)

- 3D reached ~frame 850; `gte_state` reported `gte_exec = 28517`, ring `total = 8188`.
- Every sampled entry had the **same caller** `ra = 0x80053FF8`, `cmd = 0x00280030`
  (RTPT, triple projection).
- Example entry (frame 830):

```text
V0=[0,-233,42]  V1=[27,-193,60]  V2=[17,-232,40]     # model-space vertices
RT=[-3745,0,1660, 0,2560,0, -1660,0,-3745]           # combined 3x3 (model*view), Q12
TR=[156,162,628]                                     # translation
H=443  OFX=0xFF0000 (255.0)  OFY=0x770000 (119.0)    # projection config
S0=[385,131] S1=[377,151] S2=[373,131]               # screen XY
SZ=[589,562,584]  FLAG=0x1000
```

## Interpretation

- `V` is the pre-transform (model/local) vertex; `RT`+`TR` are the combined
  modelview the game applied before RTPS. **Camera-space vertex = (RT·V + TR·4096) >> 12**
  — i.e. the geometry stereo needs *is* recoverable from this ring.
- `OFX/OFY` are 16.16 fixed (centers 255,119 → ~512x240 screen); `H` is the
  projection distance (GTE `H`).
- `ra = 0x80053FF8` is the guest routine issuing RTPT — the first anchor for the
  camera/render map (see `functions.csv`).

## Go / No-Go (plan's key checkpoint)

**Yes** — camera-space geometry is obtainable before final screen projection,
via this ring. The renderer still never sees it; any live per-eye feed will be a
separate design at the same GTE seam. But for the RE phase the data is here.
