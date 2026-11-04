"""Build and fully verify the shared, lossless texture.bin download payload.

The .bin is a 7z/LZMA2 archive, independent of either installer's executable.
It contains texture_pack/<category>/<original DDS name> plus pack metadata.
"""
import argparse
from collections import Counter
import json
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

from build_texture_installer import ROOT, catalogue, digest


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--pack', type=Path, default=ROOT/'analysis/texture-addon-under-2gb/texture_pack_dds')
    ap.add_argument('--version', default='1.0.1')
    ap.add_argument('--sevenzip', type=Path, default=Path('C:/Program Files/7-Zip/7z.exe'))
    ap.add_argument('--output-dir', type=Path, default=ROOT/'native/installer/Output/texture-payload-1.0.1')
    ap.add_argument('--metadata-dir', type=Path, default=ROOT/'analysis/texture-payload-build')
    ap.add_argument('--expected-manifest', type=Path, help='Require exact parity with a previously tested DDS manifest')
    ap.add_argument('--max-bytes', type=int, default=2_000_000_000)
    args = ap.parse_args()
    if not re.fullmatch(r'\d+\.\d+\.\d+', args.version) or args.max_bytes <= 0:
        ap.error('Use a numeric three-component version and a positive size limit')
    source, output, metadata = (p.resolve() for p in (args.pack, args.output_dir, args.metadata_dir))
    for directory in (output, metadata):
        if directory == source or directory.is_relative_to(source):
            ap.error('Output and metadata must be outside the source pack')
    sevenzip = args.sevenzip.resolve()
    if not sevenzip.is_file():
        ap.error('7-Zip not found; supply --sevenzip PATH')
    print(f'Hashing source DDS files: {source}', flush=True)
    rows = catalogue(source)
    manifest = ''.join(f'{sha} {size} {rel}\n' for rel, size, sha in rows)
    if args.expected_manifest and args.expected_manifest.read_text(encoding='utf-8-sig') != manifest:
        raise RuntimeError('Source pack differs from the tested manifest; nothing published')
    output.mkdir(parents=True, exist_ok=True)
    metadata.mkdir(parents=True, exist_ok=True)
    # Candidates/extracted test files stay inside this explicit build directory.
    # Failure or an oversized candidate preserves any previously verified payload.
    with tempfile.TemporaryDirectory(prefix='payload-', dir=metadata) as temp:
        work = Path(temp).resolve()
        assert work.is_relative_to(metadata) and work != metadata
        stage = work/'stage'
        pack = stage/'texture_pack'
        pack.mkdir(parents=True)
        print('Staging a snapshot with original DDS names and folders...', flush=True)
        for rel, size, sha in rows:
            target = pack/rel
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source/rel, target)
            if target.stat().st_size != size or digest(target) != sha:
                raise RuntimeError(f'Staged texture changed: {rel}')
        (pack/'manifest.txt').write_text(manifest, encoding='utf-8', newline='\n')
        info = dict(product='NHL Hitz 2003 Hi-Res Textures', version=args.version,
                    minimum_game_version='1.0.1', format='DDS', upscale=4,
                    files=len(rows), installed_texture_bytes=sum(row[1] for row in rows),
                    categories=dict(sorted(Counter(Path(row[0]).parts[0] for row in rows).items())),
                    manifest_sha256=digest(pack/'manifest.txt'),
                    payload_format='7z', compression='LZMA2', creates_uninstaller=False)
        (pack/'pack-info.json').write_text(json.dumps(info, indent=2)+'\n', encoding='utf-8')
        (pack/'README.txt').write_text(
            'NHL Hitz 2003 - 4x Hi-Res Textures\n\n'
            'Open config.exe: choose Render Scale 2x or higher, enable Use the hi-res\n'
            'textures on the Textures tab, then Save.\n\n'
            'Or press F1 in the game: choose Render Scale 2x or higher and turn\n'
            'Hi-res textures On.\n', encoding='utf-8', newline='\n')
        candidate = work/'texture.bin'
        command = [str(sevenzip), 'a', '-t7z', '-m0=LZMA2', '-mx=7', '-md=32m',
                   '-ms=256m', '-mmt=2', '-mtc=off', '-mta=off', '-bsp0',
                   str(candidate), 'texture_pack']
        print('Compressing shared payload (LZMA2, 2 threads)...', flush=True)
        subprocess.run(command, cwd=stage, check=True)
        if candidate.stat().st_size >= args.max_bytes:
            raise RuntimeError(f'Payload is {candidate.stat().st_size:,} bytes; limit is strictly below {args.max_bytes:,}')
        print('Testing archive integrity...', flush=True)
        subprocess.run([str(sevenzip), 't', '-t7z', '-bsp0', str(candidate)], check=True)
        print('Extracting and verifying every file, filename and DDS hash...', flush=True)
        extracted = work/'extracted'
        subprocess.run([str(sevenzip), 'x', '-t7z', '-bsp0', '-y', '-o'+str(extracted), str(candidate)], check=True)
        if catalogue_dds(extracted/'texture_pack') != rows:
            raise RuntimeError('Extracted DDS catalogue differs from source')
        expected = {p.relative_to(stage).as_posix(): (p.stat().st_size, digest(p))
                    for p in stage.rglob('*') if p.is_file()}
        actual = {p.relative_to(extracted).as_posix(): (p.stat().st_size, digest(p))
                  for p in extracted.rglob('*') if p.is_file()}
        if expected != actual:
            raise RuntimeError('Extracted archive paths or metadata differ from staged payload')
        if rows != catalogue(source):
            raise RuntimeError('Source textures changed during build')
        sha, size = digest(candidate), candidate.stat().st_size
        result = dict(info, filename='texture.bin', bytes=size, sha256=sha,
                      download_url=None, archive_root='texture_pack',
                      local_lookup='texture.bin beside the setup executable',
                      verification='7z integrity test plus full extracted path/size/SHA-256 parity and unchanged source catalogue',
                      compression_arguments=command[2:-2])
        # Only the tested candidate replaces the published local output.
        candidate.replace(output/'texture.bin')
        (output/'texture.bin.sha256').write_text(f'{sha}  texture.bin\n', encoding='ascii')
        (output/'texture-payload.json').write_text(json.dumps(result, indent=2)+'\n', encoding='utf-8')
        (metadata/'build-result.json').write_text(json.dumps(result, indent=2)+'\n', encoding='utf-8')
        (metadata/'manifest.txt').write_text(manifest, encoding='utf-8', newline='\n')
    print(f'Payload: {output/"texture.bin"}\nBytes: {size:,}\nSHA-256: {sha}', flush=True)


def catalogue_dds(pack):
    """Validate DDS files separately from the three allowed metadata files."""
    rows = []
    for path in sorted(pack.rglob('*.dds')):
        rel = path.relative_to(pack).as_posix()
        rows.append((rel, path.stat().st_size, digest(path)))
    return rows


if __name__ == '__main__':
    main()
