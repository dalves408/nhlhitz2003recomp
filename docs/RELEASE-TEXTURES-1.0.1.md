# NHL Hitz 2003 — 4x texture pack 1.0.1

First stable standalone texture-addon release: **6,198 4x-upscaled DDS textures**
covering rink surfaces, boards, jerseys, logos, player/menu artwork and arena
details. Original texture filenames, category folders and stored mipmaps are
preserved. Download size is **1,846,593,844 bytes** (1.847 GB); installed DDS
storage is **2,946,015,256 bytes**. No original game/disc executable is included.

## Install

Run **NHLHitz2003-HiRes-Textures-1.0.1-Setup.exe** and select the game folder.
It detects existing normal/debug/legacy installations and lets you change the
destination. Game **1.0.1 or newer** is required; **1.0.2 is recommended** and
provides the same pack as an optional game-Setup checkbox.

Setup uses a verified texture.bin beside its EXE when available. Otherwise it
downloads the shared BIN with percentage, size and speed display. Interrupted
downloads are kept and resumed by either installer. After success, choose
whether to keep the cached BIN for repairs/reinstalls or delete it on exit.
A BIN supplied beside Setup is never removed.

Enable the installed pack in **config.exe**: choose Render Scale **2x+**, switch
on **Use the hi-res textures** in Textures, and Save. Or use the game's **F1**
settings. The included Settings utility is version 1.0.2; newer installed versions
are preserved. User settings and saves are not reset.

The addon is extraction-only, with no Windows application registration or
uninstaller. Rerun it to repair bundled files; unrelated custom files are kept.
An older registered addon in this same game folder is migrated without running
its uninstaller or affecting the base game registration.

## Shared payload

Both installers use the existing, unchanged asset:

<https://github.com/dalves408/nhlhitz2003recomp/releases/download/texture-pack-v1.0.1/texture.bin>

SHA-256: `39ade92c2439c82e4294d3cb4ee004f30e94b2642aee6e5ad4d0b2bed177173b`.

The BIN passed full extraction and all DDS path/size/hash checks. Resume,
cancellation, cache cleanup and preservation tests passed, and the owner approved
the installer behavior. This data release tag identifies the existing payload;
the new installer implementation is in the game **v1.0.2** source release.
