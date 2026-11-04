# NHL Hitz 2003 — Windows 1.0.2

## Changes since 1.0.1

- Game Setup offers an optional, initially unchecked **4x hi-res texture pack**.
  The small standalone texture addon uses the same hosted payload.
- Both installers first check for `texture.bin` beside Setup. Otherwise they
  download it, or resume saved progress from a shared per-user cache. Downloads
  survive cancellation and installer restarts, and either installer can continue
  a partial transfer started by the other.
- Download progress shows percentage, increasing downloaded/total MB, total
  file size and speed. Saved resume offsets are excluded from the speed sample.
  Pinned file size and SHA-256 are verified before single-pass extraction.
- After success, choose whether to keep the cached BIN for repairs/reinstalls.
  Choosing No deletes the completed cache on exit. User-supplied sibling BINs
  and recovery files from interrupted/failed installs are preserved.
- **config.exe** now has a Textures tab with a master switch and separate Rink,
  Logos, Jerseys, Glow and Other choices. DDS, PNG and BMP packs are detected;
  controls are disabled without a pack or at native 1x scale.
- **F8** toggles installed hi-res textures at 2x–5x scale, saves the choice and
  refreshes textures through the existing F1 settings path. F8 does nothing at
  native 1x scale or without a pack. Holding it does not repeatedly toggle.
- The Windows game prevents idle display-off and automatic sleep while running,
  including controller-only play. Normal power behavior resumes on exit.
- The normal game EXE is **nhl_hitz_2003.exe**. Repairs/upgrades remove the old
  `nhl_hitz_2003_recomp.exe` while preserving separately installed debug builds.
  Developer debug builds use `nhl_hitz_2003-db.exe` with a versioned banner
  contained inside the game window; no debugger is included in this release.

The graphics-memory recovery, sound-effect resampling, persistent-buzz protection
and DDS loading improvements from [1.0.1](RELEASE-1.0.1.md) remain included.

## Install

Download **NHLHitz2003-Setup-1.0.2.exe**. Existing installations update in place
and keep settings, saves and game files. Fresh installations require the user's
own NHL Hitz 2003 Xbox USA disc image or extracted files. The directory is
selectable and defaults to `C:\Games\NHL Hitz 20-03` when no installation exists.
Fresh video defaults remain native 1x, fullscreen, 4:3 and 60 FPS with the other
enhancements off. Existing saved choices remain unchanged.

For textures, select the optional checkbox or run the standalone addon. The
shared BIN downloads **1,846,593,844 bytes** (1.847 GB); installed DDS files use
2,946,015,256 bytes. To use them, open config.exe, choose Render Scale **2x+**,
enable **Use the hi-res textures** on Textures, and Save. Or use **F1** in game.
Installing a pack does not automatically change the user's settings.

The addon adds no Programs and Features entry or uninstaller. The game retains
its normal game uninstaller. The Windows installers contain no debug kit, PDBs,
debug collector or original disc assets. Texture bytes are a separate download,
not a second large copy embedded in either installer.

## Validation

The owner tested and approved the preceding build. Automated checks covered
Settings persistence/defaults, DDS detection, reserved controls, transport ranges,
disconnect/retry, cancellation/resume, process/parent termination, redirects,
checksums, shared-cache access, cross-installer resume, local BIN preference,
keep/delete-on-exit and preservation after failed extraction. Numeric and actual
wizard-label tests checked progress and speed without counting saved offsets.
The native helper also resumed from the real public GitHub asset and verified
the complete pinned hash. Full offline addon extraction matched all 6,198 DDS
paths, sizes and hashes and preserved settings/custom files/registrations.

The release version bump is packaging-only relative to that approved behavior.
No new Linux build or Linux performance claim is included in this release.
