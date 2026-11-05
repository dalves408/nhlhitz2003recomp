# RecompLauncher packaging

The Windows SDK ZIP and Linux SDK tarball carry `recomplauncher.json` at
their root. They declare the Xbox USA disc or extracted folder, a headless
prepare command, the Play entry, INI settings, and the save folder. Prepare
validates the supported `default.xbe` hash and required game files. The
optional Redump verification badge needs independently sourced dump metadata.

On Linux, the SDK tarball contains the same AppImage offered for standalone
use. Keep the tarball as the launcher-selected asset. The upstream launcher
prefers a bare AppImage when both assets have equivalent platform and
architecture labels, and its manifest reader looks beside the installed
executable. In 1.0.3 the archive carries the `Linux-x64` labels and the
standalone asset uses the `Standalone.AppImage` suffix, so the launcher
ranks the SDK archive first.

Make the standalone AppImage executable and open it. It runs setup if valid
game data is missing. To add textures, recreate menu entries or install an
updated AppImage into an existing game folder, run it with `--setup`.
The wizard keeps the existing game and texture reuse, local or resumable
texture download, and separate Play and Settings shortcuts. Setup copies the
AppImage into the selected game folder under a stable name for those shortcuts.
The settings app is also available with `--settings`.

The Linux candidate still uses the same player and system dependencies as
the previous Linux installer, plus an AppImage runtime. Use
`--appimage-extract-and-run` if FUSE is unavailable. The SDK prepare wrapper
uses extraction automatically and leaves texture downloads to standalone
setup.

On Windows, use the normal Inno Setup EXE for standalone installs or the ZIP
for automatic SDK setup. Both use the same importer. The ZIP is an
alternative packaging of the release player, importer and settings tool.
The standalone installer still offers the optional texture pack.

## Build and check

- Windows: `python scripts/build_installer.py --version 1.0.3 --test --sdk`
- Linux: use `scripts/build_linux_test_installer.py` with the usual source,
  project and output arguments plus `--appimagetool` and
  `--appimage-runtime` pointing to the official x86-64 tool and runtime.

For a local SDK check, extract the platform archive and add that folder via
RecompLauncher’s Developer tools. Check the Level 3 setup, import a supported
disc, change a setting and press Play.

[RecompLauncher developer guide](https://github.com/elripalda/RecompLauncher/blob/main/docs/DEVELOPERS.md)
