"""Build the small standalone texture downloader/extractor; never bundles DDS files."""
import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess

from build_texture_installer import ROOT, digest


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--version', default='1.0.1')
    ap.add_argument('--game-version', default='1.0.2', help='Version of the included Settings utility, independent of the texture pack')
    ap.add_argument('--iscc', type=Path)
    ap.add_argument('--output-dir', type=Path, default=ROOT/'native/installer/Output/texture-download')
    args = ap.parse_args()
    if not all(re.fullmatch(r'\d+\.\d+\.\d+', v) for v in (args.version, args.game_version)):
        ap.error('version and game-version must be MAJOR.MINOR.PATCH')
    candidates = [args.iscc, shutil.which('ISCC.exe'),
                  Path(os.environ.get('LOCALAPPDATA', ''))/'Programs/Inno Setup 6/ISCC.exe',
                  Path(os.environ.get('USERPROFILE', ''))/'AppData/Local/Programs/Inno Setup 6/ISCC.exe',
                  Path(os.environ.get('ProgramFiles(x86)', 'C:/Program Files (x86)'))/'Inno Setup 6/ISCC.exe']
    compiler = next((Path(p) for p in candidates if p and Path(p).is_file()), None)
    if compiler is None:
        ap.error('Supply --iscc PATH to Inno Setup 6.7 or newer')
    build = ROOT/'native/build-release'
    def run(command):
        subprocess.run([str(x) for x in command], cwd=ROOT, check=True)
    run(['cmake', '-S', 'native', '-B', build, '-G', 'Visual Studio 17 2022', '-A', 'x64',
         '-DNHL_RELEASE=ON', '-DHITZ_DEBUGKIT=OFF', f'-DHITZ_PACKAGE_VERSION={args.game_version}'])
    run(['cmake', '--build', build, '--config', 'Release', '--target', 'nhl_config',
         'nhl_texture_download', '--parallel', '2'])
    args.output_dir.mkdir(parents=True, exist_ok=True)
    run([compiler, '/DDownloadBuild=1', f'/DPackVersion={args.version}', f'/DBuildDir={build/"Release"}',
         f'/O{args.output_dir.resolve()}', ROOT/'native/installer/nhl_hitz_2003_textures.iss'])
    output = args.output_dir/f'NHLHitz2003-HiRes-Textures-{args.version}-Setup.exe'
    sha = digest(output)
    output.with_suffix('.exe.sha256').write_text(f'{sha}  {output.name}\n', encoding='ascii')
    print(f'Installer: {output}\nBytes: {output.stat().st_size:,}\nSHA-256: {sha}')


if __name__ == '__main__':
    main()
