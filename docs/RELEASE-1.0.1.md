# NHL Hitz 2003 — Windows 1.0.1

## Changes since 1.0

- Recover graphics-pool space when allocations would otherwise fail. Blocks
  freed by the game remain intact until pressure, then become reusable before
  retrying the request. This addresses the ignored-frees path behind reported
  pool exhaustion after Exhibition and while loading Three in a Row.
- Replace linear voice resampling with a 16-tap windowed-sinc filter to reduce
  harsh high-frequency artifacts in converted effects. Double-precision phase
  keeps output consistent across streaming chunk sizes.
- Protect playing voices from physical-page alias changes. A later heap-page
  registration cannot redirect a looping graphics-memory sample buffer to game
  data. Choices reset on voice restart, stop and APU reset. Page-boundary reads
  resolve each page independently; control structures keep their existing lookup.
- Add DDS replacement textures with stored mipmaps, content-keyed caching,
  background reads/decodes and worker-created DDS textures. These reduce repeated
  loading/upload work. Existing PNG/BMP packs continue to work. Hi-res textures
  require a separately installed pack and render scale of at least 2x.
- Record video-setting changes with timestamps, frame counts, old/new values,
  requested/applied render settings, frame cap, fullscreen, filtering and pack
  state. Debug sessions can now distinguish startup settings from later changes.

## Installation

Download `NHLHitz2003-Setup-1.0.1.exe`. Existing installations update in place
and retain their settings and saves. New installations default to
`C:\Games\NHL Hitz 20-03`, with a selectable directory, 1x native rendering,
fullscreen, 4:3 output and a 60 FPS cap. VSync, MSAA, anisotropic filtering and
hi-res textures default off. Existing saved choices are preserved.

This is the normal Windows x64 player: no debug kit, PDB, debug collector,
game assets or texture pack are bundled. Your own NHL Hitz 2003 USA Xbox game
files are required. The hi-res pack is optional and is not downloaded by Setup.

## Validation and limits

The actual-runtime graphics allocator passes pressure/retry, delayed reuse,
duplicate-free, live-block, alignment/range and repeated-reuse regressions.
Audio tests cover page collisions, independent voices, restart/reset, cross-page
PCM/stream reads, resampling spectra, chunk continuity, stereo, DC and underruns.
NIS animation lifetime regression passes. Normal and debug development builds
compile; the published installer is built separately with the debug kit off.
Runtime scale/fullscreen change logging was verified through the game UI.
The exact public player also passed a separate 35-second startup check with
saved settings preserved, the expected rendering adapter, and no debug folder.
The installer compiler lists only the game, settings program and temporary
importer as payload inputs; the public game has no xrdbg or PDB reference.

The resampler's five-minute scripted run reached gameplay and ended at its
intentional watchdog deadline. Its audio telemetry recorded one 13.4 ms host
starvation interval; the change is not a claim that all audible hitches are
eliminated. The page protection has dedicated collision tests; repeated team
switches and the original full Exhibition → Three in a Row sequence still need
validation on the reporting laptop with the complete update.

A second five-minute scripted run with both audio fixes reached live
Toronto–Chicago play and reported zero host-starvation intervals, zero refused
audio buffers and no graphics-pool exhaustion. Both runs ended at their
intentional watchdog deadline. These short runs do not replace full-match and
repeated mode-transition testing.

DDS first-period tests previously recorded fewer texture-creation stalls, but
different matchups prevent an exact performance-gain claim. Full-match stability
across every mode and a steady 60 FPS on all hardware remain open. This release
does not include the separate Linux development branch.
