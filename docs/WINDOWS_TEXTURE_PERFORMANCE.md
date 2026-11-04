# Windows texture-loading checkpoint

The Windows renderer supports DDS replacement packs, content-keyed GPU caching
and asynchronous loading. This checkpoint preserves the November 2, 2026
texture work before its Linux port. Native Linux remains a separate development
branch; the controls and D3D11 worker behavior below are Windows-specific.

## Runtime behavior

- Pack filenames start with the original texture's 16-digit decoded-pixel hash.
  Category folders are `rink`, `logos`, `jerseys`, `glow` and `other`.
- DDS replaces PNG/BMP with the same key. Supported DDS variants include BC1,
  BC2, BC3, BC7, RGBA8 and BGRA8. Stored mip levels upload together; PNG/BMP
  decoding uses WIC and produces GPU-generated mipmaps.
- Replacement GPU images are cached per pack image, independent of guest slots
  that may repeatedly cycle through the same content.
- Background workers read/decode images. DDS GPU creation also runs on those
  workers using the multithread-capable D3D11 device. PNG mip creation stays on
  the render thread. Original game pixels are used while replacements load.
- The render thread publishes ready images at flips, with an image-count limit
  and an approximately 4 ms soft time budget. One operation may exceed that
  budget; it is not a hard frame-time guarantee.

Enable Hi-res textures in F1 with render scale at least 2x. Put the pack under
`%LOCALAPPDATA%\NHLHitz2003\texture_pack`, or select a directory explicitly with
`RECOMP_TEXPACK`. Game data and texture packs are not distributed in this repo.

F8 toggles the installed pack through the same settings/cache-invalidation path
as F1 and persists `[textures] enabled`. Native 1x scale, a missing pack and held-key
auto-repeat do not toggle it. F8 is reserved by the game and cannot be assigned
to a gameplay action in config.exe.

| Setting | Default | Purpose |
|---|---:|---|
| `RECOMP_PACK_CACHE` | 1 | Reuse replacement GPU images; 0 selects synchronous uncached loading |
| `RECOMP_PACK_CACHE_MB` | 1536 | Estimated cache-owned GPU bytes; retained guest references can extend residency |
| `RECOMP_PACK_ASYNC` | 1 | Background loading; 0 keeps cache with synchronous reads/creation |
| `RECOMP_PACK_LOG` | 0 | Log replacement assignments, content keys and hit/load/queued state |

The cache default is a desktop setting, not a recommendation for low-memory
systems. Actual process/GPU residency includes allocations outside this counter.
Frame telemetry adds `pack_loads`, `pack_load_ms`, `pack_create_ms`,
`pack_cache_hits` and `pack_cache_mb` when `RECOMP_FRAME_CSV` is enabled.

## Recorded gameplay evidence and limits

The original 4x / 8x MSAA / 16x AF uncapped test averaged 54.08 FPS, with a
5.97 FPS 1% low and 757 intervals above 50 ms. Texture creation coincided with
99.3% of those slow frames. Repeated image decoding, upload and mip creation
occurred on the rendering thread.

Subsequent cache/background/DDS tests recorded approximately 69–74 FPS averages
and 35–44 FPS 1% lows. The final worker-created DDS run recorded one interval
above 50 ms, a longest interval of 84 ms, and 12,899 cache hits versus 307 loads.
All of the recorded tests completed the first period without a reported crash
or texture-creation failure. The measured DDS cache working set was 127 MiB;
that is not the full pack's disk size.

These were different matchups, so neither averages nor stall counts are a
controlled before/after comparison. The original Phoenix-at-Florida workload
has not yet been reproduced with fixed inputs. First-period evidence is not
full-match stability, and the results do not establish steady 60 FPS.
Low-memory stress testing remains open.

Checkpoint verification on November 3: the normal Windows build, GPU RGB565 /
X1R5G5B5 rendering/sampling test and replacement-key/disabled-pack hash guard
test pass on an RTX 3070. A separate DDS startup smoke and retained executable
hashes are kept in ignored local analysis artifacts. No pack files or binaries
belong in the source commit.
