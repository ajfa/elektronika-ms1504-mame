#!/bin/bash
# Copies Integral's KEY300.EXE from disk 0 to an installed MS-DOS 4.01R system disk and runs it
# from CONFIG.SYS, so CP866 is loaded before COMMAND.COM prints its banner.
# Usage: add_key300_to_config.sh <disk 0 image> <system disk image>
set -eu
DISK0=$1; IMG=$2
export MTOOLS_SKIP_CHECK=1
T=$(mktemp -d)
mcopy -n -m -i "$DISK0" ::KEY300.EXE "$T/KEY300.EXE"
printf 'INSTALL=A:\\KEY300.EXE\r\nBREAK=ON\r\nBUFFERS=20\r\nFILES=20\r\n' > "$T/CONFIG.SYS"
mcopy -o -m -i "$IMG" "$T/KEY300.EXE" ::KEY300.EXE
mcopy -o -i "$IMG" "$T/CONFIG.SYS" ::CONFIG.SYS
mtype -i "$IMG" ::CONFIG.SYS
