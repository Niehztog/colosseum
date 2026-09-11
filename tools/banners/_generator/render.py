"""Software rasterizer with the Quake II look: nearest-sampled textures,
Gouraud vertex lighting, z-buffer, distance falloff, 8-bit palette output."""
import numpy as np

# ---------------------------------------------------------------- math
def norm(v):
    v = np.asarray(v, float); n = np.linalg.norm(v)
    return v / n if n else v

def look_at(eye, target, up=(0, 0, 1)):
    eye = np.asarray(eye, float); target = np.asarray(target, float)
    f = norm(target - eye)
    r = norm(np.cross(f, np.asarray(up, float)))
    u = np.cross(r, f)
    M = np.stack([r, u, f])              # rows: right, up, forward
    return M, eye

def rot_z(deg):
    a = np.radians(deg); c, s = np.cos(a), np.sin(a)
    return np.array([[c, -s, 0], [s, c, 0], [0, 0, 1.0]])


class Mesh:
    """Triangle soup: world verts, uvs, per-vertex light, one texture."""
    def __init__(self, verts, uvs, light, tex, unlit=False, alpha=1.0, alpha_tex=None):
        self.v = np.asarray(verts, float)      # (N,3,3)
        self.uv = np.asarray(uvs, float)       # (N,3,2)
        self.l = np.asarray(light, float)      # (N,3)
        self.tex = tex                         # (H,W,3) float 0..1
        self.unlit = unlit
        self.alpha = alpha
        self.alpha_tex = alpha_tex   # (H,W) per-texel coverage, for decals


