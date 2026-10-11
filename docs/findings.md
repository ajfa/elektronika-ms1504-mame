# Findings

## Disk 0

Integral's "Диск установки 0" boots Toshiba MS-DOS 3.20 R3B (TBIOS.SYS,
TDOS.SYS, 1987) and runs KEY300 and SETUP from AUTOEXEC.BAT. It also holds
the demo (DEMO.EXE, Turbo Pascal BGI, "ИНТЕГРАЛ Минск БССР 1990") and the
factory tests (KOMPTEST.EXE, 1992).

- The Toshiba DOS checks the BIOS model byte at F000:FFFE against FCh and
  FEh. On anything else it prints `Illegal to use MS-DOS Ver 3.x on this
  machine` and boots anyway.
- KEY300.EXE is one INT 10h call: AX=1102h, BL=1, which loads CP866.
- DISK300.EXE is a Russian DISKCOPY 3.10 ("Устан." and "Рабоч." disks).

## MS-DOS 4.01R

SETUP installs "MS-DOS v4.01R" from three 3.5" 720 KB distribution disks,
bought separately according to its manual: two install disks and a SHELL
disk. These disks are not known to survive. The product is the Russian
MS-DOS 4.01 from Microsoft (README dated 22 February 1990, Redmond), which
survives as a 5.25" 360 KB set.

SETUP shows what the 3.5" disks held. It copies each one to a working disk
with `DISK300 A: B:`, then deletes from the system copy AUTOEXEC.BAT,
CONFIG.SYS, KEYB.COM, KEYBOARD.SYS, DISPLAY.SYS, SETFNT.SYS, SELECT.*,
FDISK.EXE, HIMEM.SYS, FASTOPEN.EXE, NLSFUNC.EXE and the .CPI files, and
copies CONF11.SYS and AUTOEX11.BAT in their place. From the utilities copy
it deletes EMM386.SYS, XMA2EMS.SYS, XMAEM.SYS, SMARTDRV.SYS, SHARE.EXE,
MODE.COM, COUNTRY.SYS, FILESYS.EXE and IFSFUNC.EXE.

`tools/rebuild_install_disks.sh` rebuilds the three disks from the 5.25"
set following those lists, with the 4.01 boot sector and IO.SYS, MSDOS.SYS
and COMMAND.COM as the first three directory entries. The factory SETUP then
installs from them in the emulator without complaint.

## Code pages

The machine powers on with the GOST "basic" table, and the Russian MS-DOS
writes CP866, so a cold boot shows its banner garbled. The BIOS code does
this on every cold boot, so it is not an emulation artefact. `tools/add_key300_to_config.sh` puts
`INSTALL=A:\KEY300.EXE` in CONFIG.SYS, which loads CP866 before COMMAND.COM
prints anything. SETUP itself reboots warm, so its result looks right the
first time.

## Running the installer unattended

`tools/script.lua` reads an action file and drives the machine: natural
keyboard text, key taps and holds, disk swaps, and waits for text to appear
on the screen. `tools/install-msdos401r.txt` takes SETUP through the full
installation in about 410 emulated seconds. Points learnt on the way:

- The text screen is read at B800:0000 plus the page offset in 40:4Eh;
  SETUP works on page 2.
- A disk swap has to wait for DISK300's own prompt. Swapping on its first
  line of output changes the disk while DISK300.EXE is still loading.
- Some prompts spell Russian words with Latin look-alike letters, so the
  waits use strings without them.
