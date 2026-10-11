import struct
import sys

# Copies a floppy image replacing AUTOEXEC.BAT (same cluster) so it runs a given program.
src, dst, folder, program = sys.argv[1:5]
img = bytearray(open(src, 'rb').read())
new = ('ECHO OFF\r\nVERIFY OFF\r\nKEY300\r\nCD \\' + folder + '\r\n' + program + '\r\n').encode('ascii')
root = 7 * 512
data0 = root + 112 * 32
for i in range(112):
    e = root + i * 32
    if img[e:e + 11] == b'AUTOEXECBAT':
        clus = struct.unpack_from('<H', img, e + 26)[0]
        off = data0 + (clus - 2) * 1024
        img[off:off + 1024] = new.ljust(1024, b'\x00')
        struct.pack_into('<I', img, e + 28, len(new))
        break
else:
    sys.exit('AUTOEXEC.BAT not found')
open(dst, 'wb').write(img)
print(dst, new)
