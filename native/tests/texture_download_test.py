"""Windows transport regression using only a private local HTTP server and test cache.
Build nhl_texture_download first, then run this script. No game is launched.
"""
import argparse, hashlib, http.server, json, pathlib, subprocess, sys, threading, time, uuid


def main():
    root = pathlib.Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe',type=pathlib.Path,default=root/'native/build-release/Release/nhl_texture_download.exe')
    args = parser.parse_args()
    exe = args.exe.resolve()
    work = root/'analysis/texture-download-tests'/('transport-'+uuid.uuid4().hex[:8])
    work.mkdir(parents=True)
    data = bytes(range(256)) * 12288
    sha = hashlib.sha256(data).hexdigest()
    requests = []
    dropped = set()

    class Handler(http.server.BaseHTTPRequestHandler):
        def log_message(self, *args): pass
        def do_GET(self):
            requests.append((self.path, self.headers.get('Range'), self.headers.get('If-Range')))
            if self.path == '/redirect':
                self.send_response(302); self.send_header('Location', '/drop-redirect'); self.end_headers(); return
            start = int(self.headers.get('Range', 'bytes=0-').split('=')[1].split('-')[0])
            resume = bool(self.headers.get('Range')) and self.path != '/ignore'
            if self.path == '/etag' and self.headers.get('If-Range') != '"current"': resume = False
            if not resume: start = 0
            self.send_response(206 if resume else 200)
            if resume:
                reported = start + 1 if self.path == '/bad-range' else start
                self.send_header('Content-Range', f'bytes {reported}-{len(data)-1}/{len(data)}')
            self.send_header('Content-Length', str(len(data)-start))
            self.send_header('ETag', '"current"')
            self.end_headers()
            body = bytearray(data[start:])
            if self.path == '/wrong': body[0] ^= 0xff
            try:
                if self.path.startswith('/drop') and self.path not in dropped:
                    dropped.add(self.path); self.wfile.write(body[:400000]); self.wfile.flush(); self.close_connection = True; return
                chunk = 8192 if self.path == '/slow' else 262144
                for offset in range(0, len(body), chunk):
                    self.wfile.write(body[offset:offset+chunk]); self.wfile.flush()
                    if self.path == '/slow': time.sleep(.004)
            except (ConnectionError, OSError): pass

    server = http.server.ThreadingHTTPServer(('127.0.0.1',0), Handler)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    url = f'http://127.0.0.1:{server.server_port}'

    def args(name, route='/normal', extra=()):
        cache = work/name
        return [str(exe), '--url', url+route, '--cache', str(cache), '--sha256', sha, '--bytes', str(len(data)), *extra]

    def run(name, route='/normal', extra=(), code=0):
        result = subprocess.run(args(name,route,extra), capture_output=True, text=True, timeout=45, creationflags=subprocess.CREATE_NO_WINDOW)
        (work/(name+'.log')).write_text(result.stdout)
        assert result.returncode == code, (name,result.returncode,result.stdout)
        if not code:
            path = work/name/'texture.bin'
            if 'READY local' not in result.stdout:
                assert path.read_bytes() == data, name
        return result.stdout

    def seed(name, prefix=data[:350000], etag=None):
        cache=work/name; cache.mkdir()
        (cache/'texture.bin.part').write_bytes(prefix)
        if etag: (cache/'texture.bin.etag').write_bytes(etag.encode('utf-16-le'))

    def launch(name, extra=()):
        log = (work/(name+'.log')).open('w')
        return subprocess.Popen(args(name,'/slow',extra), stdout=log, stderr=log, creationflags=subprocess.CREATE_NO_WINDOW),log

    def wait_partial(name):
        path=work/name/'texture.bin.part'
        for _ in range(200):
            if path.exists() and path.stat().st_size > 120000: return path
            time.sleep(.02)
        raise AssertionError('No partial progress: '+name)

    try:
        run('normal')
        count=len(requests); run('normal'); assert len(requests)==count # Verified completed cache.
        seed('resume'); start=len(requests); run('resume'); assert requests[start][1]=='bytes=350000-'
        seed('ignore'); run('ignore','/ignore')
        seed('bad-range'); before=(work/'bad-range/texture.bin.part').read_bytes()
        run('bad-range','/bad-range',code=3); assert (work/'bad-range/texture.bin.part').read_bytes()==before
        run('disconnect','/drop'); assert any(p=='/drop' and r for p,r,e in requests)
        run('redirect','/redirect'); assert sum(p=='/redirect' for p,r,e in requests)>=2
        seed('etag', b'X'*350000, '"old"'); run('etag','/etag')
        run('wrong','/wrong',code=4); assert not (work/'wrong/texture.bin').exists()
        local=work/'local texture.bin'; local.write_bytes(data); count=len(requests)
        run('local',extra=('--local',str(local))); assert len(requests)==count
        local.write_bytes(b'not the pack'); run('bad-local',extra=('--local',str(local)),code=5)
        assert local.read_bytes()==b'not the pack'
        seed('complete-part',data); count=len(requests); run('complete-part'); assert len(requests)==count
        seed('oversize',data+b'extra'); run('oversize')
        # Forced process termination simulates an installer crash/Windows restart.
        process,log=launch('restart'); partial=wait_partial('restart'); process.kill(); process.wait(); log.close()
        saved=partial.stat().st_size; start=len(requests); run('restart'); assert requests[start][1]==f'bytes={saved}-'
        cancel=work/'cancel.flag'
        process,log=launch('cancel',('--cancel',str(cancel))); wait_partial('cancel'); cancel.write_text('cancel')
        assert process.wait(timeout=15)==2; log.close(); saved=(work/'cancel/texture.bin.part').stat().st_size
        cancel.unlink(); start=len(requests); run('cancel'); assert requests[start][1]==f'bytes={saved}-'
        process,log=launch('shared é cache'); wait_partial('shared é cache')
        second=subprocess.run(args('shared é cache'),capture_output=True,text=True,timeout=30,creationflags=subprocess.CREATE_NO_WINDOW)
        assert process.wait(timeout=10)==0 and second.returncode==0; log.close()
        assert (work/'shared é cache/texture.bin').read_bytes()==data
        assert 'WAITING' in second.stdout
        parent=subprocess.Popen([sys.executable,'-c','import time; time.sleep(30)'],creationflags=subprocess.CREATE_NO_WINDOW)
        process,log=launch('parent-exit',('--parent',str(parent.pid))); wait_partial('parent-exit')
        parent.kill(); parent.wait(); assert process.wait(timeout=15)==2; log.close()
        saved=(work/'parent-exit/texture.bin.part').stat().st_size
        start=len(requests); run('parent-exit'); assert requests[start][1]==f'bytes={saved}-'
        # Oversized stale cache must not be accepted, even at the expected path.
        cache=work/'invalid-cache'; cache.mkdir(); (cache/'texture.bin').write_bytes(data+b'bad')
        run('invalid-cache')
        result='PASS: download/cache/local verification, range resume, ignored Range, invalid range rejection, disconnect retry, redirected retry, changed ETag, corrupt checksum rejection, complete partial promotion, oversized partial/cache recovery, forced process restart, cancellation/resume, shared lock and Unicode/space paths.'
        (work/'result.txt').write_text(result)
        (work/'requests.json').write_text(json.dumps(requests,indent=2))
        print(result,'\nEvidence:',work)
    finally:
        server.shutdown()


if __name__ == "__main__":
    main()
