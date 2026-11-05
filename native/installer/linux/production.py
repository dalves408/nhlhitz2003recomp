"""Real Linux test installer backend for the shared Tk wizard."""
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import threading
import time
import urllib.error
import urllib.request

from disc import validate
from menu_shortcuts import data_home, xdg_home, install_shortcuts, require_desktop_user

ROOT = Path(__file__).resolve().parent


def ensure_settings():
    """Create the fresh-install defaults without replacing an existing file."""
    user_data = data_home() / 'nhlhitz2003'
    user_data.mkdir(parents=True, exist_ok=True)
    try:
        with (user_data / 'settings.ini').open('x') as output:
            output.write('[display]\nresolution_scale=1\nfullscreen=1\nstretch=0\nvsync=0\nframe_rate=60\nmsaa=1\nanisotropy=1\n\n[textures]\nenabled=0\n')
    except FileExistsError:
        pass


def digest(path, cancelled=lambda: False, progress=lambda done, total: None):
    h = hashlib.sha256()
    done, total = 0, path.stat().st_size
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            if cancelled():
                raise InterruptedError('Stopped. Downloaded files have been kept.')
            h.update(chunk)
            done += len(chunk)
            progress(done, total)
    return h.hexdigest()


def publish(stage, dest):
    """Overlay staged program files, moving game data only when imported."""
    if not dest.exists():
        stage.rename(dest)
        return
    if not dest.is_dir() or dest.is_symlink():
        raise ValueError('Installation folder changed during setup.')
    assets = {'game_files', 'texture_pack'}
    directories = set()
    moves = []
    for path in stage.rglob('*'):
        relative = path.relative_to(stage)
        if relative.parts[0] in assets:
            if len(relative.parts) == 1:
                moves.append((path, dest / relative))
        elif path.is_dir():
            directories.add(dest / relative)
        else:
            moves.append((path, dest / relative))
    for directory in sorted(directories):
        if directory.is_symlink() or (directory.exists() and not directory.is_dir()):
            raise ValueError('A program folder conflicts with an existing path: ' + str(directory))
    for _, target in moves:
        if target.is_symlink() or (target.exists() and
                (not target.is_dir() if target.name in assets and target.parent == dest else not target.is_file())):
            raise ValueError('A setup file conflicts with an existing path: ' + str(target))
    backup = stage.parent / 'rollback'
    backup.mkdir()
    replaced, installed, created = [], [], []
    try:
        for directory in sorted(directories):
            if not directory.exists():
                directory.mkdir()
                created.append(directory)
        for index, (path, target) in enumerate(moves):
            if target.exists():
                saved = (target.with_name(f'game_files.backup-{time.time_ns()}')
                         if target == dest / 'game_files' else backup / str(index))
                if target == dest / 'texture_pack' and any(target.iterdir()):
                    raise ValueError('Textures appeared during setup. Run Setup again to keep them.')
                target.rename(saved)
                replaced.append((saved, target))
            path.rename(target)
            installed.append((target, path))
    except (OSError, ValueError):
        for target, path in reversed(installed):
            target.rename(path)
        for saved, target in reversed(replaced):
            saved.rename(target)
        for directory in reversed(created):
            directory.rmdir()
        raise


