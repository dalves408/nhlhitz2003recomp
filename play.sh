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

# Clean Linux perf defaults (Nobara / Pro measurement learnings, 2026-11):
# - workers=2 helps exact snapshot memcmp on large spans
# - exact texture validation only (do NOT set RECOMP_GL_TEXTURE_VALIDATE=sampled)
# - draw coalesce off (do NOT set RECOMP_GL_DRAW_COALESCE=1)
# - on-screen FPS HUD gated by env; compiled out of NHL_RELEASE
export RECOMP_GL_WORKERS=${RECOMP_GL_WORKERS:-2}
unset RECOMP_GL_TEXTURE_VALIDATE
unset RECOMP_GL_DRAW_COALESCE
if [[ -z ${RECOMP_GL_FPS_OVERLAY+x} ]]; then
  export RECOMP_GL_FPS_OVERLAY=${HITZ_FPS_OVERLAY:-1}
fi
if [[ ${RECOMP_GL_FPS_OVERLAY} == 0 ]]; then
  unset RECOMP_GL_FPS_OVERLAY
fi

exec "$ROOT/native/build-linux/nhl_hitz_2003_recomp" "$@"
