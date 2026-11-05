# NHL Hitz 2003 1.0.3

Windows 1.0.3 and the first public Linux release are available together.
Bring your own **NHL Hitz 2003 Xbox USA** ISO/XISO or extracted game folder.

| Platform | Standalone | RecompLauncher |
| --- | --- | --- |
| Windows x64 | `NHLHitz2003-Setup-1.0.3.exe` | `NHLHitz2003-1.0.3-Windows-x64.zip` |
| Linux x64 | `NHLHitz2003-1.0.3-Standalone.AppImage` | `NHLHitz2003-1.0.3-Linux-x64.tar.gz` |

## Windows

- Fixed in-match Xbox controller rumble. The controller menu updates the live
  vibration setting, and short pulses stay active long enough for the motors
  to spin up. The fix respects Vibration Off.
- Fixed the audio mix so the reverb send no longer doubles dry audio in the
  left channel, and skating, hits, and puck sounds use the intended 3D submix
  level. The earlier resampling and silent-voice protection remain included.
- Setup recognizes a complete copy of the current 4× texture pack and skips
  the redundant download. Existing settings and saves are preserved.
- The standalone installer and SDK use the same verified, staged game-data
  importer.

## Linux

- Native x86-64 AppImage with OpenGL rendering, SDL2 audio and controller input,
  in-game settings, and support for the same optional 4× texture pack.
- First-run setup reuses valid `game_files` and keeps existing `texture_pack`
  contents. When game data is missing, it imports it from your disc or folder.
  The optional texture download supports resume and uses a local `texture.bin`
  beside the AppImage when available.
- Separate **NHL Hitz 2003** and **NHL Hitz 2003 Settings** entries are offered
  by default. Run the AppImage with `--setup` to repair an existing installation
  or add textures later.
- Fresh settings start fullscreen at 1×, 4:3 and a 60 FPS cap. Existing
  display preferences, settings and saves are preserved.

Make the AppImage executable with
`chmod +x NHLHitz2003-1.0.3-Standalone.AppImage` before opening it. Linux
requires x86-64, glibc 2.39 or newer as the supported baseline, SDL2,
libepoxy, libpng16, Python 3, Tk for the setup and settings windows, and an
OpenGL 3.3 driver. Optional texture extraction uses 7-Zip. For systems
without working FUSE, use the AppImage’s `--appimage-extract-and-run` option.

## RecompLauncher

Use the platform archive for SDK-managed setup. Both include a
`recomplauncher.json` manifest, headless game-data validation and import,
direct Play, launcher-editable settings, and the save location. The manifests
pass the SDK schema, and the shipped entry points were tested with a real
disc image. Full testing inside the RecompLauncher client and the optional
Redump badge remain pending.

The installers and archives exclude the debug kit, original game data, and
the separate [shared texture payload](https://github.com/dalves408/nhlhitz2003recomp/releases/tag/texture-pack-v1.0.1).
