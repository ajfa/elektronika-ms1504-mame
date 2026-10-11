#!/bin/bash
# Builds ms1504.exe with MSYS2 (MINGW64). Run from a MINGW64 shell:
#   MAME_TREE=/c/src/mame PYTHON=/c/Python3x/python.exe tools/build-windows.sh
# The tree must already contain driver/ms1504.cpp and the patches (see tools/build.sh).
# Copying build/generated from a Linux build of the same tree skips the slow layout step.
set -u
: "${MAME_TREE:?set MAME_TREE}"
: "${PYTHON:?set PYTHON to a regular Windows python.exe (not the Microsoft Store alias)}"
export OS=Windows_NT
cd "$MAME_TREE"
mingw32-make SUBTARGET=ms1504 SOURCES=src/mame/ussr/ms1504.cpp NOWERROR=1 \
  MINGW64=C:/msys64/mingw64 PTR64=1 TOOLCHAIN=C:/msys64/mingw64/bin/ \
  PYTHON_EXECUTABLE="$PYTHON" -j"${JOBS:-2}"
