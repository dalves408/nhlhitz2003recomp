# NHL Hitz 2003 — native Linux port

Status of the native Linux (OpenGL) build on branch `codex/linux-native`. The game plays
full matches at the 60 FPS cap on a 2013 laptop (i7-4610M, Radeon HD 8600M, Mesa radeonsi).

## Build

```sh
scripts/install_linux_deps.sh            # gcc, cmake, ninja, SDL2, epoxy, libpng, zlib
python3 scripts/prepare.py               # regenerates native/src/recomp/gen from your own disc
cmake -S native -B native/build-linux -G Ninja -DCMAKE_BUILD_TYPE=Release -DHITZ_GUEST_OPT_LEVEL=2
cmake --build native/build-linux && (cd native/build-linux && ctest)
linux/install_shortcuts.sh               # "NHL Hitz 2003" + "NHL Hitz 2003 Settings" menu entries
```

### Toolkit patch

`tools/xboxrecomp` is the upstream toolkit (pinned at v0.16.0, `f0f8a46`). The Linux fixes to it
(output mute for the pause overlays, APU mixdown fixes, XInput rumble) are kept in
`patches/xboxrecomp-linux.patch`. Configuring the Linux build applies it automatically when it is
not already applied (`git apply --reverse --check` first, so re-configuring is a no-op). It fails
loudly if the toolkit is at a different commit; `-DHITZ_APPLY_TOOLKIT_PATCH=OFF` skips it.
To undo: `git -C tools/xboxrecomp apply -R ../../patches/xboxrecomp-linux.patch`.

## Running

`linux/play.sh` starts the game with the tuned settings below; output goes to `analysis/play.log`.
`linux/play_debug.sh` is a developer variant with rumble/USB logging (not a release shortcut).
`linux/nhl_config.py` is the settings app (Video, Controls key mapper, Textures); it edits the same
`~/.local/share/nhlhitz2003/settings.ini` / `keymap.ini` as the in-game F1 menu.

## What changed versus the first bring-up

**Performance** (opt-in env toggles; `linux/play.sh` sets the kept ones). Laptop medians, 1x, hi-res off:

- Baseline: 55.3 FPS.
- Streamed + indexed vertices with texture page watch (`RECOMP_GL_VERTEX_UPLOAD=stream`,
  `RECOMP_GL_VERTEX_INDEXED=1`, `RECOMP_GL_TEXTURE_WATCH=1`): 62.0 FPS uncapped.
- Push-buffer idle wait (`RECOMP_PB_IDLE_WAIT_US`): render thread 93% → 81% CPU.
- Shader prewarm (`RECOMP_GL_SHADER_PREWARM`): first-entry compile frames 14–17 → 1–3.
- Texture slot index (`RECOMP_GL_TEXTURE_INDEX=1`): 59.0 FPS, p95 20.65 ms at the 60 cap.
- Hi-res pack at 2x: median 53 FPS.
- Rejected (left off by default): texture workers, render CPU affinity, cap vsync, decoded-texture reuse.
- Diagnostics: `RECOMP_FRAME_PROFILE=<csv>`.
See `linux/play.sh` for the exact toggle set.
**Overlays and settings** (match the Windows build):
- Esc "quit?" prompt and F1 settings overlay; settings saved to settings.ini immediately.
- Render Scale (1x–4x), MSAA (off/2/4/8, per-target multisampled FBOs) and hi-res textures all apply live.
  No restart is ever needed. Hi-res textures need 2x or above: at 1x the row reads "Off (Needs 2x+)"
  and is greyed out (the settings app's Textures tab matches).
- While an overlay is open the guest clock is paused and audio is muted (silence is still fed so APU
  state is untouched), as on Windows.

**Audio**: the stub APU mixdown summed the I3DL2 reverb send (mix bin 10) into the dry mix, so 3D voices
(skate loops) were doubled into the left channel; it no longer does.
3D voices now also take their HRTF submix bin's headroom. Optional Windows APU voice processor on Linux: `-DHITZ_LINUX_APU_VP=ON`.

**Rumble**: the guest's SET_REPORT buffer lives in the image .data section, which ohci.c read from the
contiguous-memory window (all zeros), so motors never started; it now falls back to the image copy; minimum pulse `RECOMP_RUMBLE_MIN_MS` (250);
`RECOMP_RUMBLE_LOG=1` logging.

**MSAA**: the GL backend had no anti-aliasing (the menu said "not available"). Every render target now
has multisampled colour+depth resolved on texture use/blit/flip, off/2x/4x/8x, applied live.

**Keyboard**: Controls key mapper (keymap.ini, same format as Windows config.exe) read on Linux.

## Hi-res texture pack

Supported at render scale 2x+ from `~/.local/share/nhlhitz2003/texture_pack`. The pack itself and
its installer are not part of this repository.

## Verification

CTest suite plus `hitz_gl_backend_test` (GPU combiners, render targets, MSAA, texture watch/eviction).
Live scale 1x→2x→1x, hi-res toggling, overlay pause (game clock frozen) and mute (silent capture)
were checked with screenshots and logs on the laptop.
## Windows follow-ups

- **Rumble SET_REPORT**: the same zero-buffer read affects Windows; the fix is in the shared
  `native/overrides/ohci.c`, so it needs a Windows rumble test before a Windows release.
- **Audio, I3DL2 send mixed dry** (mix bin 10 doubled into the left channel) and **3D submix
  headroom**: the same toolkit APU mixdown is used on Windows. The fixes are only applied to the
  Linux toolkit via `patches/xboxrecomp-linux.patch`; port and A/B them on Windows.
- **Perf ports to D3D11**: texture page watch + slot index, push-buffer idle wait (`Sleep(0)` spin
  in `xbox_memory_layout.c`), shader prewarm and the frame profile CSV.