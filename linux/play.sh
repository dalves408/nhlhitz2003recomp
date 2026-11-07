#!/bin/bash
# NHL Hitz 2003 (Linux): normal play with the tuned GL settings, 60 FPS cap, no test automation.
# Frame rate, render scale, MSAA and hi-res come from settings.ini (F1 menu or linux/nhl_config.py).
REPO="$(cd "$(dirname "$(readlink -f "$0")")/.." && pwd)"
cd "$REPO/native" || exit 1
export DRI_PRIME=${DRI_PRIME:-1} SDL_AUDIODRIVER=${SDL_AUDIODRIVER:-pulseaudio}
export RECOMP_GL_VERTEX_UPLOAD=stream RECOMP_GL_VERTEX_INDEXED=1
export RECOMP_GL_TEXTURE_WATCH=1 RECOMP_GL_TEXTURE_WATCH_CLEAR_EVERY=8 RECOMP_GL_TEXTURE_WATCH_FAST=1
export RECOMP_GL_TEXTURE_INDEX=1 RECOMP_PB_IDLE_WAIT_US=100
mkdir -p "$REPO/analysis"
export RECOMP_GL_SHADER_PREWARM="$REPO/analysis/hitz_prewarm.txt"
exec ./build-linux/nhl_hitz_2003_recomp "$@" > "$REPO/analysis/play.log" 2>&1