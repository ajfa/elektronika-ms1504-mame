#!/bin/bash
# Builds the ms1504 subtarget of MAME on Linux.
# Usage: MAME_TREE=/path/to/mame tools/build.sh
set -eu
: "${MAME_TREE:?set MAME_TREE to a MAME 0.289 source tree}"
HERE=$(cd "$(dirname "$0")/.." && pwd)

cp "$HERE/driver/ms1504.cpp" "$MAME_TREE/src/mame/ussr/ms1504.cpp"
cd "$MAME_TREE"
for p in "$HERE"/patches/*.patch; do
  git apply --check "$p" 2>/dev/null && git apply "$p" && echo "applied $(basename "$p")"
done

# MAME needs about 2.5 GB per compiler job
JOBS=$(awk -v cpus="$(nproc)" '/MemAvailable/ { j = int($2 / 2621440); if (j < 1) j = 1; if (j > cpus) j = cpus; print j }' /proc/meminfo)
LINK=""
command -v mold >/dev/null && LINK="mold -run"
$LINK make SUBTARGET=ms1504 SOURCES=src/mame/ussr/ms1504.cpp NOWERROR=1 NO_USE_PIPEWIRE=1 -j"$JOBS"
ls -l "$MAME_TREE/ms1504"
