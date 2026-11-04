# Shared texture.bin and download installers

The standalone texture addon and the optional game-Setup texture task now use
one hosted payload. The addon remains extraction-only: no Windows Programs and
Features entry and no uninstaller. The game uses its existing game registration.
Both preserve settings and unrelated custom textures.

The BIN is published on the separate
[Hi-Res Texture Pack 1.0.1 release](https://github.com/dalves408/nhlhitz2003recomp/releases/tag/texture-pack-v1.0.1).
Game 1.0.2 is the Latest game release. The standalone addon installer, shared BIN,
checksum and payload manifest are published together on the texture-pack release.

## Pinned payload

- URL: <https://github.com/dalves408/nhlhitz2003recomp/releases/download/texture-pack-v1.0.1/texture.bin>
- Size: **1,846,593,844 bytes** (1.847 GB), 153,406,156 bytes below 2 GB.
- SHA-256: `39ade92c2439c82e4294d3cb4ee004f30e94b2642aee6e5ad4d0b2bed177173b`.
- Pack version: 1.0.1; minimum game version: 1.0.1.
- Contents: **6,198 DDS files**, 2,946,015,256 texture bytes, plus three metadata
  and instruction files. All DDS names and category folders are unchanged.
- Archive: standard 7z/LZMA2 with a `.bin` extension, 32 MiB dictionary,
  twelve solid blocks limited to 256 MiB. No game/config EXE or debugger inside.

The archive has one root, `texture_pack`, containing `rink`, `logos`, `jerseys`,
`glow`, `other`, `manifest.txt`, `pack-info.json` and short `README.txt`.
It is independent of both installer EXEs and is not a coupled Inno disk slice.
The original source packs and preceding monolithic installers are preserved.

## Local detection and persistent resume

Both installers use `native/installer/texture_download_code.iss` and the native
`nhl_texture_download.exe` helper, extracted temporarily rather than installed.
Their pinned URL, size, hash and archive size come from `texture_payload.iss`.

Lookup order:

1. Verified `texture.bin` beside the setup EXE.
2. Verified completed download cache.
3. Partial matching cache, resumed from its saved byte length.
4. New download.

Both installers show the file's total size, percentage, downloaded/total MB and
current transfer speed while downloading. For example:
`48.8% - 902.0 / 1846.6 MB - 2.0 MB/s`, with the 1.85 GB total in the headline.
Speed uses roughly one-second samples and excludes saved bytes when resuming.
Stalls report zero throughput, reconnect/restart changes reset the sample, and
checksum verification is labeled separately without a misleading network speed.

A missing local BIN is normal: the wizard says "The texture pack will be
downloaded" and proceeds. An invalid sibling BIN is preserved. Interactive Setup
offers to download the matching copy instead; silent Setup falls back to its
verified cache/download without waiting for a prompt.

Cache: `%LOCALAPPDATA%\NHLHitz2003\Downloads\<expected SHA-256>\`.
Partial data is `texture.bin.part`; a strong server ETag is saved separately.
The hash-keyed directory prevents mixing pack versions. A named mutex serializes
writers, so a partial download started by the addon can be continued by game
Setup, or vice versa. Completed cache files are verified before reuse.

After successful installation, both interactive installers ask whether to keep
the downloaded texture.bin for future repairs or reinstalls. Yes retains the
cache (about 1.85 GB); No removes only that completed cache file as Setup exits.
A BIN supplied beside Setup is never deleted. Failed or cancelled downloads and
extractions retain their recovery files and do not offer this cleanup choice.
If another process has the cache open, deletion failure is logged and the file
is left in place. Silent installs keep the cache by default; `/KEEPTEXTUREBIN=0`
requests deletion after success, while `/KEEPTEXTUREBIN=1` explicitly retains it.
Texture extraction failures now return a nonzero Setup exit code even if Inno
otherwise treats a post-install script error as a completed installation.

Cancellation stops the transfer and keeps useful progress. Retry continues it;
Cancel again closes Setup. Relaunching either installer can resume the cache.
The helper also stops when the parent installer exits unexpectedly. Files are
persistent across Windows restarts; tests simulate interruption by terminating
processes rather than rebooting the user's computer.

The helper uses HTTPS, follows refreshed redirects from the original versioned
GitHub URL, checks HTTP range offsets and total size, and uses If-Range for a
saved strong ETag. A server returning a full response instead of a requested
range triggers a safe restart, never an append. Transient network failures
retry with backoff. Final size and SHA-256 must both match before extraction.
Corrupt completed downloads are rejected; cache repair never modifies a sibling
BIN supplied by the user. Resume requires a server that honors byte ranges;
this was verified against the actual public GitHub asset.

## Single-pass extraction

The verified archive is streamed once with `ExtractArchive`, using
`ArchiveExtraction=enhanced/nopassword` and `MapArchiveExtensions('.bin', '.7z')`.
Do not replace this with per-file `[Files] external extractarchive` entries for
this solid payload: that path repeatedly decompresses solid blocks and was very
slow in a full-pack test. The successful single-pass test extracted all textures
in about 121 seconds on this host with detailed installer logging enabled.
This is an observed test duration, not a general performance guarantee.

Downloads are prepared before program installation. Free space is checked after
download, accounting for the textures and pending game/disc files. Extraction
runs after program installation (and successful game-data import for fresh
installs). Cancelling extraction keeps the verified BIN; rerunning Setup repairs
bundled files. Texture files remain portable data and have no separate uninstall
log; game uninstall may leave the texture_pack directory in place.

Game Setup has an unchecked optional `hires` task. Leaving it off neither starts
the helper nor downloads/extracts the pack. Installing the pack does not change
render scale, enable textures automatically, or overwrite settings. The short
finish instructions explain config.exe / F1 and the requirement for 2x+ scale.

## Build

```powershell
python scripts/build_texture_download_installer.py --game-version 1.0.2 --iscc "<path to ISCC.exe>"
python scripts/build_installer.py --version 1.0.2 --iscc "<path to ISCC.exe>"
```

The first builds the small addon under `native/installer/Output/texture-download`.
The second builds the normal game release installer under `native/installer/Output`.
Neither bundles the BIN or debug kit. The addon includes the updated config.exe;
its version-aware replacement preserves newer Settings versions. Both retain
existing icons, game-folder detection, legacy EXE migration and settings defaults.

To rebuild the data payload itself:

```powershell
python scripts/build_texture_payload.py --expected-manifest analysis/texture-portable-build/manifest.txt
```

Requires Python 3.12+ and 7-Zip. The builder snapshots and hashes every DDS,
compresses with two threads, tests the archive, fully extracts it, verifies every
path/size/SHA-256 and rechecks that sources did not change. Only candidates below
2,000,000,000 bytes replace the preceding verified output. Output contains
texture.bin, texture.bin.sha256 and texture-payload.json. A newly built manifest
starts with a null URL until its versioned release asset has been published.
Never silently replace an existing published payload under an unchanged hash.

## Validation

The permanent transport regression uses a private local server and private cache:

```powershell
cmake --build native/build-release --config Release --target nhl_texture_download
python native/tests/texture_download_test.py
```

Passed: ranges, ignored/malformed ranges, dropped connections, refreshed
redirects, changed ETags, corrupted hashes, invalid caches, local BIN preference,
forced termination/restart, parent-installer exit, cancellation/resume, shared
cache locking, and Unicode/space paths. A real GitHub test resumed at byte
1,846,528,308, downloaded only the last 64 KiB, and verified the entire BIN hash.

Isolated installer regressions passed with private registry/shortcut namespaces:
optional task off makes no request; failed addon download preserves its partial
cache without extraction; game Setup resumes that same cache; verified cache
repair; valid sibling BIN preference; invalid sibling preservation/fallback;
settings/custom textures/base uninstaller preservation; and no addon registration.
A shared wizard cancellation test stopped the child through its real progress
callback, retained partial bytes, and resumed them successfully.

Cache-retention regressions additionally cover keep/delete-on-exit through both
installers, preservation after interrupted downloads and failed extraction,
protection of sibling BIN files, and no cleanup when the optional task is off.
Statistics checks cover resumed-byte exclusion, rate calculation, stalls,
restart/clock wrap, byte units, percentage limits and actual wizard label text.

The full new-addon offline test passed all 6,198 extracted DDS path/size/hash
checks, current config.exe matching, unchanged actual game/addon registrations,
and preservation of settings, custom files and base uninstaller.
Local evidence: `analysis/texture-payload-upload`; final artifacts have independent
SHA-256 sidecars. The owner tested and approved the installer behavior for publication.

References: [Inno archive extraction](https://jrsoftware.org/is6help/topic_setup_archiveextraction.htm),
[ExtractArchive](https://jrsoftware.org/ishelp/topic_isxfunc_extractarchive.htm),
[solid-archive caveat](https://jrsoftware.org/files/is6.5-whatsnew.htm), and
[HTTP ranges](https://www.rfc-editor.org/rfc/rfc9110.html#name-range-requests).
