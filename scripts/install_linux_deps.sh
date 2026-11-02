#!/usr/bin/env bash
# Debian/Ubuntu host dependencies for native Linux development.
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
if [[ ! -r /etc/os-release ]]; then
    echo "Cannot identify this Linux distribution." >&2
    exit 1
fi
. /etc/os-release
case " ${ID:-} ${ID_LIKE:-} " in
    *" debian "*|*" ubuntu "*) ;;
    *) echo "This project helper currently supports Debian/Ubuntu only." >&2; exit 1 ;;
esac

sudo -v
# Use the pinned toolkit's dependency list; do not modify the submodule.
bash "$ROOT/tools/xboxrecomp/tools/linux/install_deps.sh"
sudo apt-get install -y ninja-build gdb clang lld ripgrep python3-venv python3-pip
python3 -m venv "$ROOT/.venv"
"$ROOT/.venv/bin/python" -m pip install 'capstone==5.0.7' psutil
echo "Linux dependencies installed. Activate with: source \"$ROOT/.venv/bin/activate\""
