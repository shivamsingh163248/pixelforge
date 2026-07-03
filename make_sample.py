import struct, zlib, os
w, h = 64, 64
rows = []
for y in range(h):
    row = bytearray([0])
    for x in range(w):
        row += bytes([(x * 4) & 255, (y * 4) & 255, ((x + y) * 2) & 255])
    rows.append(bytes(row))
raw = b"".join(rows)
def chk(t, d):
    return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)
png = (b"\x89PNG\r\n\x1a\n"
       + chk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
       + chk(b"IDAT", zlib.compress(raw))
       + chk(b"IEND", b""))
open("sample.png", "wb").write(png)
print("wrote", os.path.getsize("sample.png"), "bytes")
