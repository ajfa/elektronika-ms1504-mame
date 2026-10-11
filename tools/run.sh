#!/bin/bash
# Headless run of the ms1504 driver, no window and no sound.
# Usage: MAME_TREE=... ROMPATH=... tools/run.sh <seconds> <floppy image> [more MAME options]
# With SCRIPT=<file> it also runs tools/script.lua with that action file.
set -eu
: "${MAME_TREE:?set MAME_TREE}"
: "${ROMPATH:?set ROMPATH to the folder holding ms1504.zip}"
SECS=$1; FLOP=$2; shift 2
HERE=$(cd "$(dirname "$0")" && pwd)
ARGS=(ms1504 -rompath "$ROMPATH" -video none -sound none -skip_gameinfo -nothrottle
      -seconds_to_run "$SECS" -flop1 "$FLOP")
if [ -n "${SCRIPT:-}" ]; then
  ARGS+=(-autoboot_script "$HERE/script.lua" -autoboot_delay 0)
fi
env -u DISPLAY -u WAYLAND_DISPLAY -u XDG_SESSION_TYPE \
    SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy MS_SCRIPT="${SCRIPT:-}" \
    "$MAME_TREE/ms1504" "${ARGS[@]}" "$@"
