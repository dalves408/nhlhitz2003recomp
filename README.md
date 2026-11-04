# NHL Hitz 2003 — Static Recompilation for Windows

A native Windows port of the original Xbox **NHL Hitz 2003 (USA)**. The game's x86 executable (`default.xbe`) is
statically recompiled to C and runs on a Windows host runtime: D3D11 graphics, XAudio2 sound, XInput and keyboard.

Work in progress. A prebuilt installer is on the [releases page](https://github.com/dalves408/nhlhitz2003recomp/releases).

Windows **1.0.2** adds an optional hi-res texture download in Setup, resumable
downloads with size/speed statistics, a Textures tab in Settings, F8 texture
toggling at 2x+ scale, and Windows idle display/sleep prevention. See the
[1.0.2 release notes](docs/RELEASE-1.0.2.md). The public installer contains no
debug kit or original game data and preserves settings and saves. The separate
[4x texture addon](https://github.com/dalves408/nhlhitz2003recomp/releases/tag/texture-pack-v1.0.1)
uses the same shared download as game Setup.

**No game data is included.** You need your own legally owned copy of the game (Title ID `0x4D57000B`). The ISO, its
extracted files, the XBE and the C generated from it are never committed.

## Requirements

- Windows 10/11 x64, a D3D11 GPU
- Visual Studio 2022 (or Build Tools) with the C++ workload
- CMake 3.20+
- Python 3.12 with `capstone` 5.0.7 (`pip install capstone==5.0.7`)

## Build

### 1. Clone with the toolkit submodule

```powershell
git clone --recurse-submodules https://github.com/dalves408/nhlhitz2003recomp.git
cd nhlhitz2003recomp
```

The recompiler and runtime are [sp00nznet/xboxrecomp](https://github.com/sp00nznet/xboxrecomp), pinned in
`tools/xboxrecomp`.

### 2. Provide the game files

```powershell
extract-xiso -x "NHL Hitz 2003 (USA).iso" -d extracted
mkdir game_files; copy extracted\default.xbe game_files\
cmd /c mklink /J native\game extracted
```

### 3. Recompile the XBE to C

```powershell
python scripts/prepare.py
```

This writes the generated C into `native/src/recomp/gen/`. It takes about 15 minutes. Use
`--from-stage disasm|identify|abi|recompile` to resume a later stage.

### 4. Build and run

```powershell
cmake -S native -B native/build -G "Visual Studio 17 2022" -A x64
cmake --build native/build --config Release --target nhl_hitz_2003_recomp --parallel 6
cd native
.\build\Release\nhl_hitz_2003.exe
```

Or double-click `play.bat`, which builds and launches. `play-debug.bat` builds the debug flavor
(`-DHITZ_DEBUGKIT=ON`), which writes crash, hang and mark dumps to `%LOCALAPPDATA%\NHLHitz2003\debug`.

Windows builds use `nhl_hitz_2003.exe` for normal play and `nhl_hitz_2003-db.exe`
for the debug kit. Debug banners show the configured version and stay inside
the game window. Updated installers remove the legacy
`nhl_hitz_2003_recomp.exe` during repair/update and preserve the other new EXE
when it is already installed. See [debug installer behavior](docs/DEBUG_INSTALLER_NAMING.md).

Settings and saves are stored in `%LOCALAPPDATA%\NHLHitz2003`. Press **F1** in game for the settings overlay.
Press **F8** to toggle an installed hi-res texture pack at Render Scale **2x or higher**.
At native 1x scale F8 is ignored. The toggle saves the same setting as the F1 menu.
While running, the Windows game prevents idle display-off and automatic sleep,
including during controller-only play. Normal power behavior resumes on exit.

## Controls

An Xbox/XInput controller works directly. Keyboard:

| Key | Xbox |
|---|---|
| Enter / Backspace | Start / Back |
| Z / X / C / V | A / B / X / Y |
| Arrows | D-pad |
| W A S D / I J K L | Left / right stick |
| Q / E | White / Black |
| 1 / 3 | Left / right trigger |

## Credits and licenses

- [xboxrecomp](https://github.com/sp00nznet/xboxrecomp) by sp00nznet (MIT; the APU and NV2A components derive from
  [xemu](https://github.com/xemu-project/xemu), LGPL-2.1+). Files in `native/overrides/` are modified copies of
  toolkit files and keep their licenses.
- NHL Hitz 2003 is © Midway. This project is not affiliated with or endorsed by Midway, the NHL or Microsoft, and it
  distributes no game code or assets.

Windows replacement-pack DDS support, caching, asynchronous loading, controls
and measured limitations are documented in
[Windows texture performance](docs/WINDOWS_TEXTURE_PERFORMANCE.md).

The graphics-pool exhaustion fix and timestamped runtime video-setting logs
are described in [Windows memory recovery](docs/WINDOWS_MEMORY_RECOVERY.md).

The optional DDS pack is available through the
[standalone texture add-on installer](docs/STANDALONE_TEXTURE_INSTALLER.md).
It detects the installed game folder and extracts without an uninstaller or
Windows application registration. The included config.exe has a Textures tab
for the master switch and texture categories. The 1.0.2 game installer
has an optional texture-pack checkbox. Both installers prefer a verified
texture.bin beside Setup and otherwise share a resumable download cache.
After success, Setup asks whether to keep its cached BIN for repairs/reinstalls
or delete it on exit. User-supplied sibling BINs are preserved.
The [shared payload and installer notes](docs/SHARED_TEXTURE_PAYLOAD.md) include
the published BIN URL, checksum, build commands and validation results.
