# Native Linux development

The project now targets Windows and native Linux. Windows has an existing working
player; native Linux integration is in progress. The acceptance target is a
complete match at a steady 60 FPS on this environment. The native player now
boots, renders menus and reaches live Exhibition play. Full-match
completion and 60 FPS have not been validated.

## Linux implementation checkpoint

The platform-specific build is selected automatically on Linux. New host code
uses SDL2, OpenGL, XDG save locations, case-insensitive asset lookup and anonymous
sparse memfds for raw partitions. Disc assets keep their original case. Raw
partition lengths are fixed with memfd seals; loose save files persist normally.
The project-owned Linux x86-64 fault decoder routes APU, AC97 and OHCI register
accesses to the existing hardware models. Guest timing and completion mechanisms
are retained.

Texture validation checks every byte on every use. A bounded 32 MiB cache keeps
raw snapshots for exact `memcmp` validation, with XXH3 full-span hashing when
the snapshot budget is exhausted or allocation fails. Set
`RECOMP_GL_TEXTURE_SNAPSHOT_MB=0` to test the hash-only fallback; the accepted
range is 0–128 MiB. Same-frame mutations (including tails), budget eviction and
subsequent GPU texture refresh pass readback checks. The legacy CSV fields
`hash_ms` / `hash_bytes` now cover all texture validation, including comparisons.
The pinned xxHash header and license are in `native/third_party/`.

The OpenGL backend supports CPU-transformed batches, register combiners, four
texture stages, native 1555/RGB565 targets, shared depth/stencil buffers, surface
copies and presentation. Optional GPU FFP and decoded vertex programs use the
same packed input layout and CPU verifier as the Windows implementation, with
OpenGL transform feedback returning the 29-float verification record.
Register combiners (`RECOMP_D3D11_RC=1`), GPU FFP and GPU vertex programs are
enabled by the launcher after the `linux_verify05` live-game CPU/GPU comparison
reported zero mismatches across more than 5.6 million sampled vertices. Unsupported
batches retain CPU rendering. `RECOMP_GPU_FFP=0` and `RECOMP_D3D11_GPUVS=0`
provide diagnostic opt-outs; the historical D3D11 name also selects the Linux path.

Checks completed on AMD A6-7310 / Radeon R4 (Kabini), Mesa 25.0.7, OpenGL 4.5:

- The native ELF bring-up executable completed a 360-second scripted probe
  without a guest crash; captured frames show Exhibition setup and rink entrance.
  The subsequent `linux_verify05` GPU-verification probe reached live play;
  `linux_verify05_frame_gl_005280.png` shows 2:50 remaining in the first period.
  A later capture shows 2:02 remaining. Its supervisor was interrupted by the
  host and the still-running child was stopped during recovery; the run metadata
  distinguishes this from a guest crash. Both probes used guest optimization
  level 0. The optimized build now links and reports guest optimization level 2.
- The first optimized live-play sample (`linux_opt07`, frames 15960–16799)
  averages 9.62 FPS with a 5.37 FPS 1% low; seven screenshot frames were excluded.
  This is 833 measured frames / 86.57 seconds, GPU verification off, uncapped,
  no active compiler, prior test asset cache reused. Median is 100.11 ms and p95
  138.39 ms. Texture validation averages 25.50 ms/frame and the whole backend
  56.40 ms/frame. The Linux 60 FPS target is far from met.
- Seven CTest regressions pass: guest memory aliases and reserved-window ownership,
  x86 MMIO registers/widths/flags/RIP, file lookup and anonymous partitions,
  guest-clock pause/resume, NIS buffer lifetime, material and lighting.
- Hardware tests pass 32 scalar/GPU register-combiner comparisons, 16-bit target
  readbacks, GPU FFP positions, the captured depth-cancellation fixture and a
  decoded Xbox vertex program with transform-feedback readback.

These checks do not establish full-match stability or 60 FPS. Source
generation completed: 19,928 translated functions, zero translation failures,
2,391,211 generated lines, and 20 unresolved target stubs. The Linux build adds
a project-owned strict check to its build-directory stub copy: a literal RET
is allowed, while other missing code stops with its address instead of silently
returning. Unimplemented instructions also trap by default. Original generated
files remain untouched. Logs are `analysis/linux_prepare.log` and
`analysis/prepare_*.log`. Build serially on this 3.2 GiB RAM host.

Build and run:

```bash
bash play.sh                    # build and run the native Linux player
bash play.sh --build-only
.venv/bin/python scripts/probe_linux.py --tag linux_probe_new --seconds 120 \
  --env RECOMP_GL_CAPTURE_PREFIX=../analysis/linux_probe_new_frame_ \
  --env RECOMP_GL_CAPTURE_EVERY=120
```

Use a new probe tag each time. Probes isolate user data under `analysis/`, save
stdout/stderr, frame CSV and termination metadata, and terminate only their own
child process group. Signal handlers record supervisor interruptions, and a
Linux parent-death signal stops the game if its supervisor is killed. Deadline,
SIGTERM and SIGKILL cleanup checks pass. Create `analysis/TAG.stop` to end a probe. A deadline is
reported separately from a guest crash. Frame CSV intervals measure renderer
presentation callbacks rather than physical display scanout; exclude startup,
shader compilation and menu windows from gameplay performance claims. Set
`RECOMP_PRESENT_HZ=0` for an uncapped diagnostic run; normal Linux presentation
defaults to 60 Hz at native scale with no MSAA.

