# Reproducing

## Media

| What | Where | Check |
|---|---|---|
| BIOS 28/06/90, disk 0, passport and manual | `mc1504_docs.rar` on the leningrad.su Soviet electronics museum and on retro-computer.ru | see below |
| BIOS 18/02/93 (XTA) | `MS1504_hdd_bios.256`, retro-computer.ru, dumped by Byteman | see below |
| Russian MS-DOS 4.01 retail | old-dos.ru, `MS-DOS 4.01.ver.4.01 (Retail).Russian.zip` | 4-1.IMA to 4-5.IMA and 4-S.IMA |
| Schematics | `MS1504_sch.rar`, retro-computer.ru | |

The ROM set `ms1504.zip`:

| File | Size | CRC32 | SHA1 |
|---|---|---|---|
| pk300_mc1504_27c512.bin | 65536 | f0234773 | a8783fa55f584750ac0d96fcfb78adb2b7a717f2 |
| ms1504_hdd_bios.256 | 32768 | e4418f13 | 008ee600cab7222b6dea2f89273af34a955bdd88 |

Disk 0 (`mc1504BOOT.imz` and `mc1504SETUP.imz` in the archive are the same
image, as is `mc1504.IMA` on retro-computer.ru): SHA1
c0f413f12c50a80d55f48cb4fe7dc4042b940a0c.

## Build

    MAME_TREE=/path/to/mame0289 tools/build.sh

The script copies the driver into `src/mame/ussr/`, applies the two
patches and builds the subtarget. On Ubuntu 22.04 the PipeWire headers are
too old, hence `NO_USE_PIPEWIRE=1`; when that flag is first added to an
existing tree, add `REGENIE=1` once.

For Windows, build the same tree on Linux first, copy it with
`build/generated`, and run `tools/build-windows.sh` from an MSYS2 MINGW64
shell. The resulting ms1504.exe needs only system DLLs.

## Disks

    tools/rebuild_install_disks.sh <dir with the six .IMA files> <out dir>
    tools/set_autoexec.py <disk 0> demo.img DEMO DEMO.EXE
    tools/set_autoexec.py <disk 0> komptest.img KOMPTEST KOMPTEST.EXE
    tools/add_key300_to_config.sh <disk 0> <installed system disk>

The rebuilt disks get a new volume serial number each time; the files and
their dates are the original ones.

## Running

Headless, with a snapshot at the end:

    MAME_TREE=... ROMPATH=... tools/run.sh 700 demo.img

The unattended installer, from the repository root with a `disks/` folder
holding `install0.img`, `msdos401r_install1.img`, `msdos401r_install2.img`
and two blank 737280 byte images, `work_system.img` and
`work_utilities.img`:

    SCRIPT=tools/install-msdos401r.txt MAME_TREE=... ROMPATH=... \
      tools/run.sh 450 disks/install0.img

At the end the machine is at `A>` on the new system disk.
