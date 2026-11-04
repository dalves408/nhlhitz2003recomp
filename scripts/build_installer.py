"""Build the standalone player, importer, settings and Inno Setup package. Game data is never bundled."""
import argparse
import hashlib
import os
from pathlib import Path
import re
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--version', default='1.0.2')
    parser.add_argument('--debug', action='store_true', help='package the integrated xrdbg player build')
    parser.add_argument('--test', action='store_true', help='label a non-debug release-candidate package as a test build')
    parser.add_argument('--iscc', type=Path)
    parser.add_argument('--manifest-source', type=Path, help='regenerate metadata from a known-good extraction')
    args = parser.parse_args()
    if args.test and args.debug:
        parser.error('--test packages exclude the debugger; build --debug separately')
    if not re.fullmatch(r'\d+\.\d+\.\d+', args.version):
        parser.error('--version must be MAJOR.MINOR.PATCH')
    def run(command):
        subprocess.run([str(x) for x in command], cwd=ROOT, check=True)
    if args.manifest_source:
        run(['python', 'native/installer/gen_manifest.py', args.manifest_source])
    if not (ROOT/'native/installer/manifest.h').is_file():
        parser.error('disc metadata missing; use --manifest-source with your known-good extraction')
    candidates = [args.iscc, shutil.which('ISCC.exe'),
                  Path(os.environ.get('LOCALAPPDATA', ''))/'Programs/Inno Setup 6/ISCC.exe',
                  Path(os.environ.get('USERPROFILE', ''))/'AppData/Local/Programs/Inno Setup 6/ISCC.exe',
                  Path(os.environ.get('ProgramFiles(x86)', 'C:/Program Files (x86)'))/'Inno Setup 6/ISCC.exe']
    iscc = next((Path(p) for p in candidates if p and Path(p).is_file()), None)
    if not iscc:
        parser.error('Inno Setup 6 compiler not found; supply --iscc PATH')
    build = ROOT/('native/build-player-debug' if args.debug else 'native/build-release')
    run(['cmake', '-S', 'native', '-B', build, '-G', 'Visual Studio 17 2022', '-A', 'x64',
         '-DNHL_RELEASE=ON', f'-DHITZ_DEBUGKIT={"ON" if args.debug else "OFF"}',
         f'-DHITZ_PACKAGE_VERSION={args.version}'])
    run(['cmake', '--build', build, '--config', 'Release', '--target',
         'nhl_hitz_2003_recomp', 'nhl_import', 'nhl_config', 'nhl_texture_download', '--parallel', '6'])
    game_exe = 'nhl_hitz_2003-db.exe' if args.debug else 'nhl_hitz_2003.exe'
    if not (build/'Release'/game_exe).is_file():
        raise RuntimeError(f'Expected current build output missing: {game_exe}')
    command = [iscc, f'/DAppVersion={args.version}', f'/DBuildDir={build / "Release"}']
    suffix = '-debug' if args.debug else ''
    if args.debug:
        command += ['/DDebugBuild=1', '/DOutSuffix=-debug']
    if args.test:
        suffix = '-test'
        command += ['/DTestBuild=1', '/DOutSuffix=-test']
    run(command + [ROOT/'native/installer/nhl_hitz_2003.iss'])
    output = ROOT/f'native/installer/Output/NHLHitz2003-Setup-{args.version}{suffix}.exe'
    digest = hashlib.sha256(output.read_bytes()).hexdigest()
    output.with_suffix('.exe.sha256').write_text(f'{digest}  {output.name}\n')
    print(f'Installer: {output}\nSHA-256: {digest}')


if __name__ == '__main__':
    main()
