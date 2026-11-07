# Linux perf: workers=2 + FPS overlay (clean ship)

Branch work from Nobara / NHL Hitz Pro Linux measurement (2026-11), ported to
this 2003 Linux GL launcher where the same knobs apply.

## Shipped defaults (`play.sh`)

| Knob | Value | Notes |
|---|---|---|
| `RECOMP_GL_WORKERS` | **2** | Parallel exact snapshot `memcmp` for large spans |
| Texture validation | **Exact** (unset `RECOMP_GL_TEXTURE_VALIDATE`) | Soft-dirty watch + snapshot/XXH |
| `RECOMP_GL_DRAW_COALESCE` | **Off** (unset) | Unsafe merges caused model glitches on Pro |
| `RECOMP_GL_FPS_OVERLAY` / `HITZ_FPS_OVERLAY` | **1** by default locally | `#ifndef NHL_RELEASE`; all HUD fields ~**500 ms** |

Opt out of the HUD: `HITZ_FPS_OVERLAY=0 ./play.sh`.

## Measured (Pro @ 2×+HIRES on AMD HD 8600M)

- Phase 1 baseline ~25 FPS; workers=2 ~**+2–3 FPS** with lower `hash_ms`
- Exact validation kept; visually clean
- **Do not** enable `RECOMP_GL_TEXTURE_VALIDATE=sampled` or draw-coalesce as defaults — severe texture/model corruption on Pro

## Unsafe experiments (not defaults)

- Sampled / deferred dirty checks (`sampled`)
- CPU/FFP draw coalesce without full GVS batching

## High-end leftovers

- D3D11-safe once-per-flip exact validate (opt-in only after soak)
- Proper GPUVS draw batching
- Fill / gfx-thread overlap on faster GPUs

60 FPS @ 2×+HIRES on entry-level mobile dGPUs is often GPU-fill limited; treat
workers + exact validation + overlay as the sensible clean baseline.
