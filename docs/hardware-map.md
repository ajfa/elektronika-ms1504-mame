# Hardware map

Everything here was read from the 28/06/90 BIOS unless it says otherwise.
Addresses in the BIOS are F000:xxxx.

## Memory

| Range | Use |
|---|---|
| 00000-9FFFF | 640 KB RAM |
| B8000-BBFFF | 16 KB video RAM, mirrored at BC000 |
| B8000-B8FFF | character generator RAM while 3DFh bit 0 is set |
| F8000-FFFFF | BIOS, 32 KB |

The BIOS chip is a 27C512 holding the same 32 KB twice.

## I/O

| Port | Use |
|---|---|
| 000-00F, 080-083 | 8237 DMA and page registers |
| 020-021 | 8259 |
| 040-043 | 8253 |
| 060-063 | 8255, XT style; switches read through 062h |
| 070-071 | MC146818 compatible RTC (KA512VI1) |
| 0A0 | NMI mask |
| 0E0 | clock: bit 7 set gives 4.77 MHz, clear gives 7.16 MHz |
| 1F0-1FF | XTA hard disk, 18/02/93 BIOS only |
| 213 | expansion unit, written once at POST |
| 2F8, 3F8 | 8250 serial |
| 378 | parallel |
| 3D4-3DA | CGA |
| 3DF | bit 0: character generator RAM mapped at B800:0000 |
| 3F2, 3F4, 3F5 | NEC 765 type floppy controller |
| 3F7 | read: bit 7 disk change, bits 5 and 6 an external 5.25" drive |

The keyboard handler writes 00h to 0E0h on Ctrl+Alt+PgUp and 80h on
Ctrl+Alt+PgDn.

With bits 5 and 6 of 3F7h clear both drives are internal 720 KB ones. If
either is set the BIOS treats one unit as a 360 KB 5.25" drive (DBC1).

## Character generator

The BIOS (DA02) fills the 4 KB RAM in four 1 KB blocks:

| RAM | Source | Content |
|---|---|---|
| 000-3FF | A400 | thin font, characters 00-7F |
| 400-7FF | A800 | thin font, characters 80-FF |
| 800-BFF | FA6E | thick font, characters 00-7F |
| C00-FFF | AC00 | thick font, characters 80-FF |

INT 10h AX=1102h with BL=0, 1 or 2 copies a new upper half into both fonts
from A800, B000 or B800: GOST "basic" Cyrillic, CP866 and CP437. Holding
key 5Ah and pressing F1, F2 or F3 does the same from the keyboard
handler. The value is kept in 40:98h, which survives only a warm boot
(40:72h = 1234h), so the machine always powers on with "basic".

Which font the LCD uses for normal text and which for intensity is not
known. The driver guesses thin for normal text and thick for intensity,
since a published description gives the LCD a 4 KB generator and the
external monitor 2 KB.

## Keyboard

XT protocol and XT scan codes, plus:

| Code | Key |
|---|---|
| 54h | SysReq (INT 15h AH=85h) |
| 57h, 58h | F11, F12 |
| 5Ah | Rus/Lat while held: 40:18h bit 0 |
| 5Bh | Rus/Lat lock: toggles 40:18h bit 1 |

The translation table at A25E has four bytes per code: type, Latin,
Cyrillic (in "basic" coding) and Control. Letters give Cyrillic when
exactly one of the two Rus/Lat bits is set. The number row is a Soviet
typewriter row: unshifted it gives `) - / " : , . _ ? % ! ;`, shifted the
digits and `= $`, and with 5Ah held `& @ ^ { } \ < > | [ ] `` ` ``.
The Latin to Cyrillic layout is ЙЦУКЕН (table at 9D76).

## RTC

At POST the BIOS checks register D bit 7 and prints `RTC BATTERY ERROR` if
it is clear, then sets register A to 26h and B to 02h. The driver gives the
clock that state as its default NVRAM. With that default the 18/02/93 BIOS
prints `RTC DATA ERROR`; a key press goes on.
