"""Bounded native Linux run with isolated saves, logs and frame telemetry."""
import argparse
import ctypes
import hashlib
import json
import os
from pathlib import Path
import signal
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--tag', required=True)
    parser.add_argument('--seconds', type=float, default=120)
    parser.add_argument('--env', action='append', default=[])
    parser.add_argument('--executable', type=Path, default=ROOT / 'native/build-linux/nhl_hitz_2003_recomp')
    parser.add_argument('--guest-opt-level', choices=('0', '1', '2', '3'),
                        help='Known optimization level for a retained binary; overrides the current build cache')
    args = parser.parse_args()
    if not args.tag or any(c not in 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-' for c in args.tag):
        parser.error('tag must contain only letters, numbers, underscores or hyphens')
    if args.seconds <= 0:
        parser.error('seconds must be positive')
    base = ROOT / 'analysis' / args.tag
    if base.with_suffix('.run.json').exists() or base.with_suffix('.stderr.txt').exists():
        parser.error('use a new tag so previous evidence is preserved')
    env = os.environ.copy()
    env.update(XDG_DATA_HOME=str(base) + '_data',
               RECOMP_FRAME_CSV=str(base) + '.frames.csv',
               RECOMP_CRASH_GUEST_DUMP=str(base) + '_guest.bin',
               RECOMP_WATCHDOG_SECS='0')
    for item in args.env:
        key, sep, value = item.partition('=')
        if not sep:
            parser.error('--env expects NAME=VALUE')
        env[key] = value
    executable = args.executable.resolve()
    with executable.open('rb') as binary:
        binary_hash = hashlib.file_digest(binary, 'sha256').hexdigest()
    cache = executable.parent / 'CMakeCache.txt'
    build = {}
    if cache.exists():
        for line in cache.read_text().splitlines():
            if line.startswith(('CMAKE_BUILD_TYPE:', 'HITZ_GUEST_OPT_LEVEL:')):
                key, value = line.split('=', 1)
                build[key.split(':')[0]] = value
        build['optimization_source'] = 'current build cache; binary not verified'
    try:
        info = subprocess.run([str(executable), '--build-info'],
                              capture_output=True, text=True, timeout=5, check=True)
        level = json.loads(info.stdout)['guest_opt_level']
        if level not in (0, 1, 2, 3):
            raise ValueError('invalid binary optimization metadata')
        build['HITZ_GUEST_OPT_LEVEL'] = str(level)
        build['optimization_source'] = 'binary --build-info'
    except (subprocess.SubprocessError, ValueError, KeyError):
        pass
    if args.guest_opt_level is not None:
        build['HITZ_GUEST_OPT_LEVEL'] = args.guest_opt_level
        build['optimization_source'] = 'explicit retained-binary metadata'
    started = time.monotonic()
    reason = 'exit'
    interrupted = 0
    def interrupted_by_host(signum, frame):
        nonlocal interrupted
        interrupted = signum
    signal.signal(signal.SIGTERM, interrupted_by_host)
    signal.signal(signal.SIGINT, interrupted_by_host)
    parent_pid = os.getpid()
    libc = ctypes.CDLL(None, use_errno=True)
    def stop_if_parent_dies():
        # Also stop the native child if the supervisor receives SIGKILL or
        # the host tears down its command session before finally can run.
        if libc.prctl(1, signal.SIGTERM, 0, 0, 0) != 0:
            os._exit(125)
        if os.getppid() != parent_pid:
            os.kill(os.getpid(), signal.SIGTERM)
    with base.with_suffix('.stdout.txt').open('wb') as out, base.with_suffix('.stderr.txt').open('wb') as err:
        child = subprocess.Popen([str(executable)],
                                 cwd=ROOT / 'native', env=env, stdout=out, stderr=err,
                                 start_new_session=True, preexec_fn=stop_if_parent_dies)
        base.with_suffix('.pid').write_text(str(child.pid))
        try:
            while child.poll() is None:
                if interrupted:
                    reason = 'supervisor-signal-' + str(interrupted)
                    break
                if time.monotonic() - started >= args.seconds:
                    reason = 'deadline'
                    break
                if base.with_suffix('.stop').exists():
                    reason = 'stop-file'
                    break
                time.sleep(.2)
        finally:
            if child.poll() is None:
                try:
                    os.killpg(child.pid, signal.SIGTERM)
                except ProcessLookupError:
                    pass
                try:
                    child.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    try:
                        os.killpg(child.pid, signal.SIGKILL)
                    except ProcessLookupError:
                        pass
                    child.wait()
    result = dict(reason=reason, returncode=child.returncode,
                  elapsed=time.monotonic() - started, pid=child.pid,
                  executable_sha256=binary_hash, build=build,
                  overrides={item.partition('=')[0]: item.partition('=')[2]
                             for item in args.env if item.startswith(('RECOMP_', 'XDG_DATA_HOME='))})
    base.with_suffix('.run.json').write_text(json.dumps(result, indent=2) + '\n')
    try:
        print(json.dumps(result))
    except BrokenPipeError:
        pass
    if interrupted:
        return 128 + interrupted
    return 0 if reason in ('deadline', 'stop-file') else child.returncode


if __name__ == '__main__':
    raise SystemExit(main())
