# Texture addon installers

The current small standalone addon Setup uses the same hosted texture.bin as the
optional texture-pack task in game Setup. Both prefer a verified BIN beside the
setup EXE and otherwise download/resume it using a shared persistent cache.
The addon detects the installed game folder and lets the user change it. It
creates no Programs and Features entry, shortcuts or uninstaller. Game Setup
retains its normal game registration and shortcuts.

## Install and enable

1. Close the game and Settings. Run
   `NHLHitz2003-HiRes-Textures-1.0.1-Setup.exe`, or select the optional **Install
   the 4x hi-res texture pack** checkbox in game Setup.
2. Select the installed game folder. The addon accepts normal, debug and legacy
   executable names, requires game 1.0.1+, and suggests a registered installation
   or `C:\Games\NHL Hitz 20-03`. Game Setup also supports fresh game installation
   from the user's own disc image or extracted files, as before.
3. A valid texture.bin beside Setup is used offline. If absent, Setup says the
   pack will be downloaded. Interrupted/cancelled transfers keep their progress;
   retry or rerun either installer to resume. A mismatched sibling BIN is kept
   and interactive Setup offers to download the correct copy instead.
   The download view shows percentage, downloaded/total MB, total file size and
   transfer speed. Saved progress counts toward completion, not current speed.
4. Open **config.exe**: choose Render Scale **2x or higher**, enable **Use the
   hi-res textures** on Textures, then Save. Alternatively use **F1** in game:
   choose Render Scale 2x+ and turn **Hi-res textures On**.
5. When finishing a successful installation, choose whether to keep the cached
   texture.bin for repairs/reinstalls. **No** deletes Setup's completed cache on
   exit; installed textures and any BIN supplied beside Setup are preserved.

The payload is **1.847 GB** to download and uses about **2.95 GB** for installed
textures. Keeping the completed download cache uses another 1.85 GB for repairs.
The same icon and wizard artwork as the game installer are used. The addon
includes updated config.exe with the NHL Hitz Pro style master/category controls
and DDS detection, while preserving a newer installed Settings version.

Neither installer changes game settings or saves, increases render scale, or
enables textures automatically. Fresh settings retain 1x, fullscreen, 4:3 and
60 FPS with the other video enhancements off. Rerunning repairs bundled textures
and leaves unrelated custom textures alone. User-folder replacements retain their
existing priority over matching files beside the game executable.

An old registered addon is migrated only when it belongs to this exact selected
game folder: its registration and exact old addon-uninstaller files are removed
after successful extraction. Its uninstaller is never executed. Base game and
other-folder registrations are preserved; an inaccessible old machine-wide entry
is logged without requesting elevation. Texture files are portable data without
a separate uninstall log. Remove their files manually if wanted; game uninstall
may leave texture_pack in place.

## Current build commands

```powershell
python scripts/build_texture_download_installer.py --game-version 1.0.2 --iscc "<path to ISCC.exe>"
python scripts/build_installer.py --version 1.0.2 --iscc "<path to ISCC.exe>"
```

Outputs:

- `native/installer/Output/texture-download/NHLHitz2003-HiRes-Textures-1.0.1-Setup.exe`
- `native/installer/Output/NHLHitz2003-Setup-1.0.2.exe`

The addon is about 2.5 MB; neither installer embeds the BIN or debug kit. Each has
an SHA-256 sidecar. The installers are published separately from the shared BIN, checksum and
metadata. The addon and its payload are on the dedicated
[texture-pack release](https://github.com/dalves408/nhlhitz2003recomp/releases/tag/texture-pack-v1.0.1).
Game 1.0.2 is published as the Latest game release. No downloader is implemented inside
config.exe; it only manages settings.

See [shared payload and resume implementation](SHARED_TEXTURE_PAYLOAD.md) for the
pinned download URL/hash, cache behavior, native transport and regression commands.

## Validation

Passed: real GitHub range resume and full SHA-256; private transport tests for
disconnect, cancel, process restart, parent exit, range validation/fallback,
redirects, ETags and corrupt data; shared wizard cancellation; game task off with
no requests; addon interruption resumed by game Setup; sibling BIN preference;
invalid sibling preservation/fallback; and shared-cache repair.
Keep/delete-on-exit was also checked through both installers. Failed extraction,
interrupted downloads and user-supplied sibling BINs retain their recovery/source
files. Silent installs default to keeping the cache; `/KEEPTEXTUREBIN=0` requests
deletion on success. Extraction failure returns a nonzero installer exit code.

The full new-addon offline test extracted all **6,198 DDS files**, preserving
2,946,015,256 DDS bytes, paths, names and hashes. It installed current config.exe,
preserved settings/custom files/base uninstaller, and left the actual game/addon
registrations unchanged. A full test caught repeated solid-block decompression
with per-file archive entries; both new installers now extract the entire
verified BIN in one pass. Successful full extraction took about 121 seconds on
this host with detailed logging enabled. Source packs remain unchanged.

Local test evidence is under `analysis/texture-payload-upload`. The permanent
transport regression is `native/tests/texture_download_test.py`. New-installer
user testing was completed and the installer behavior was approved for release.

## Earlier monolithic offline builds

The preceding self-contained extractor is preserved under
`native/installer/Output/texture-addon-portable`; it is 1,874,426,650 bytes and
contains the textures plus config.exe. It passed its own full extraction tests.
The earlier registered-style expanded addon is separately preserved under
`native/installer/Output/texture-addon-under-2gb` (1,874,270,579 bytes). Those files
have not been replaced by the new small downloader.

`scripts/build_texture_installer.py` still builds the monolithic variant using
lossless LZMA2/max and a strict default limit below 2,000,000,000 bytes. It validates
and hashes every DDS before/after compilation, includes the release config.exe,
and preserves the preceding good output if an oversized candidate is rejected.
The newer independent BIN is built with `scripts/build_texture_payload.py`.
All texture assets/build outputs stay out of Git source control.
