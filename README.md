# NHL Hitz 2003 — Static Recompilation for Windows and Linux

A native Windows port of the original Xbox **NHL Hitz 2003 (USA)**. The game's x86 executable (`default.xbe`) is
statically recompiled to C and runs on a Windows host runtime: D3D11 graphics, XAudio2 sound, XInput and keyboard.

Work in progress. A prebuilt installer is on the [releases page](https://github.com/dalves408/nhlhitz2003recomp/releases).

**No game data is included.** You need your own legally owned copy of the game (Title ID `0x4D57000B`). The ISO, its
extracted files, the XBE and the C generated from it are never committed.

## Native Linux development

The Linux development player uses SDL2/OpenGL and reaches live Exhibition
gameplay. On the current AMD A6-7310 / Radeon R4 host, the first optimized
gameplay sample averages **9.62 FPS**; a full match and steady 60 FPS remain
unvalidated. See [STATUS.md](STATUS.md) for the checkpoint and remaining work,
and [docs/LINUX.md](docs/LINUX.md) for setup and verification. After dependencies,
game extraction and source generation, `bash play.sh` builds and launches it.
The Windows installer and settings instructions below are Windows-specific.

## Windows requirements

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
.\build\Release\nhl_hitz_2003_recomp.exe
```

Or double-click `play.bat`, which builds and launches. `play-debug.bat` builds the debug flavor
(`-DHITZ_DEBUGKIT=ON`), which writes crash, hang and mark dumps to `%LOCALAPPDATA%\NHLHitz2003\debug`.

Settings and saves are stored in `%LOCALAPPDATA%\NHLHitz2003`. Press **F1** in game for the settings overlay.

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

Linux DDS replacement loading, bounded asynchronous caching, mip filtering,
the Windows-style FPS/CPU/GPU title, WSL test setup and validation limits are
documented in [Linux texture performance](docs/LINUX_TEXTURE_PERFORMANCE.md).
