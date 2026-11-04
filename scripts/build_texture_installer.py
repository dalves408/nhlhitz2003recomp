"""Build the standalone DDS add-on; never alters textures or the game installer."""
import argparse
from collections import Counter
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
CATEGORIES = {'rink', 'logos', 'jerseys', 'glow', 'other'}


def digest(path):
    with path.open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()


def catalogue(source):
    if not source.is_dir():
        raise ValueError(f'Pack directory not found: {source}')
    rows = []
    for p in sorted(source.rglob('*')):
        if p.is_symlink() or p.is_junction():
            raise ValueError(f'Pack links are not supported: {p}')
        if not p.is_file():
            continue
        rel = p.relative_to(source)
        if p.resolve().is_relative_to(source) is False:
            raise ValueError(f'Pack path escaped source: {p}')
        if len(rel.parts) != 2 or rel.parts[0] not in CATEGORIES:
            raise ValueError(f'Unexpected pack path: {rel}')
        if not re.fullmatch(r'[0-9A-Fa-f]{16}_[A-Za-z0-9_.-]+\.dds', rel.name):
            raise ValueError(f'Unexpected texture filename: {rel}')
        with p.open('rb') as f:
            if f.read(4) != b'DDS ':
                raise ValueError(f'Not a DDS file: {rel}')
        before = p.stat()
        sha = digest(p)
        after = p.stat()
        if (before.st_size, before.st_mtime_ns) != (after.st_size, after.st_mtime_ns):
            raise ValueError(f'Texture changed while hashing: {rel}')
        rows.append((rel.as_posix(), after.st_size, sha))
    if not rows:
        raise ValueError('No DDS textures found')
    return rows


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--pack', type=Path, default=ROOT/'analysis/texture-upscaled-4x-compact/texture_pack_dds')
    ap.add_argument('--version', default='1.0.0')
    ap.add_argument('--iscc', type=Path)
    ap.add_argument('--config-exe', type=Path, default=ROOT/'native/build-release/Release/config.exe',
                    help='Release Settings utility with the Textures tab')
    ap.add_argument('--output-dir', type=Path, default=ROOT/'native/installer/Output')
    ap.add_argument('--metadata-dir', type=Path, default=ROOT/'analysis/texture-installer-build')
    ap.add_argument('--metadata-only', action='store_true')
    ap.add_argument('--max-installer-bytes', type=int, default=2_000_000_000,
                    help='Reject a finished installer at or above this size (decimal bytes)')
    args = ap.parse_args()
    if not re.fullmatch(r'\d+\.\d+\.\d+', args.version):
        ap.error('version must have three numeric components')
    if args.max_installer_bytes <= 0:
        ap.error('max-installer-bytes must be positive')
    source = args.pack.resolve()
    config = args.config_exe.resolve()
    if not config.is_file():
        ap.error('Build the release config.exe first, or supply --config-exe PATH')
    config_sha = digest(config)
    metadata = args.metadata_dir.resolve()
    output = args.output_dir.resolve()
    if metadata.is_relative_to(source) or output.is_relative_to(source):
        ap.error('metadata and output directories must be outside the source pack')
    print(f'Hashing DDS source pack: {source}', flush=True)
    rows = catalogue(source)
    metadata.mkdir(parents=True, exist_ok=True)
    manifest = metadata/'manifest.txt'
    manifest.write_text(''.join(f'{sha} {size} {rel}\n' for rel, size, sha in rows), encoding='utf-8', newline='\n')
    files = metadata/'pack_files.iss'
    files.write_text(''.join(
        f'Source: "{source/Path(rel)}"; DestDir: "{{app}}\\texture_pack\\{Path(rel).parent}"; Flags: ignoreversion\n'
        for rel, _, _ in rows), encoding='utf-8-sig', newline='\n')
    info = dict(product='NHL Hitz 2003 Hi-Res Textures', version=args.version,
                minimum_game_version='1.0.1', format='DDS', upscale=4, files=len(rows),
                installer_compression='lzma2/max', solid_compression=True,
                install_mode='extract-only', creates_uninstaller=False,
                configuration_utility=dict(filename='config.exe', sha256=config_sha),
                installed_texture_bytes=sum(size for _, size, _ in rows),
                categories=dict(sorted(Counter(Path(rel).parts[0] for rel, _, _ in rows).items())),
                manifest_sha256=digest(manifest))
    (metadata/'pack-info.json').write_text(json.dumps(info, indent=2)+'\n', encoding='utf-8')
    readme = metadata/'README.txt'
    readme.write_text(
        f'NHL Hitz 2003 - 4x Hi-Res DDS Textures {args.version}\n\n'
        'Open config.exe: set Render Scale to 2x or higher, enable Use the hi-res\n'
        'textures on the Textures tab, then Save.\n\n'
        'Or press F1 in the game: set Render Scale to 2x or higher and turn\n'
        'Hi-res textures On.\n',
        encoding='utf-8', newline='\n')
    print(json.dumps(info, indent=2), flush=True)
    if args.metadata_only:
        return
    candidates = [args.iscc, shutil.which('ISCC.exe'),
                  Path(os.environ.get('LOCALAPPDATA', ''))/'Programs/Inno Setup 6/ISCC.exe',
                  Path(os.environ.get('ProgramFiles(x86)', 'C:/Program Files (x86)'))/'Inno Setup 6/ISCC.exe']
    iscc = next((Path(p) for p in candidates if p and Path(p).is_file()), None)
    if not iscc:
        ap.error('Inno Setup 6 compiler not found; supply --iscc PATH')
    output.mkdir(parents=True, exist_ok=True)
    exe = output/f'NHLHitz2003-HiRes-Textures-{args.version}.exe'
    # Publish only a verified candidate. A failed/oversized rebuild must not
    # overwrite an earlier valid installer or leave it with a stale checksum.
    with tempfile.TemporaryDirectory(prefix='.texture-build-', dir=output) as staging:
        subprocess.run([str(iscc), f'/DPackVersion={args.version}', f'/DPackFiles={files}',
                        f'/DPackManifest={manifest}', f'/DPackInfo={metadata/"pack-info.json"}',
                        f'/DPackReadme={readme}', f'/O{staging}',
                        f'/DPackConfig={config}',
                        str(ROOT/'native/installer/nhl_hitz_2003_textures.iss')], cwd=ROOT, check=True)
        print('Verifying that source textures remained unchanged...', flush=True)
        if rows != catalogue(source):
            raise RuntimeError('Source pack changed during build; installer must not be distributed')
        if digest(config) != config_sha:
            raise RuntimeError('Settings utility changed during build; installer must not be distributed')
        candidate = Path(staging)/exe.name
        if candidate.stat().st_size >= args.max_installer_bytes:
            raise RuntimeError(f'Installer exceeds size budget: {candidate.stat().st_size:,} bytes; '
                               f'must be below {args.max_installer_bytes:,}. Previous output preserved.')
        sha = digest(candidate)
        candidate.replace(exe)
    exe.with_suffix('.exe.sha256').write_text(f'{sha}  {exe.name}\n', encoding='ascii')
    shutil.copy2(readme, output/f'NHLHitz2003-HiRes-Textures-{args.version}-README.txt')
    (metadata/'build-result.json').write_text(json.dumps(dict(info, installer=str(exe),
        installer_bytes=exe.stat().st_size, installer_sha256=sha), indent=2)+'\n', encoding='utf-8')
    # Stable contract for the future game-installer download checkbox. The
    # publisher supplies a real URL after the release asset has been uploaded.
    integration = dict(product=info['product'], version=args.version, download_url=None,
        filename=exe.name, bytes=exe.stat().st_size, sha256=sha,
        minimum_game_version=info['minimum_game_version'], install_mode='extract-only',
        silent_arguments=['/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', '/DIR={game_dir}', '/LOG={log_path}'],
        success_exit_codes=[0])
    exe.with_suffix('.json').write_text(json.dumps(integration, indent=2)+'\n', encoding='utf-8')
    print(f'Installer: {exe}\nSHA-256: {sha}', flush=True)


if __name__ == '__main__':
    main()