class ProductionBackend:
    is_demo = False
    default_folder = Path.home() / 'Games/NHL Hitz 2003'

    def __init__(self):
        require_desktop_user()
        self.stop = threading.Event()
        self.metadata = json.loads((ROOT / 'release.json').read_text())
        self.version = self.metadata['version']
        self.pack = self.metadata['texture']
        self.cache = xdg_home('XDG_CACHE_HOME', '.cache') / 'nhlhitz2003/downloads' / self.pack['sha256']
        self.downloaded = None
        self.delete_on_exit = None

    def cancel(self):
        self.stop.set()

    def cancelled(self):
        return self.stop.is_set()

    def texture_summary(self):
        local = Path(os.environ.get('HITZ_SETUP_DIR', ROOT)) / 'texture.bin'
        if local.is_file():
            return 'texture.bin found beside Setup. It will be verified and extracted; no download is needed.'
        if (self.cache / 'texture.bin').is_file():
            return 'A saved texture download is available. Setup will verify and use it.'
        if (self.cache / 'texture.bin.part').is_file():
            return 'An interrupted texture download was found. Setup will resume it.'
        return 'texture.bin is not beside Setup. The 1.85 GB texture pack will be downloaded.'

    def validate(self, source, report):
        self.stop.clear()
        return validate(source, ROOT / 'supported-disc.json', report, cancelled=self.cancelled)

    @staticmethod
    def installed_game(destination):
        path = Path(destination) / 'game_files'
        return path if path.is_dir() else None

    @staticmethod
    def installed_textures(destination):
        path = Path(destination) / 'texture_pack'
        return path.is_dir() and any(path.iterdir())

    def check_destination(self, dest):
        if dest.exists() and (not dest.is_dir() or (any(dest.iterdir()) and not
                (self.installed_game(dest) or (dest / 'texture_pack').is_dir()
                 or (dest / '.hitz-linux-install.json').is_file()))):
            raise ValueError('Choose an empty folder or an existing NHL Hitz 2003 folder.')

    def _verify(self, path, report):
        if path.stat().st_size != self.pack['bytes']:
            return False
        last = [0.0]
        def update(done, total):
            now = time.monotonic()
            if now - last[0] > .15 or done == total:
                last[0] = now
                report(38 + done / total * 5, f'Verifying texture.bin — {done / total:.1%}')
        return digest(path, self.cancelled, update) == self.pack['sha256']

    def _payload(self, report):
        if self.cancelled():
            raise InterruptedError('Download stopped. Run Setup again to resume.')
        local = Path(os.environ.get('HITZ_SETUP_DIR', ROOT)) / 'texture.bin'
        if local.is_file():
            if not self._verify(local, report):
                raise ValueError('texture.bin beside Setup is not the required pack. Move it aside to download the current pack.')
            return local
        self.cache.mkdir(parents=True, exist_ok=True)
        complete, partial = self.cache / 'texture.bin', self.cache / 'texture.bin.part'
        if complete.exists():
            if not self._verify(complete, report):
                raise ValueError('Cached texture.bin failed verification. Remove it and retry: ' + str(complete))
            self.downloaded = complete
            return complete
        report(43, 'texture.bin was not found beside Setup. Downloading the shared texture pack…')
        count = partial.stat().st_size if partial.exists() else 0
        if count > self.pack['bytes']:
            raise ValueError('Partial download exceeds the expected size. Remove it and retry: ' + str(partial))
        if count < self.pack['bytes']:
            headers = {'User-Agent': 'NHLHitz2003-Linux-Setup', 'Accept-Encoding': 'identity'}
            if count:
                headers['Range'] = f'bytes={count}-'
            response = urllib.request.urlopen(urllib.request.Request(self.pack['url'], headers=headers), timeout=20)
            with response:
                # A server ignoring Range requires restarting, never appending a full response.
                if response.status == 200:
                    count = 0
                elif response.status != 206 or not re.fullmatch(
                        rf'bytes {count}-{self.pack["bytes"] - 1}/{self.pack["bytes"]}', response.headers.get('Content-Range', '')):
                    raise ValueError('The server returned an invalid resume range. Partial data was kept.')
                start_count, start, last = count, time.monotonic(), 0.0
                with partial.open('ab' if count else 'wb') as output:
                    while True:
                        if self.cancelled():
                            raise InterruptedError('Download stopped. Run Setup again to resume.')
                        block = response.read(256 * 1024)
                        if not block:
                            break
                        if count + len(block) > self.pack['bytes']:
                            raise ValueError('Server sent more data than expected.')
                        output.write(block)
                        count += len(block)
                        now = time.monotonic()
                        if now - last > .15 or count == self.pack['bytes']:
                            speed = (count - start_count) / max(.001, now - start)
                            report(43 + 22 * count / self.pack['bytes'],
                                   f'Downloading texture.bin — {count / self.pack["bytes"]:.1%}\n'
                                   f'{count / 1e9:.2f} / {self.pack["bytes"] / 1e9:.2f} GB • {speed / 1e6:.2f} MB/s')
                            last = now
        if not self._verify(partial, lambda p, text: report(66, text)):
            raise ValueError('Download is incomplete or failed SHA-256 verification. Cached data was kept: ' + str(partial))
        partial.replace(complete)
        self.downloaded = complete
        return complete

    def stage_program(self, stage):
        shutil.copytree(ROOT / 'player', stage, dirs_exist_ok=True)

    def install(self, destination, textures, shortcut, report, source, *, install_program=True):
        self.stop.clear()
        binary = ROOT / 'player/native/build-linux/nhl_hitz_2003'
        try:
            check = subprocess.run([str(binary), '--build-info'], capture_output=True, text=True, timeout=10)
        except OSError as exc:
            raise ValueError('The Linux player cannot run on this system. See README-LINUX.txt for runtime requirements.\n' + str(exc)) from exc
        if check.returncode:
            raise ValueError('A game runtime dependency is missing or incompatible. See README-LINUX.txt.\n' + check.stderr[-1600:])
        dest = Path(destination).expanduser().resolve()
        self.check_destination(dest)
        existing_game = self.installed_game(dest)
        reuse_game = False
        if existing_game:
            report(3, 'Checking the game files already installed…')
            try:
                self.validate(existing_game, lambda text: report(3, text))
                reuse_game = True
            except InterruptedError:
                raise
            except (OSError, ValueError) as exc:
                if not source or Path(source).expanduser().resolve() == existing_game.resolve():
                    raise ValueError('The installed game files need a supported disc image or extracted game folder.\n' + str(exc)) from exc
        if not reuse_game and not source:
            raise ValueError('Select your Xbox USA disc image or extracted game folder.')
        reuse_textures = self.installed_textures(dest)
        textures = textures and not reuse_textures
        if textures and not (shutil.which('7zz') or shutil.which('7z')):
            raise ValueError('Install 7-Zip to use the texture pack: Debian/Ubuntu: sudo apt install 7zip; Fedora: sudo dnf install 7zip')
        dest.parent.mkdir(parents=True, exist_ok=True)
        required = (0 if reuse_game else self.metadata['game_bytes']) + self.pack['extracted_bytes'] * bool(textures) + 200_000_000
        local = Path(os.environ.get('HITZ_SETUP_DIR', ROOT)) / 'texture.bin'
        if textures and not local.is_file() and not (self.cache / 'texture.bin').is_file():
            self.cache.mkdir(parents=True, exist_ok=True)
            partial = self.cache / 'texture.bin.part'
            remaining = max(0, self.pack['bytes'] - (partial.stat().st_size if partial.exists() else 0))
            if self.cache.stat().st_dev == dest.parent.stat().st_dev:
                required += remaining
            elif shutil.disk_usage(self.cache).free < remaining + 50_000_000:
                raise ValueError('There is not enough free space in the texture download cache: ' + str(self.cache))
        if shutil.disk_usage(dest.parent).free < required:
            raise ValueError(f'At least {required / 1e9:.2f} GB of free installation space is needed.')
        record_path = dest / '.hitz-linux-install.json'
        old_record = (json.loads(record_path.read_text()) if install_program and
                      record_path.is_file() and not record_path.is_symlink() else {})
        with tempfile.TemporaryDirectory(prefix='.hitz-setup-', dir=dest.parent) as temp:
            stage = Path(temp) / 'install'
            stage.mkdir()
            report(5, 'Verifying your game source…')
            last = [0.0]
            def importing(done, total, name):
                now = time.monotonic()
                if now - last[0] > .15 or done == total:
                    last[0] = now
                    report(5 + done / total * 30, f'Extracting game data — {done / total:.1%}\n{name}')
            if reuse_game:
                report(35, 'Keeping the installed game files.')
            else:
                validate(source, ROOT / 'supported-disc.json', destination=stage / 'game_files',
                         copy_progress=importing, cancelled=self.cancelled)
            if install_program:
                report(36, 'Installing the Linux game and settings application…')
                self.stage_program(stage)
            if reuse_textures:
                report(70, 'Keeping the installed texture pack.')
            payload = self._payload(report) if textures else None
            if payload:
                report(70, 'Extracting the hi-res texture pack…')
                tool = shutil.which('7zz') or shutil.which('7z')
                if not tool:
                    raise ValueError('7-Zip is required to extract texture.bin.')
                log = Path(temp) / '7zip.log'
                with log.open('wb') as output:
                    process = subprocess.Popen([tool, 'x', '-t7z', '-y', '-bsp1', '-o' + str(stage), str(payload)],
                                               stdout=output, stderr=subprocess.STDOUT)
                    try:
                        while process.poll() is None:
                            if self.cancelled():
                                raise InterruptedError('Texture extraction stopped. Downloaded data was kept.')
                            with log.open('rb') as progress_file:
                                progress_file.seek(max(0, log.stat().st_size - 4096))
                                matches = re.findall(rb'(\d+)%', progress_file.read())
                            if matches:
                                pct = min(100, int(matches[-1]))
                                report(70 + pct * .2, f'Extracting hi-res textures — {pct}%')
                            time.sleep(.15)
                        if process.returncode:
                            raise ValueError('Texture extraction failed. Downloaded data was kept.')
                    finally:
                        if process.poll() is None:
                            process.terminate()
                            try:
                                process.wait(timeout=3)
                            except subprocess.TimeoutExpired:
                                process.kill()
                                process.wait()
                manifest = stage / 'texture_pack/manifest.txt'
                if digest(manifest) != self.pack['manifest_sha256']:
                    raise ValueError('Extracted texture manifest does not match the release.')
                rows = manifest.read_text().splitlines()
                if len(rows) != self.pack['files']:
                    raise ValueError('Texture file count does not match the release.')
                for row in rows:
                    if self.cancelled():
                        raise InterruptedError('Installation stopped. Downloaded data was kept.')
                    expected, size, name = row.split(' ', 2)
                    path = stage / 'texture_pack' / name
                    if not path.resolve().is_relative_to(stage / 'texture_pack') or path.is_symlink() or path.stat().st_size != int(size):
                        raise ValueError('Missing or invalid extracted texture: ' + name)
            if self.cancelled():
                raise InterruptedError('Installation stopped. Downloaded data was kept.')
            if install_program:
                owned = {p.relative_to(stage).as_posix(): digest(p) for p in stage.rglob('*') if p.is_file() and not p.relative_to(stage).as_posix().startswith(('game_files/', 'texture_pack/'))}
                (stage / 'install-info.json').write_text(json.dumps(self.metadata, indent=2))
                owned['install-info.json'] = digest(stage / 'install-info.json')
                record = {'files': owned, 'shortcuts': old_record.get('shortcuts', {})}
                (stage / record_path.name).write_text(json.dumps(record, indent=2))
            publish(stage, dest)
        if shortcut:
            report(95, 'Creating application-menu shortcuts…')
            menu = install_shortcuts(dest, replace=True)
            record['shortcuts'] = menu['shortcuts']
            record['menu_refresh'] = menu['refresh']
            with tempfile.NamedTemporaryFile(mode='w', encoding='utf-8', dir=dest, delete=False) as stream:
                updated_record = Path(stream.name)
                json.dump(record, stream, indent=2)
            try:
                updated_record.replace(record_path)
            finally:
                updated_record.unlink(missing_ok=True)
        ensure_settings()
        report(100, 'Installation complete.')
        return dest

    def finish(self, parent):
        if self.downloaded:
            from tkinter import messagebox
            keep = messagebox.askyesno('Keep texture.bin?',
                'Keep the downloaded texture.bin for future repairs or reinstalls?\n\n' + str(self.downloaded), parent=parent)
            if not keep:
                self.delete_on_exit = self.downloaded
            self.downloaded = None

    def close(self):
        if self.delete_on_exit:
            try:
                self.delete_on_exit.unlink(missing_ok=True)
            except OSError:
                pass  # Cache in use: leave it available rather than fail setup exit.
            self.delete_on_exit = None

    def launch(self, destination):
        subprocess.Popen([str(Path(destination) / 'play.sh')], start_new_session=True)

    def configure(self, destination):
        subprocess.Popen([str(Path(destination) / 'settings.sh')], start_new_session=True)
