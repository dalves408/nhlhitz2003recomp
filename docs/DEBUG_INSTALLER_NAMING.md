# Windows executable names and debug installer

Windows CMake builds now select their output name from `HITZ_DEBUGKIT`:

| Build | Executable | Matching symbols |
|---|---|---|
| Normal | `nhl_hitz_2003.exe` | No PDB in the public installer |
| Debug kit | `nhl_hitz_2003-db.exe` | `nhl_hitz_2003-db.pdb`, retained with the development build |

The CMake target remains `nhl_hitz_2003_recomp`, so build commands do not change.
PE OriginalFilename/InternalName, launchers and installer shortcuts use the
new names. The debug startup banner and log include the configured
`HITZ_PACKAGE_VERSION`, for example `NHL Hitz 2003 v1.0.2 (debug)`.

Debug banners are clipped child windows of the game client area. Startup is
queued until the renderer creates its window. The popup follows movement,
resizing, DPI/fullscreen changes and minimization through normal child-window
behavior, with no polling thread or desktop-topmost window. Expiry and parent
destruction remove the popup. It takes no focus or mouse input.

The vendored xrdbg source stays unchanged. CMake generates an adapted copy
that replaces only its banner implementation with `hitz_debug_banner.inc`;
it fails configuration if the expected upstream section markers change.

## Installation and migration

```powershell
python scripts/build_installer.py --version 1.0.2 --debug
```

The debug installer contains the `-db.exe` and debug collector, with shortcuts
to the debug program. The normal installer contains only the normal EXE.
Installing either flavor preserves the other newly named EXE and creates
shortcuts only for flavors present in the folder. Both use the existing
settings/saves and game installation. The shared game uninstaller manages the
program files installed by both flavors.

Future repairs/updates delete `nhl_hitz_2003_recomp.exe` in the selected install
folder. Existing game data and settings stay intact. When migrating from a
legacy installation, only the selected flavor is newly installed; install
the normal package as well if both launch options are wanted.

The standalone texture add-on accepts the new normal/debug names and the
legacy name, retaining compatibility with the original published 1.0.1.
No downloader integration is included.

## Validation

- Normal/debug builds produce distinct EXEs and correct PE filename metadata.
- An isolated installer identity verifies upgrade and repair cleanup,
  coexistence, valid shortcut targets and unchanged game/settings sentinels.
  Real user registration and shortcuts are preserved.
- `hitz_debug_banner_test` verifies queued startup, child-window containment,
  movement, resize/fullscreen/tiny-window geometry, replacement, expiry and
  parent destruction.
- An isolated debug startup run verifies the renamed process, versioned
  banner/log, continued presentation and expected GPU selection.

Existing published game release assets are not replaced by these local builds.
