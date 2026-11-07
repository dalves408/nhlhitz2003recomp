"""Record process/thread CPU and host memory once per second during a probe.

CPU percentages use 100% for one logical CPU. Host core readings include other
applications. This records no GPU utilization and does not infer GPU load.
"""
import argparse
import json
from pathlib import Path
import signal
import time

import psutil


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('pid', type=int)
    ap.add_argument('--output', type=Path, required=True)
    ap.add_argument('--seconds', type=float, default=900)
    args = ap.parse_args()
    if args.seconds <= 0:
        ap.error('seconds must be positive')
    process = psutil.Process(args.pid)
    identity = dict(pid=process.pid, created=process.create_time(),
                    executable=process.exe(), logical_cpus=psutil.cpu_count())
    running = True
    def stop(signum, frame):
        nonlocal running
        running = False
    signal.signal(signal.SIGTERM, stop)
    signal.signal(signal.SIGINT, stop)
    started = previous = time.monotonic()
    cpu = process.cpu_times()
    previous_cpu = cpu.user + cpu.system
    previous_threads = {t.id: t.user_time + t.system_time for t in process.threads()}
    psutil.cpu_percent(percpu=True)
    with args.output.open('x', buffering=1) as out:
        out.write(json.dumps(dict(identity=identity)) + '\n')
        while running and time.monotonic() - started < args.seconds:
            time.sleep(1)
            try:
                if not process.is_running() or process.status() == psutil.STATUS_ZOMBIE:
                    break
                now = time.monotonic()
                elapsed = now - previous
                cpu = process.cpu_times()
                total = cpu.user + cpu.system
                threads = process.threads()
                thread_cpu = [dict(id=t.id, cpu_percent=100 *
                    (t.user_time + t.system_time - previous_threads.get(t.id, t.user_time + t.system_time)) / elapsed)
                    for t in threads]
                out.write(json.dumps(dict(seconds=now-started,
                    cpu_percent=100*(total-previous_cpu)/elapsed,
                    rss_bytes=process.memory_info().rss,
                    available_memory_bytes=psutil.virtual_memory().available,
                    swap_used_bytes=psutil.swap_memory().used,
                    host_core_percent=psutil.cpu_percent(percpu=True),
                    threads=thread_cpu)) + '\n')
                previous, previous_cpu = now, total
                previous_threads = {t.id: t.user_time+t.system_time for t in threads}
            except (psutil.NoSuchProcess, psutil.AccessDenied):
                break


if __name__ == '__main__':
    main()