For faster initial crash checks, developers can configure
`-DHITZ_GUEST_OPT_LEVEL=0`; those runs are not performance evidence. `play.sh`
explicitly selects optimization level 2 for normal play. Probe metadata records
the binary SHA-256 and binary-reported optimization level (`--build-info`) so
bring-up and performance runs can
be distinguished. For a retained bring-up executable whose build cache has
since changed, pass `--guest-opt-level 0` explicitly.

Keyboard defaults: Enter = Start; arrows = D-pad; Z/X/C/V = A/B/X/Y;
W/A/S/D = left stick; 1/3 = triggers; Q/E = White/Black. F11 toggles
fullscreen and Esc exits. SDL game controllers are also supported.

## Development dependencies

On Debian/Ubuntu, from the project root:

```bash
bash scripts/install_linux_deps.sh
source .venv/bin/activate
```

This uses the pinned toolkit's dependency installer for GCC/build-essential,
CMake, pkg-config, SDL2, OpenGL, libepoxy, OpenSSL and FFmpeg development libraries
(libavcodec, libavformat, libavutil and libswscale). The project helper adds Ninja,
GDB, Clang/LLD, ripgrep and Python environment support. Capstone 5.0.7 and psutil
are installed in `.venv`. System installation requires sudo authentication in a
terminal. For other distributions, consult the toolkit's
`tools/linux/install_deps.sh`; this project helper currently supports Debian/Ubuntu.

The initial Debian 13 workspace already has GCC/G++ 14.2.0 and Make. Project-local
CMake 4.4.4, Ninja 1.13.2, pip, Capstone 5.0.7 and psutil 7.2.2 have been installed.
Python is currently 3.13.5; the Windows instructions specify 3.12. The XBE parser,
lifter hooks and all 214 CFG comparisons pass on this host. Full source generation
has completed. System dependency installation is now complete after the owner
authenticated in a visible desktop terminal. Verified system tools include CMake
3.31.6, Ninja 1.12.1, Clang 19.1.7 and GDB 16.3. Verified libraries include SDL2
2.32.4, libepoxy 1.5.10, OpenSSL 3.5.7 and FFmpeg development components.

The standalone toolkit now configures and builds successfully with GCC/Ninja in
`native/build-linux-toolkit` (all 65 build steps). The build emits compiler
warnings; it has not been treated as a warnings-clean build. Its log is
`analysis/linux_toolkit_build.log`. This validates the toolkit's static libraries,
not the game-specific overrides, player linkage or gameplay.
The existing material and lighting regression programs also compile and pass
directly with the Linux C compiler.

## Local game data

The supplied ISO was extracted with XboxDev extract-xiso, built locally under
`tools/xiso/build/`. It produced 484 files totaling 2,064,188,924 bytes. Regenerating
the disc manifest matches both checked-in metadata files exactly, including XBE
SHA-256 `d38199733a472c601015b80c719945beac737361321a7dfca5e3376fdd75df8f`.
This checks file paths/sizes and executable identity; the checked-in manifest does
not contain hashes for every asset.

Local paths follow the existing pipeline:

- `extracted/`: original disc assets, preserving filename case.
- `game_files/default.xbe`: executable copied from the extraction.
- `game_files/default_analysis.json`: XBE parser output.
- `native/game -> ../extracted`: relative Linux symlink to disc assets.

All game data, generated C and analysis artifacts remain ignored by Git. Keep
the original case: Windows/Xbox case-insensitive lookups will need explicit
handling on Linux filesystems.

To regenerate the guest code after the initial parser output exists:

```bash
source .venv/bin/activate
python scripts/prepare.py
```

Generation is separate from compiling/running the host. Never hand-edit generated
sources or modify `tools/xboxrecomp` or the vendored `tools/xrdbg` kit.

## Initial porting work

The pinned toolkit already contains a POSIX Win32 compatibility layer, SDL2 audio,
input and framebuffer presentation, and an OpenGL 3.3 D3D8 backend. These provide
building blocks; the OpenGL D3D8 backend does not automatically replace this game's
custom NV2A pushbuffer D3D11 renderer.

1. Add platform selection to `native/CMakeLists.txt`. Its Windows libraries,
   D3D11 helper programs/tests and renderer are currently selected unconditionally.
   First compile the toolkit's Linux runtime and portable project regression tests.
2. Port the host entry point in `native/src/main.c`: Windows headers, exception
   handling, startup paths, window/event ownership and user-data locations need
   Linux equivalents. Preserve guest memory mappings, register TLS and dispatch.
3. Audit every project override against the toolkit's POSIX paths. Preserve Hitz
   fixes, particularly the dispatch gate, NIS lifetime and movie lifting fixes.
   Cover case-insensitive asset lookup, save paths and memory-only raw disk behavior.
4. Add a native rendering backend for the NV2A pushbuffer backend interface.
   Assess reuse of toolkit OpenGL code against the actual interface before choosing
   implementation details. Retain Windows rendering as a regression baseline.
5. Integrate SDL2 audio/controllers/keyboard and validate legal screens, movies,
   menus, an Exhibition match, complete matches, saves and frame pacing on Linux.
   Add Linux crash diagnostics separately from the unchanged Windows xrdbg kit.

The Windows stability/performance limitations in `STATUS.md` remain relevant.
Installing dependencies does not establish Linux build or runtime support.

## Performance launcher defaults (clean)

See [`PERF_LINUX_OVERLAY.md`](PERF_LINUX_OVERLAY.md): `RECOMP_GL_WORKERS=2`, exact
texture validation, coalesce off, and an optional on-screen FPS HUD
(`RECOMP_GL_FPS_OVERLAY` / `HITZ_FPS_OVERLAY`, ~500 ms refresh, `#ifndef NHL_RELEASE`).
Sampled validation and draw-coalesce are **not** defaults.
