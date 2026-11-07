# Linux texture performance and diagnostics

## Implemented

The OpenGL renderer accepts content-hash-named DDS, PNG and BMP replacement
textures with the original pack folder structure. DDS wins within the selected
source directory. BC1/BC2/BC3 and BC7 UNORM are uploaded compressed when the
context supports them; stored mip levels are retained. Unsupported or malformed
DDS files fall back to PNG/BMP for that hash, then to the original game texture.
PNG/BMP images receive generated mipmaps. Native texture dimensions still govern
guest UV normalization. Mip-aware filtering and optional anisotropy apply to
replacement textures.

One low-priority worker reads and decodes immutable pack files. Only the render
thread creates or deletes OpenGL textures. A content-keyed LRU owns replacement
GL names; guest entries keep indices, so eviction cannot leave stale handles.
Default estimated resident storage is 128 MiB, with 64 MiB of reserved pending
image payload. Images used in the current frame are protected from eviction;
if admission is blocked, the original texture remains visible. The upload pump
runs after presentation, with a soft 2 ms budget (one driver call can exceed it).
Decoder/driver scratch allocations and driver-retained storage are not included
in these payload limits.

No replacement pixel hash or pack scan runs when disabled, at native 1x, or
without an installed pack. Exact every-use native texture validation is retained,
including same-frame and tail mutations. This change does not adopt the Windows
renderer’s sampled validation policy.

## Configuration

Settings default to `$XDG_DATA_HOME/nhlhitz2003/settings.ini`, or
`~/.local/share/nhlhitz2003/settings.ini`. Under `[textures]`, `enabled=1` enables
the pack and `rink`, `logos`, `jerseys`, `glow`, `other` enable categories.
The default pack is `texture_pack` beside that INI, then beside the executable.
The XDG source wins for duplicate hashes. `RECOMP_TEXPACK` selects an explicit
root and suppresses default roots. Original files are never changed.

| Environment variable | Default / effect |
|---|---|
| `RECOMP_HIRES` | Override enabled setting with 0 or 1 |
| `RECOMP_RENDER_SCALE` | 1; replacements activate at 2–5 |
| `RECOMP_PACK_CACHE_MB` | 128; resident storage estimate, 1–2048 |
| `RECOMP_PACK_PENDING_MB` | 64; pending image payload, 1–256 |
| `RECOMP_PACK_ASYNC` | 1; 0 reads synchronously for diagnosis |
| `RECOMP_PACK_UPLOAD_MS` | 2; soft upload-pump budget |
| `RECOMP_PACK_LOG` | 0; 1 logs residency/fallback events |
| `RECOMP_PACK_RINK` etc. | Category override, 0 or 1 |
| `RECOMP_ANISOTROPY` | 1; up to 16, clamped to context capability |

Frame CSV includes per-frame load/creation time, hits, cache/pending MiB,
queue depth, evictions and failures. File read/decode time is worker time;
creation time is render-thread time and may include driver stalls.

## Window title

The title matches Windows: `FPS`, `CPU`, `GPU`, and draws/frame. CPU measures
this process’s aggregate kernel+user time divided by elapsed time and online
logical processors: 100% means all processors busy. It is not the percentage of
the busiest thread. FPS and CPU are sampled approximately once a second.

GPU telemetry samples per-process DRM engine counters from `/proc/self/fdinfo`
on a background thread once per second. Duplicate descriptors are deduplicated;
the busiest device/engine percentage is shown. Missing/unsupported telemetry is
`N/A`, including the tested WSLg D3D12 backend. Device-wide utilization is not
substituted for process utilization. `RECOMP_PERF_LOG=1` records the title values
once per second (`gpu_pct=-1` means unavailable).

## Windows 11 Linux test environment

The current test machine uses dedicated Ubuntu 24.04 WSL2 distro
`NHLHitz-Linux`, source/builds on ext4 at `/home/hitz/nhlhitz2003`, GCC 13.3,
CMake 3.28, Ninja, SDL2, libepoxy, libpng, OpenSSL, FFmpeg development libraries,
Python venv, Capstone, psutil and GDB. Generated guest code uses `-O2`.
Toolkit revision remains `f0f8a461496a4f23d9b0aeaa284b8d70ba7ab82b`.

Hardware GL was verified as Mesa 25.2.8 OpenGL 4.6,
`D3D12 (NVIDIA GeForce RTX 3070)` using:

