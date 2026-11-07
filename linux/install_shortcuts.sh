#!/bin/bash
# Installs the "NHL Hitz 2003" and "NHL Hitz 2003 Settings" menu entries and icons for this checkout.
set -e
REPO="$(cd "$(dirname "$(readlink -f "$0")")/.." && pwd)"
APPS="${XDG_DATA_HOME:-$HOME/.local/share}/applications"; ICONS="${XDG_DATA_HOME:-$HOME/.local/share}/icons/hicolor"
mkdir -p "$APPS" "$ICONS/64x64/apps" "$ICONS/128x128/apps" "$ICONS/256x256/apps"
cp "$REPO/linux/icons/nhlhitz2003-64.png" "$ICONS/64x64/apps/nhlhitz2003.png"
cp "$REPO/linux/icons/nhlhitz2003-128.png" "$ICONS/128x128/apps/nhlhitz2003.png"
cp "$REPO/linux/icons/nhlhitz2003-256.png" "$ICONS/256x256/apps/nhlhitz2003.png"
for f in nhl-hitz-2003 nhl-hitz-2003-settings; do
  sed "s|@REPO@|$REPO|g" "$REPO/linux/$f.desktop.in" > "$APPS/$f.desktop"; chmod +x "$APPS/$f.desktop"
done
chmod +x "$REPO/linux/play.sh" "$REPO/linux/play_debug.sh"
echo "Installed menu entries in $APPS"