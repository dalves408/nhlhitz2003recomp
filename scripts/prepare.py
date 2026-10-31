"""Run the Hitz 2003 pipeline with project-local artifacts and checked stages."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'analysis'
XBE = str(ROOT / 'game_files/default.xbe')
D, I, A = (OUT / name for name in ('disasm', 'identified', 'abi'))
STEPS = [
    ('parse', ['tools.xbe_parser', XBE, '--json', str(ROOT / 'game_files/default_analysis.json')]),
    ('disasm', ['tools.disasm', XBE, '--text-only', '--extra-sections', 'D3D,D3DX,XGRPH,DSOUND,WMADECXM,WMADEC,XPP', '--output', str(D), '--seed-functions', str(OUT / 'seeds_manual.json'), '-v']),
    ('identify', ['tools.func_id', XBE, '--functions', str(D / 'functions.json'), '--strings', str(D / 'strings.json'), '--xrefs', str(D / 'xrefs.json'), '--output', str(I)]),
    ('abi', ['tools.abi_analysis', XBE, '--disasm-dir', str(D), '--func-id-dir', str(I), '--output-dir', str(A), '-v']),
    ('recompile', ['tools.recomp', XBE, '--all', '--split', '250', '--game-name', 'NHL Hitz 2003', '--disasm-dir', str(D), '--func-id-dir', str(I), '--abi-dir', str(A), '--gen-dir', str(ROOT / 'native/src/recomp/gen'), '--exclude-manual', str(ROOT / 'native/src/recomp_manual.c'), '--output-dir', str(OUT / 'recomp')]),
]


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--from-stage', choices=[n for n, _ in STEPS], default='parse')
    args = ap.parse_args()
    import capstone
    print(f'Python {sys.version.split()[0]}, Capstone {capstone.__version__}', flush=True)
    seeds = json.loads((OUT / 'seeds_manual.json').read_text())
    if not isinstance(seeds, list) or not seeds:
        raise ValueError('seeds_manual.json must be a nonempty list')
    metadata = json.loads((ROOT / 'game_files/default_analysis.json').read_text())
    code_ranges = [(int(s['virtual_addr'], 16), int(s['virtual_addr'], 16) + s['raw_size'])
                   for s in metadata['sections'] if s['executable'] and s['name'] != 'DOLBY']
    for seed in seeds:
        address = int(seed['start'], 16)
        if not any(lo <= address < hi for lo, hi in code_ranges):
            raise ValueError(f"Seed {seed['start']} is outside file-backed Xbox CPU code; do not seed heap callback values")
    for name, argv in STEPS[[n for n, _ in STEPS].index(args.from_stage):]:
        gen = ROOT / 'native/src/recomp/gen'
        unchanged = {}
        if name == 'recompile' and gen.exists():
            for path in gen.iterdir():
                if path.is_file():
                    stat = path.stat()
                    unchanged[path.name] = (hashlib.sha256(path.read_bytes()).digest(), stat.st_atime_ns, stat.st_mtime_ns)
        logpath = OUT / f'prepare_{name}.log'
        print(f'{name}: {logpath}', flush=True)
        with logpath.open('w', encoding='utf-8') as log:
            result = subprocess.run([sys.executable, '-u', str(ROOT / 'scripts/tool_runner.py'), *argv], cwd=ROOT / 'tools/xboxrecomp', stdout=log, stderr=subprocess.STDOUT)
        if result.returncode:
            print(logpath.read_text(encoding='utf-8', errors='replace')[-6000:], file=sys.stderr)
            return result.returncode
        if name == 'disasm':
            starts = {int(f['start'], 16) for f in json.loads((D / 'functions.json').read_text())}
            missing = [s['start'] for s in seeds if int(s['start'], 16) not in starts]
            if missing:
                raise RuntimeError(f'Discovery missed required seeds: {missing}')
        if name == 'recompile':
            # The upstream writer rewrites every chunk. Preserve timestamps
            # only for byte-identical output so an instruction-hook change
            # does not unnecessarily recompile all 80 chunks.
            for filename, (digest, atime, mtime) in unchanged.items():
                path = gen / filename
                if path.is_file() and hashlib.sha256(path.read_bytes()).digest() == digest:
                    os.utime(path, ns=(atime, mtime))
            dispatch = (ROOT / 'native/src/recomp/gen/recomp_dispatch.c').read_text(encoding='utf-8')
            missing = [s['start'] for s in seeds if f"0x{int(s['start'], 16):08X}u" not in dispatch]
            if missing:
                raise RuntimeError(f'Translation lost required dispatch entries: {missing}')
            # Test the emitted bodies too: basic-block lifting bypasses
            # lift_instruction for LOOP, which a standalone hook test misses.
            copy_targets = {0x4BC23, 0x4BC53, 0x4BCE5, 0x4BD09, 0x4BD2D}
            for path in gen.glob('recomp_*.c'):
                body = path.read_text(encoding='utf-8')
                copy_targets = {va for va in copy_targets if
                    f'if ((ecx != 0) && (1)) goto loc_{va:08X};' not in body}
                if not copy_targets:
                    break
            if copy_targets:
                raise RuntimeError(f'MV30 copy loops lost their row-count fix: {sorted(copy_targets)}')
        print(f'{name}: complete', flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
