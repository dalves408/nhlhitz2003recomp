#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
cd "$ROOT"
if [[ -d .venv/bin ]]; then export PATH="$ROOT/.venv/bin:$PATH"; fi
if [[ ! -f native/src/recomp/gen/recomp_dispatch.c ]]; then
    echo "Generate the guest sources first: .venv/bin/python scripts/prepare.py" >&2
    exit 1
fi
cmake -S native -B native/build-linux -G Ninja -DCMAKE_BUILD_TYPE=Release -DHITZ_GUEST_OPT_LEVEL=2
cmake --build native/build-linux --parallel "${HITZ_BUILD_JOBS:-1}"
if [[ "${1:-}" == --build-only ]]; then exit 0; fi
exec "$ROOT/native/build-linux/nhl_hitz_2003_recomp" "$@"
