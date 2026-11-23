#!/bin/bash
# Start the macOS build of Melee Unlocked. Double-click in Finder, or from Terminal:
#   ./play-mac.command [path/to/melee.iso] [extra game options...]
# Without an ISO argument it uses the last disc you picked (or melee.iso next to this file),
# and otherwise asks for one. Build first with port/mac/build_all.sh (--arch arm64 for the native one).
# On Apple silicon the native build is used when it exists; MELEE_MAC_ARCH=x86_64 picks the Rosetta one.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
app="$here/build-mac-full-arm64-squad/MeleeUnlocked.app"
if [ "$(uname -m)" != arm64 ] || [ "${MELEE_MAC_ARCH:-arm64}" = x86_64 ] || [ ! -x "$app/Contents/MacOS/MeleeUnlocked" ]; then
  app="$here/build-mac-full-squad/MeleeUnlocked.app"
fi
state="${XDG_DATA_HOME:-$HOME/.local/share}/melee-unlocked/last-iso"
if [ ! -x "$app/Contents/MacOS/MeleeUnlocked" ]; then
  echo "No Mac build at $app" >&2
  exit 1
fi
iso=""
if [ $# -gt 0 ] && [ -f "$1" ]; then iso="$1"; shift; fi
[ -z "$iso" ] && [ -f "$state" ] && iso="$(cat "$state")"
[ -z "$iso" ] && [ -f "$here/melee.iso" ] && iso="$here/melee.iso"
if [ -z "$iso" ] || [ ! -f "$iso" ]; then
  iso="$(osascript -e 'POSIX path of (choose file with prompt "Choose your Melee NTSC 1.02 ISO")')" || exit 1
fi
mkdir -p "$(dirname "$state")"
printf '%s\n' "$iso" > "$state"
exec "$app/Contents/MacOS/MeleeUnlocked" --iso "$iso" "$@"
