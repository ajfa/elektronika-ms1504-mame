#!/bin/bash
# Rebuilds the three 3.5" 720 KB MS-DOS 4.01R distribution disks that the MS 1504
# "Disk 0" installer (SETUP.EXE) expects, from the Russian MS-DOS 4.01 retail 5.25" set.
# The split follows the files SETUP.EXE deletes from each copied disk.
# Usage: rebuild_install_disks.sh <dir with 4-1.IMA..4-5.IMA and 4-S.IMA> <output dir>
set -eu
SRC=$1; OUT=$2
export MTOOLS_SKIP_CHECK=1 MTOOLS_NO_VFAT=1
TMP=$(mktemp -d)
mkdir -p "$OUT"

for d in 1 2 3 4 5 S; do
  mkdir -p "$TMP/$d"
  mcopy -n -m -i "$SRC/4-$d.IMA" '::*' "$TMP/$d/" 2>/dev/null
done
head -c 512 "$SRC/4-1.IMA" > "$TMP/boot.bin"

newdisk() {
  rm -f "$1"
  mformat -C -f 720 -B "$TMP/boot.bin" -i "$1" ::
}

label() {
  mlabel -i "$1" "::$2"
}

put() {
  local img=$1; shift
  for f in "$@"; do
    local s
    for s in 1 2 3 4 5 S; do
      if [ -f "$TMP/$s/$f" ]; then
        mcopy -n -m -i "$img" "$TMP/$s/$f" "::$f"
        continue 2
      fi
    done
    echo "missing $f" >&2; exit 1
  done
}

D1=$OUT/msdos401r_install1.img
newdisk "$D1"
put "$D1" IO.SYS MSDOS.SYS COMMAND.COM
mattrib -i "$D1" -a +s +h +r ::IO.SYS ::MSDOS.SYS
label "$D1" "VERSION 401"
put "$D1" CONFIG.SYS AUTOEXEC.BAT COUNTRY.SYS \
  DISPLAY.SYS SETFNT.SYS KEYB.COM KEYBOARD.SYS SELECT.COM SELECT.EXE SELECT.DAT \
  SELECT.HLP SELECT.PRT FDISK.EXE FORMAT.COM SYS.COM DISKCOPY.COM HIMEM.SYS \
  FASTOPEN.EXE NLSFUNC.EXE EGA.CPI 4201.CPI 4208.CPI 5202.CPI CHKDSK.COM \
  ANSI.SYS DRIVER.SYS

D2=$OUT/msdos401r_install2.img
newdisk "$D2"
label "$D2" "VERSION 401"
put "$D2" MODE.COM COUNTRY.SYS SHARE.EXE EMM386.SYS XMA2EMS.SYS \
  XMAEM.SYS SMARTDRV.SYS FILESYS.EXE IFSFUNC.EXE ASSIGN.COM ATTRIB.EXE COMP.COM \
  DEBUG.COM DISKCOMP.COM EDLIN.COM FC.EXE FIND.EXE JOIN.EXE LABEL.COM MEM.EXE \
  MORE.COM SORT.EXE SUBST.EXE TREE.COM XCOPY.EXE README.TXT GRAFTABL.COM \
  RECOVER.COM REPLACE.EXE APPEND.EXE PRINT.COM RAMDRIVE.SYS PRINTER.SYS \
  GRAPHICS.COM GRAPHICS.PRO BACKUP.COM RESTORE.COM EXE2BIN.EXE

D3=$OUT/msdos401r_shell.img
newdisk "$D3"
label "$D3" "VERSION 401"
put "$D3" SHELLB.COM SHELLC.EXE SHELL.CLR SHELL.HLP SHELL.MEU \
  DOSUTIL.MEU PCIBMDRV.MOS

for d in "$D1" "$D2" "$D3"; do
  echo "== $(basename "$d"): $(mdir -i "$d" :: | grep -E "files|free" | tr -s " " | tr "
" " ")"
done
rm -rf "$TMP"