class Renderer:
    def __init__(self, w, h, fov=65.0, ss=2):
        self.W, self.H = w * ss, h * ss
        self.ss = ss
        self.fov = fov
        self.color = np.zeros((self.H, self.W, 3), np.float32)
        self.depth = np.full((self.H, self.W), 1e30, np.float32)
        self.fog_color = np.array([0.10, 0.08, 0.07])
        self.fog_start, self.fog_end = 700.0, 4200.0

    def set_camera(self, eye, target, up=(0, 0, 1)):
        self.M, self.eye = look_at(eye, target, up)
        self.focal = (self.W / 2.0) / np.tan(np.radians(self.fov) / 2.0)

    def clear(self, sky=None):
        if sky is None:
            self.color[:] = self.fog_color
        else:
            self.color[:] = sky
        self.depth[:] = 1e30

    # -------------------------------------------------- near-plane clip
    @staticmethod
    def _clip_near(P, attrs, znear=4.0):
        """Clip one triangle (3,3) camera-space against z>znear. Returns list of tris."""
        d = P[:, 2] - znear
        inside = d > 0
        n = int(inside.sum())
        if n == 3:
            return [(P, attrs)]
        if n == 0:
            return []
        idx = [0, 1, 2]
        out_p, out_a = [], []
        for i in range(3):
            j = (i + 1) % 3
            if inside[i]:
                out_p.append(P[i]); out_a.append([a[i] for a in attrs])
            if inside[i] != inside[j]:
                t = d[i] / (d[i] - d[j])
                out_p.append(P[i] + t * (P[j] - P[i]))
                out_a.append([a[i] + t * (a[j] - a[i]) for a in attrs])
        tris = []
        for k in range(1, len(out_p) - 1):
            P2 = np.stack([out_p[0], out_p[k], out_p[k + 1]])
            A2 = [np.stack([out_a[0][m], out_a[k][m], out_a[k + 1][m]]) for m in range(len(attrs))]
            tris.append((P2, A2))
        return tris

    def draw(self, mesh):
        M, eye = self.M, self.eye
        V = (mesh.v.reshape(-1, 3) - eye) @ M.T
        V = V.reshape(-1, 3, 3)
        tex = mesh.tex
        th, tw = tex.shape[0], tex.shape[1]
        cx, cy = self.W / 2.0, self.H / 2.0
        f = self.focal

        # cheap cull: entirely behind camera
        keep = (V[:, :, 2] > 4.0).any(axis=1)
        idxs = np.nonzero(keep)[0]

        for ti in idxs:
            P = V[ti]
            uv = mesh.uv[ti]
            li = mesh.l[ti]
            for Pc, (uvc, lic) in self._clip_near(P, [uv, li]):
                z = Pc[:, 2]
                sx = cx + f * Pc[:, 0] / z
                sy = cy - f * Pc[:, 1] / z
                # backface / degenerate
                area = (sx[1]-sx[0])*(sy[2]-sy[0]) - (sx[2]-sx[0])*(sy[1]-sy[0])
                if abs(area) < 1e-9:
                    continue
                x0 = max(int(np.floor(sx.min())), 0); x1 = min(int(np.ceil(sx.max())) + 1, self.W)
                y0 = max(int(np.floor(sy.min())), 0); y1 = min(int(np.ceil(sy.max())) + 1, self.H)
                if x1 <= x0 or y1 <= y0:
                    continue
                xs = np.arange(x0, x1) + 0.5
                ys = np.arange(y0, y1) + 0.5
                gx, gy = np.meshgrid(xs, ys)
                w0 = ((sx[1]-sx[0])*(gy-sy[0]) - (sy[1]-sy[0])*(gx-sx[0])) / area
                w1 = ((sx[2]-sx[1])*(gy-sy[1]) - (sy[2]-sy[1])*(gx-sx[1])) / area
                # barycentric: b0 for v0 etc.
                b2, b0 = w0, w1
                b1 = 1.0 - b0 - b2
                m = (b0 >= -1e-6) & (b1 >= -1e-6) & (b2 >= -1e-6)
                if not m.any():
                    continue
                iz = b0 / z[0] + b1 / z[1] + b2 / z[2]
                zz = 1.0 / np.maximum(iz, 1e-9)
                sub = self.depth[y0:y1, x0:x1]
                m &= (zz < sub)
                if not m.any():
                    continue
                bm0, bm1, bm2 = b0[m], b1[m], b2[m]
                zm = zz[m]
                u = (bm0 * uvc[0, 0] / z[0] + bm1 * uvc[1, 0] / z[1] + bm2 * uvc[2, 0] / z[2]) * zm
                v = (bm0 * uvc[0, 1] / z[0] + bm1 * uvc[1, 1] / z[1] + bm2 * uvc[2, 1] / z[2]) * zm
                lg = bm0 * lic[0] + bm1 * lic[1] + bm2 * lic[2]
                ui = np.mod((u * tw).astype(np.int64), tw)
                vi = np.mod((v * th).astype(np.int64), th)
                c = tex[vi, ui]
                if not mesh.unlit:
                    c = c * lg[:, None]
                    # distance fog toward the murky Q2 black
                    ff = np.clip((zm - self.fog_start) / (self.fog_end - self.fog_start), 0, 1)[:, None]
                    c = c * (1 - ff) + self.fog_color * ff
                if mesh.alpha_tex is not None:
                    a = mesh.alpha_tex[vi, ui][:, None] * mesh.alpha
                    dst = self.color[y0:y1, x0:x1][m]
                    c = c * a + dst * (1 - a)
                    tgt = self.color[y0:y1, x0:x1]; tgt[m] = c; self.color[y0:y1, x0:x1] = tgt
                elif mesh.alpha < 1.0:
                    dst = self.color[y0:y1, x0:x1][m]
                    c = c * mesh.alpha + dst * (1 - mesh.alpha)
                    tgt = self.color[y0:y1, x0:x1]; tgt[m] = c; self.color[y0:y1, x0:x1] = tgt
                else:
                    tgt = self.color[y0:y1, x0:x1]; tgt[m] = c; self.color[y0:y1, x0:x1] = tgt
                    sub[m] = zm; self.depth[y0:y1, x0:x1] = sub

    def resolve(self):
        """Downsample the supersampled buffer."""
        s = self.ss
        c = np.clip(self.color, 0, 1)
        if s == 1:
            return c
        H, W = self.H // s, self.W // s
        return c[:H*s, :W*s].reshape(H, s, W, s, 3).mean(axis=(1, 3))