```sh
export GALLIUM_DRIVER=d3d12
export MESA_D3D12_DEFAULT_ADAPTER_NAME=NVIDIA
export LD_LIBRARY_PATH=/home/hitz/.local/sdl2/lib
export SDL_AUDIODRIVER=pulseaudio
```

Ubuntu’s SDL 2.30.0 PulseAudio enumeration hung because its 256 KiB hotplug
thread stack could not accommodate this executable’s approximately 458 KiB
static TLS. The test environment installs unmodified upstream SDL 2.32.10
(`5d249570393f7a37e037abf22cd6012a4cc56a71`) under
`/home/hitz/.local/sdl2`; it uses the default thread stack and checks creation
failure. System SDL and the toolkit are unchanged. Audio works with this runtime.
These are WSLg results, not native Radeon/Linux performance measurements.

## Validation, 2026-11-03

Nine CTest checks pass, including bounded DDS/PNG parsing and CPU/GPU telemetry
math. The OpenGL backend test passes combiners, 16-bit targets, vertex feedback,
cached state and exact texture mutations/eviction. The separate
`hitz_gl_texture_pack_test` passes actual GPU pixel/alpha readback, stored mip
levels, malformed-DDS PNG fallback, synchronous/asynchronous loading, cache
reuse, eviction/reload, and disabled/1x guards. GPU tests require a real context
and are intentionally separate from portable CTest:

```sh
cmake --build native/build-linux --target hitz_gl_backend_test hitz_gl_texture_pack_test --parallel 2
ctest --test-dir native/build-linux --output-on-failure
native/build-linux/hitz_gl_backend_test
native/build-linux/hitz_gl_texture_pack_test
```

The unchanged Linux reference binary was retained before renderer changes:
SHA256 `ffa21076d5d9d1ea950d5bd1743b068ca3c3d5d7c5d24b7d80f1ce33eb9e68f1`.
The bounded 420-second `wsl_baseline03` run reached St. Louis–San Jose live play
with audio. Visually confirmed frames 13201–13578 averaged **9.89 FPS**, with
**8.01 FPS 1% low**, 101.51 ms median, 114.93 ms p95, 120.98 ms p99 and 874.9
draws/frame (378 frames, 38.21 seconds, no capture frames). This is a short
gameplay baseline, not a full-match result. Startup/menu FPS is excluded.

The 420-second `wsl_parity01` run loaded the existing BC7 pack at 2x, reached
Nashville–Atlanta live play (capture 12600, first period 2:27), and ended at its
intentional deadline without a guest crash. No pack failures were reported.
Frames 12601–12936 averaged 9.52 FPS with 6.90 FPS 1% low, 103.16 ms median,
123.78 ms p95 and 136.77 ms p99. The cache settled at 88.01 MiB with no pending
payload. Inclusive draw CPU time averaged 84.83 ms/frame, exact texture
validation 4.17 ms, and decode 1.87 ms. Different teams and render scale prevent
attributing the difference from the reference to this change.

The one-second process/thread monitor recorded peak RSS 1523.82 MiB, no swap,
and 12.83% mean CPU normalized over 16 logical processors. The last sample had
threads near 100% and 87% of one processor; low normalized CPU therefore does
not mean the main workload is unconstrained. An actual X11 window-title read
confirmed `FPS: 46.1 | CPU: 10% | GPU: N/A` during a menu scene; this is a display
check, not a gameplay FPS result.

A separate ten-second CPU-clock profile during the entrance sequence collected
2026 samples without loss. Approximately 46.9% self time fell in guest
`sub_0021B3E0` (which contains a GPU-register polling loop), and 27.8% in libc
copy instructions beneath rendering. These observations identify tracing
targets; they do not establish that bypassing a guest wait is correct. The
profile window differs from the live-play timing window above.

## Remaining performance work

Finish controlled DDS comparisons and memory-pressure regression runs, then repeat
native-scale profiling with fixed teams, arena, camera and sequence. Prioritize
measured render submission/vertex streaming costs before speculative threading.
Trace the sampled guest polling loop’s synchronization and the render-path
copies before altering either; preserve command ordering and immutable inputs.
Evaluate direct native DXT upload with exact validation and pixel comparisons;
then bounded vertex/index reuse and state/command batching. Measure shader
compile misses before implementing warmup or a disk cache keyed to driver and
shader version. Compare cold/warm pack loading and 1x/5x settings separately.
Full matches, low-RAM native Linux, unsupported BC7 hardware and sustained 60 FPS
remain unvalidated. No 60 FPS claim is made.
