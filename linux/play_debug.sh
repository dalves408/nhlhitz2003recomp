#!/bin/bash
# Developer script: play.sh with rumble / USB logging (not a release shortcut).
export RECOMP_RUMBLE_LOG=1 RECOMP_USB_STATS=1
exec "$(dirname "$(readlink -f "$0")")/play.sh" "$@"