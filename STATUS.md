# NHL Hitz 2003 Windows / Linux Recomp — Status

## Linux parity work (2026-11-03)

Development resumed on `codex/linux-texture-parity`. Windows texture changes
were reviewed, built, tested and preserved as local checkpoint `26638e2`, then
cherry-picked here as `8d99b1e`. A dedicated Ubuntu 24.04 WSL2 environment now
builds and tests the Linux player with accelerated RTX 3070 OpenGL and audio.

Implemented Linux DDS/PNG/BMP pack lookup, compressed DDS uploads with stored
mips, bounded content-keyed LRU residency, asynchronous reads/decodes and
render-thread upload pumping. Exact native texture mutation checks remain.
The window title now displays FPS, CPU% normalized consistently with Windows,
and GPU% or `N/A`, updated approximately once per second.

Nine portable tests and both GPU test executables pass. The unchanged Linux
baseline reached live gameplay; the verified 38.21-second window averaged
9.89 FPS. This WSLg measurement is not a native Radeon result. A second
seven-minute run loaded the BC7 pack at 2x and reached live Nashville–Atlanta
play with zero reported pack failures and 88 MiB resident cache. Its verified
35.29-second gameplay window averaged 9.52 FPS; different settings and teams
prevent an optimization comparison. Controlled performance comparisons, full-match
completion and 60 FPS remain open. See
[Linux texture performance](docs/LINUX_TEXTURE_PERFORMANCE.md) for controls,
test details and the remaining optimization order.

## Linux branch checkpoint (2026-11-02)

Branch: `codex/linux-native`, based on upstream `main` at `291d1d9`.
The Linux changes have been reapplied after upstream rewrote its history and
removed historical documentation and packaging files. This file records only
the Linux checkpoint; it does not restore the removed Windows status history.
This section records the earlier paused checkpoint; work resumed above.

## Current native Linux result

Acceptance target: complete a full match at a steady 60 FPS on the current
AMD A6-7310 / Radeon R4 host (3.2 GiB RAM). **The target remains open.**
The native ELF player now boots through legal screens, renders the menus,
accepts scripted controller input, enters Exhibition setup and renders the
rink entrance sequence. The subsequent GPU-verification run reached live
Exhibition play (captured first-period clock progressing from 2:50 to 2:02).
The 360-second `linux_verify04` bring-up probe ended
at its deadline without a guest crash. It used unoptimized generated code,
so it is not a gameplay performance result. The optimized guest build now
links and reports optimization level 2.
Full-match completion and 60 FPS have not been validated.

Linux integration includes SDL2 input/audio, an OpenGL NV2A backend, x86-64
MMIO fault handling, case-insensitive assets, XDG loose saves and anonymous
bounded raw partitions. Native bring-up exposed and fixed two portability
bugs: guest contiguous-memory aliases colliding with host mappings, and
invalid file handles being dereferenced during first-run cache queries.
The complete guest virtual window is now reserved before alias placement;
invalid file queries return an error to the guest.

All seven portable CTest checks pass: guest memory aliases, MMIO decoding,
filesystem/partitions, host pause, NIS lifetime, FFP material and lighting.
Actual Kabini GPU checks pass scalar/register-combiner comparisons, native
16-bit targets, GPU vertex transform feedback, shared depth, cached blend/depth
state and the previously fixed Windows depth-cancellation regression.
The live-game CPU/GPU comparison has reported zero mismatches across more
than 5.6 million sampled vertices (`analysis/linux_verify05.stderr.txt`). GPU
FFP and vertex programs are now enabled by default, with unsupported batches
retaining CPU rendering. Combiners are enabled by default. The first Linux
launcher omitted that switch, preventing GPU vertex execution in
`linux_verify04` despite the requested GPU settings. The `linux_verify05`
supervisor was terminated by the host before it wrote its completion record;
the still-running child was stopped during recovery. Its metadata records
this interruption separately from a guest crash or a normal probe deadline.

The Linux texture cache now validates every byte on every use with bounded
raw snapshots (32 MiB default), falling back to full-span XXH3 hashes. GPU
readback checks pass same-frame mutations, tails, snapshot eviction and refresh,
as well as the hash-only fallback. Redundant GL bindings and uniform lookup
work have been reduced. The optimized `linux_opt07` live-play sample (frames
15960–16799, seven capture frames excluded) averages **9.62 FPS**, with a
**5.37 FPS 1% low**, median 100.11 ms and p95 138.39 ms. The window contains
833 measured frames / 86.57 seconds; captures show Detroit vs Columbus in the
first period. It ran uncapped with GPU verification off and no compiler active.
Prior test cache files were reused. This is renderer presentation timing, not
display scanout or full-match coverage. Evidence:
`analysis/linux_opt07.gameplay_summary.json`, `.frames.csv` and `.process.jsonl`.
Texture validation averages 25.50 ms per frame, total backend CPU time 56.40 ms
and decoded-texture work 4.64 ms (included in the backend total). Other guest
work also needs profiling. The current Linux performance target is far from met.

Source generation completed: 19,928 translated functions, zero failures,
2,391,211 generated lines and 20 unresolved target stubs. Linux traps on
unimplemented instructions and non-RET unresolved code instead of silently
returning. The pinned toolkit and vendored debug kit remain unchanged.
See [docs/LINUX.md](docs/LINUX.md) for setup, launcher and probe commands.
The Windows results below remain Windows-only evidence.

## Validation and remaining work

The optimized `linux_opt07` probe was stopped through its stop file after
637.17 seconds (child terminated with SIGTERM, as requested by the supervisor).
This is an intentional test stop, not a recorded guest crash. The measured
window and its limitations are described above. Audio underruns occurred;
audio quality, end-to-end save persistence, complete matches and consecutive
matches still require validation. Windows has not been rebuilt on this Linux
host after the portability changes. There is no Linux installer or native
settings overlay in this checkpoint.

Checks rerun before publishing this branch: all seven CTests, Python
byte-compilation of the three Linux probe/reporting scripts, shell syntax and
Git whitespace checks pass. The previous Radeon checks also pass texture
mutation/eviction, hash fallback, cached state and rendering after presentation.
The latest host-source static analysis reports no warnings. Game files, generated
guest C, binaries, logs, captures and RAM dumps remain local and ignored.

Next work when development resumes:

1. Profile the main guest/render thread and the NV2A acknowledgement thread
   during live play to identify the remaining CPU costs.
2. Reduce repeated validation of unchanged textures while preserving detection
   of writes through every guest alias and file-I/O path. No write-tracking
   optimization has been implemented yet. Evaluate indexed GPU vertex reuse
   and upload/submission costs against independent CPU/GPU checks.
3. Repeat uncapped, visually verified gameplay measurements without a compiler
   or verification overhead. Then validate a complete match at steady 60 FPS,
   consecutive matches, input, audio and saved settings.

Build with `bash play.sh --build-only`; launch with `bash play.sh`. Local
regressions: `ctest --test-dir native/build-linux --output-on-failure`. GPU
regressions: build and run `hitz_gl_backend_test` in a graphical session.
See [docs/LINUX.md](docs/LINUX.md) for dependencies and probe usage.
