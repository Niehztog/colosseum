"""Read Quake II retail assets: PAK archives, PCX/WAL images, MD2 models."""
import struct, os, glob
import numpy as np

class PakFS:
    def __init__(self, paks):
        self.index = {}          # name -> (pakpath, offset, length)
        for p in paks:
            try:
                f = open(p, 'rb')
                magic, ofs, ln = struct.unpack("<4sii", f.read(12))
                if magic != b'PACK':
                    f.close(); continue
                f.seek(ofs)
                for _ in range(ln // 64):
                    d = f.read(64)
                    name = d[:56].split(b'\0')[0].decode('latin1').replace('\\', '/').lower()
                    fp, fl = struct.unpack("<ii", d[56:64])
                    self.index.setdefault(name, (p, fp, fl))
                f.close()
            except Exception:
                pass

    def has(self, name): return name.lower() in self.index
    def read(self, name):
        p, fp, fl = self.index[name.lower()]
        with open(p, 'rb') as f:
            f.seek(fp); return f.read(fl)
    def list(self, prefix=''):
        return sorted(k for k in self.index if k.startswith(prefix))


def read_pcx(data):
    """8-bit RLE PCX -> (H,W) uint8 indices, (256,3) uint8 palette or None."""
    manuf, ver, enc, bpp = data[0], data[1], data[2], data[3]
    xmin, ymin, xmax, ymax = struct.unpack("<HHHH", data[4:12])
    bytes_per_line = struct.unpack("<H", data[66:68])[0]
    w, h = xmax - xmin + 1, ymax - ymin + 1
    out = np.zeros((h, bytes_per_line), np.uint8)
    p = 128
    for y in range(h):
        x = 0
        row = out[y]
        while x < bytes_per_line:
            b = data[p]; p += 1
            if (b & 0xC0) == 0xC0:
                run = b & 0x3F; b = data[p]; p += 1
                run = min(run, bytes_per_line - x)
                row[x:x + run] = b; x += run
            else:
                row[x] = b; x += 1
    pal = None
    if len(data) >= 769 and data[-769] == 0x0C:
        pal = np.frombuffer(data[-768:], np.uint8).reshape(256, 3).copy()
    return out[:, :w], pal


def read_wal(data):
    name = data[:32].split(b'\0')[0].decode('latin1')
    w, h = struct.unpack("<II", data[32:40])
    offs = struct.unpack("<4I", data[40:56])
    px = np.frombuffer(data[offs[0]:offs[0] + w * h], np.uint8).reshape(h, w)
    return px.copy(), name


class MD2:
    """Quake II alias model."""
    def __init__(self, data):
        f = struct.unpack("<17i", data[:68])
        (ident, ver, self.skinw, self.skinh, framesize, num_skins, self.num_xyz,
         num_st, self.num_tris, num_glcmds, self.num_frames,
         ofs_skins, ofs_st, ofs_tris, ofs_frames, ofs_glcmds, ofs_end) = f
        assert ident == 0x32504449 and ver == 8, "not an MD2"
        self.skins = [data[ofs_skins + 64 * i: ofs_skins + 64 * i + 64].split(b'\0')[0]
                      .decode('latin1').replace('\\', '/') for i in range(num_skins)]
        st = np.frombuffer(data[ofs_st:ofs_st + 4 * num_st], np.int16).reshape(num_st, 2)
        self.st = st.astype(np.float64) / np.array([self.skinw, self.skinh])
        tri = np.frombuffer(data[ofs_tris:ofs_tris + 12 * self.num_tris], np.int16).reshape(self.num_tris, 6)
        self.tri_xyz = tri[:, 0:3].astype(np.int32)
        self.tri_st = tri[:, 3:6].astype(np.int32)
        self.frames = []
        for i in range(self.num_frames):
            o = ofs_frames + framesize * i
            sc = np.frombuffer(data[o:o + 12], np.float32).astype(np.float64)
            tr = np.frombuffer(data[o + 12:o + 24], np.float32).astype(np.float64)
            nm = data[o + 24:o + 40].split(b'\0')[0].decode('latin1')
            v = np.frombuffer(data[o + 40:o + 40 + 4 * self.num_xyz], np.uint8).reshape(self.num_xyz, 4)
            xyz = v[:, :3].astype(np.float64) * sc + tr
            self.frames.append({'name': nm, 'xyz': xyz, 'ni': v[:, 3].astype(np.int32)})

    def frame_named(self, prefix):
        for i, fr in enumerate(self.frames):
            if fr['name'].lower().startswith(prefix.lower()):
                return i
        return 0

    def frame_names(self):
        return [f['name'] for f in self.frames]


def load_anorms(path):
    """id's 162-entry vertex normal table, parsed from anorms.h."""
    txt = open(path).read()
    import re
    nums = re.findall(r'\{\s*(-?\d*\.?\d+)\s*,\s*(-?\d*\.?\d+)\s*,\s*(-?\d*\.?\d+)\s*\}', txt)
    a = np.array([[float(x) for x in t] for t in nums])
    assert len(a) == 162, f"expected 162 normals, got {len(a)}"
    return a
