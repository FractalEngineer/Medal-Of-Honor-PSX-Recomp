# Debug tooling (on our `vr-dev` fork)

## `disasm` — guest MIPS disassembly

Stock psxrecomp's debug server has **no disassembly**, which blocked M1 (we could
see values and writers but not code). Added in the fork (`d58db909`):

```text
python psxrecomp\tools\debug_client.py --port 4370 disasm addr=0x8001121C count=64
```

- Reads live guest RAM (`psx_read_word`), so it disassembles EXE code **and**
  RAM-installed / overlay code.
- Backed by the recompiler's own decoder: `recompiler/src/mips_decoder.cpp`,
  bridged to the C debug server by `runtime/src/disasm_shim.cpp` +
  `runtime/include/psx_disasm.h`; command registered in
  `runtime/src/debug_server.c`; sources wired in `runtime/runtime.cmake`.
- Verified: `0x8001121C` region disassembles to real MIPS, and `SH $t3, 960($s3)`
  matches the `0x1F8003C0` scratchpad write seen in `wtrace`.

## Other useful commands (stock)

| command | note |
|---|---|
| `gte_ring_dump count=N` | GTE RTPS/RTPT ring: V, RT, TR, H/OFX/OFY, SXY, SZ, caller_ra |
| `gte_state` / `gte_frame_stats` | GTE exec count + per-frame projections |
| `wtrace_range lo=… hi=…` / `wtrace_dump addr_lo=… addr_hi=…` | RAM write trace with RA/registers |
| `read_ram addr=… len=…` | guest RAM read |
| `fntrace_*`, `fn_entry_*` | function-entry tracing |
| `input <hex>` | controller override (d-pad 0x0010/0x0020/0x0040/0x0080, Start 0x0008, Cross 0x4000) |
