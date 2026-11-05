# Linux test installer

Uses the same `gui(backend)` wizard as the prototype under `../linux-demo`.
`ProductionBackend` replaces all demo operations: real ISO/XISO or folder
validation/import, staged game installation, application/settings launchers,
shared texture download/extraction and user settings initialization.

Current test source: GitHub `origin/codex/linux-native`, commit
`7b923cc10900b278e60b906c7094622177ae9220`; toolkit
`f0f8a461496a4f23d9b0aeaa284b8d70ba7ab82b` plus the branch's Linux patch.
The old dirty Windows/Linux checkouts are preserved. Build snapshot is
`/home/hitz/nhlhitz2003-release-test` in WSL `NHLHitz-Linux`.

The only packaging adjustment to the game source changes `HITZ_SHADER_DIR`
from the absolute build directory to `src/shaders`. `play.sh` sets the matching
installed working directory and passes the imported `game_files` explicitly.
Generated translation uses the current pinned toolkit and branch hooks with
the existing same-XBE disassembly (all current manual seeds covered). The
Windows input path in cached analysis is normalized after checking the exact
supported XBE SHA-256. Provenance is recorded under `analysis/linux-test-release`.

Build on Linux after compiling the isolated source in Release with guest O2:

```sh
python3 /mnt/c/Recomp/NHLHitz03/scripts/build_linux_test_installer.py \
  --source /home/hitz/nhlhitz2003-release-test \
  --project /mnt/c/Recomp/NHLHitz03 \
  --output /mnt/c/Recomp/NHLHitz03/native/installer/Output/linux-test-release
```

The `.run` contains the stripped player, shaders, settings app, wizard and
metadata. No original game data, ISO, sample disc, demo texture data, debug kit,
symbol archive, or real texture payload is distributed. Runtime dependencies
remain system packages: Python/Tk, SDL2, libepoxy, libpng16, and optional 7-Zip
for textures. The Ubuntu 24.04 build requires glibc 2.39 or newer conservatively.
Debian 13 and Fedora 41+ are targets; only Ubuntu/WSL validation is currently
available. Do not claim certification on untested distributions.

Choose a new or existing NHL Hitz 2003 folder. Setup verifies installed
`game_files` and skips the disc selection and import when they are valid.
Existing nonempty `texture_pack` folders are kept without downloading or
extracting textures. If game data is missing or fails validation, supply a
supported disc image or extracted folder. Setup stages the import before
publishing it and backs up incomplete game data before replacing it.
Setup replaces program files from the package and preserves unrelated files.
New settings default to 1x, fullscreen, 4:3, 60 FPS and enhancements off. Existing
settings/saves are preserved under the game's normal XDG data path.

Game and Settings menu entries are selected by default. User-writable logs and
shader prewarm files stay in XDG data/cache directories. Uninstall removes only
unchanged program files and recorded shortcuts, retaining imported game data,
textures, user settings/saves and modified files.

The pinned Windows `texture.bin` is the only texture asset used. Setup first
checks beside its `.run`, then the SHA-specific XDG cache, then downloads from
the existing GitHub URL. Range downloads resume to `texture.bin.part`; a server
ignoring Range restarts safely. Size and SHA-256 are checked before extraction;
the extracted manifest/count/file sizes are checked afterward. Progress shows
percentage, received/total GB and MB/s. Cancellation retains cache and discards
staged install files. Successful GUI install asks whether to retain downloaded
`texture.bin`; choosing No deletes only the downloaded cache on setup exit,
never a copy beside the installer. Noninteractive installs retain cache.

Test command (system Python/Tk must be installed; no display needed):

```sh
sh NHLHitz2003-Setup-1.0.2-linux-test3-x86_64.run \
  --install "$HOME/Games/Hitz Test" --source /path/to/game.iso \
  --no-textures --no-shortcuts
```

Bounded HTTP fixture tests are in `analysis/linux-test-release/test_backend.py`.
Build/test logs and final artifact identity belong in that same evidence folder.
No GitHub release or upload is performed by the builder.

## Menu integration fix (test2)

After a Nobara report, shortcut handling moved into `menu_shortcuts.py`.
Empty/relative XDG data/cache locations now fall back to user defaults instead
of writing an invisible `applications` directory in the setup working folder.
Desktop entries are executable and use correctly escaped `/bin/sh` Exec
arguments. After creation, update-desktop-database and available KDE 6/5 cache
refresh tools run with the normalized data directory. Setup refuses sudo
sessions that would put entries into root's menu instead of the desktop user's.
The user's precise Nobara desktop/environment is not yet confirmed.

`install-shortcuts.py` is installed with the player. A standalone identical
repair helper is published locally as `NHLHitz2003-Repair-Menu.py`; it opens a
folder chooser when run without arguments. Repair replaces the two menu
entries, backs up different old entries, refreshes caches, and updates the
uninstall record. No ISO, texture extraction or reinstall is needed.

Test2 keeps exactly the same tested player binary as test1. Menu tests validate
entries with desktop-file-validate, check GIO visibility, actually launch both
entries from paths with spaces/special characters, verify empty/relative XDG
fallback, and exercise KDE refresh invocation, repair and the sudo guard.
Packaged backend integration uses a tiny private synthetic-disc manifest, never
included in the real installer. Actual visual Nobara/KDE menu testing remains
with the user. Evidence: `analysis/linux-menu-fix`.

References: [Desktop Entry Specification](https://specifications.freedesktop.org/desktop-entry/latest-single/)
and [Desktop Menu install locations](https://specifications.freedesktop.org/menu/latest/locations.html).
