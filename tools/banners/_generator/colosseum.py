"""Parametric colosseum arena built from Quake II textures."""
import numpy as np
from q2scene import wal_tex, MeshBuilder, fs, palette
from q2lib import read_pcx
from render import Mesh, norm

# ------------------------------------------------------------------ lighting
def lightmap(p, seed=3, amp=0.11):
    """Blotchy variation standing in for a Q2 lightmap. Several octaves, so it
    reads as surface grime rather than one big stain across the arena floor."""
    x, y, z = p[0], p[1], p[2]
    r = seed * 1.7
    v = (np.sin(x * 0.0121 + r) * np.cos(y * 0.0107 - r) * 0.55 +
         np.sin(z * 0.0163 + r * 2) * 0.55 +
         np.sin(x * 0.0345 - r) * np.cos(y * 0.0298 + r) * 0.40 +
         np.sin((x + y) * 0.0631 + r * 3) * 0.22)
    return 1.0 + amp * v


class Lighting:
    def __init__(self, sun=(-0.45, 0.35, 0.82), ambient=0.30, sun_i=0.78,
                 warm=(1.0, 0.82, 0.62), seed=3, fill=0.10, cam=None, lm_amp=0.11):
        self.sun = norm(sun); self.ambient = ambient; self.sun_i = sun_i
        self.warm = np.asarray(warm, float); self.seed = seed; self.fill = fill
        self.cam = None if cam is None else np.asarray(cam, float)
        self.lm_amp = lm_amp

    def at(self, p, n):
        if self.cam is not None and np.dot(n, self.cam - np.asarray(p, float)) < 0:
            n = -n
        d = max(float(np.dot(n, self.sun)), 0.0)
        up = max(float(n[2]), 0.0)
        v = self.ambient + self.sun_i * d + self.fill * up
        return v * lightmap(np.asarray(p, float), self.seed, self.lm_amp)

    __call__ = at


# ------------------------------------------------------------------ helpers
def dark_tex(tex, f):
    return np.clip(tex * f, 0, 1)


def add_face(mb, pts, lit, uvs=None, uv_scale=64.0):
    """Quad from 4 world points; per-corner light from `lit(p,n)`."""
    p = [np.asarray(x, float) for x in pts]
    # A wedge that comes to a point (the centre ring of the arena floor) has
    # p0 == p3, so the obvious cross product vanishes; fall back to another
    # pair rather than dropping the face and leaving a hole in the floor.
    n = np.cross(p[1] - p[0], p[3] - p[0])
    if np.linalg.norm(n) < 1e-9:
        n = np.cross(p[1] - p[0], p[2] - p[0])
    if np.linalg.norm(n) < 1e-9:
        n = np.cross(p[2] - p[0], p[3] - p[0])
    ln = np.linalg.norm(n)
    if ln < 1e-9:
        return
    n = n / ln
    if uvs is None:
        e1 = np.linalg.norm(p[1] - p[0]); e2 = np.linalg.norm(p[3] - p[0])
        uvs = [(0, 0), (e1 / uv_scale, 0), (e1 / uv_scale, e2 / uv_scale), (0, e2 / uv_scale)]
    L = [lit(q, n) for q in p]
    mb.V += [[p[0], p[1], p[2]], [p[0], p[2], p[3]]]
    mb.U += [[uvs[0], uvs[1], uvs[2]], [uvs[0], uvs[2], uvs[3]]]
    mb.L += [[L[0], L[1], L[2]], [L[0], L[2], L[3]]]


# ------------------------------------------------------------------ arena
class Arena:
    """A ring of stacked arcades: the Colosseum, in Quake II geometry."""

    def __init__(self, cfg, lit, lit_floor=None):
        self.c = cfg
        self.lit = lit
        self.lit_floor = lit_floor or lit
        self.meshes = []
        self.glows = []

    def build(self, cam_yaw=None):
        c = self.c
        R = c['radius']
        nbays = c['bays']
        step = 2 * np.pi / nbays

        floor_t = wal_tex(c['tex_floor'])
        wall_t = wal_tex(c['tex_wall'])
        pier_t = wal_tex(c['tex_pier'])
        trim_t = wal_tex(c['tex_trim'])
        void_t = dark_tex(wal_tex(c['tex_wall']), c.get('void_f', 0.10))

        mb_floor = MeshBuilder(floor_t)
        mb_wall = MeshBuilder(wall_t)
        mb_pier = MeshBuilder(pier_t)
        mb_trim = MeshBuilder(trim_t)
        mb_void = MeshBuilder(void_t)

        # ---- arena floor (fan of wedges so lighting varies across it)
        RINGS = c.get('floor_rings', 7)
        GR = c.get('ground_radius', R)      # extend past the ring for exterior views
        for i in range(nbays):
            a0, a1 = i * step, (i + 1) * step
            for k in range(RINGS):
                r0 = GR * k / RINGS
                r1 = GR * (k + 1) / RINGS
                p0 = (r0 * np.cos(a0), r0 * np.sin(a0), 0)
                p1 = (r1 * np.cos(a0), r1 * np.sin(a0), 0)
                p2 = (r1 * np.cos(a1), r1 * np.sin(a1), 0)
                p3 = (r0 * np.cos(a1), r0 * np.sin(a1), 0)
                uvs = [(p0[0] / 96, p0[1] / 96), (p1[0] / 96, p1[1] / 96),
                       (p2[0] / 96, p2[1] / 96), (p3[0] / 96, p3[1] / 96)]
                add_face(mb_floor, [p0, p1, p2, p3], self.lit_floor, uvs=uvs)

        # ---- podium (the barrier wall around the sand)
        z = 0.0
        pod = c['podium']
        for i in range(nbays):
            a0, a1 = i * step, (i + 1) * step
            q0 = (R * np.cos(a0), R * np.sin(a0), 0)
            q1 = (R * np.cos(a1), R * np.sin(a1), 0)
            add_face(mb_pier, [q1, q0, (q0[0], q0[1], pod), (q1[0], q1[1], pod)],
                     self.lit, uv_scale=c.get('uv_pier', 64))
        self._cornice(mb_trim, R, pod, c['trim_h'], c['trim_proj'], nbays, step)
        z = pod + c['trim_h']

        # ---- stacked arcades
        for ti, tier in enumerate(c['tiers']):
            self._arcade(mb_wall, mb_pier, mb_void, mb_trim, R, z, tier, nbays, step, ti)
            z += tier['h']
            self._cornice(mb_trim, R, z, c['trim_h'], c['trim_proj'], nbays, step,
                          broken=c.get('broken', 0.0), seed=ti)
            z += c['trim_h']

        # ---- attic storey: solid wall, pilasters, small openings
        if c.get('attic'):
            self._attic(mb_wall, mb_pier, mb_void, R, z, c['attic'], nbays, step)

        for mb in (mb_floor, mb_pier, mb_wall, mb_trim):
            m = mb.mesh()
            if m is not None:
                self.meshes.append(m)
        mv = mb_void.mesh(unlit=True)
        if mv is not None:
            self.meshes.append(mv)
        return self.meshes

    # ---------------------------------------------------------------- pieces
    def _cornice(self, mb, R, z, h, proj, nbays, step, broken=0.0, seed=0):
        rng = np.random.RandomState(1000 + seed)
        for i in range(nbays):
            if broken and rng.rand() < broken:
                continue
            a0, a1 = i * step, (i + 1) * step
            Ri = R - proj
            o0 = (R * np.cos(a0), R * np.sin(a0)); o1 = (R * np.cos(a1), R * np.sin(a1))
            i0 = (Ri * np.cos(a0), Ri * np.sin(a0)); i1 = (Ri * np.cos(a1), Ri * np.sin(a1))
            # inward-facing band
            add_face(mb, [(i1[0], i1[1], z), (i0[0], i0[1], z),
                          (i0[0], i0[1], z + h), (i1[0], i1[1], z + h)], self.lit, uv_scale=48)
            # underside
            add_face(mb, [(i0[0], i0[1], z), (i1[0], i1[1], z),
                          (o1[0], o1[1], z), (o0[0], o0[1], z)], self.lit, uv_scale=48)
            # top ledge
            add_face(mb, [(o0[0], o0[1], z + h), (o1[0], o1[1], z + h),
                          (i1[0], i1[1], z + h), (i0[0], i0[1], z + h)], self.lit, uv_scale=48)

    def _arcade(self, mb_wall, mb_pier, mb_void, mb_trim, R, z0, tier, nbays, step, ti):
        h = tier['h']
        z1 = z0 + h
        of = tier.get('open', 0.60)
        plinth = tier.get('plinth', 26.0)
        depth = tier.get('depth', 58.0)
        SEG = tier.get('seg', 9)
        uvp = self.c.get('uv_pier', 64)
        rng = np.random.RandomState(77 + ti)

        for i in range(nbays):
            a = (i + 0.5) * step
            ca, sa = np.cos(a), np.sin(a)
            t = np.array([-sa, ca, 0.0])          # tangent
            nin = np.array([-ca, -sa, 0.0])       # inward (toward arena centre)
            Rc = R * np.cos(step / 2)             # chord seat: endpoints land on R
            C = np.array([Rc * ca, Rc * sa, 0.0])
            W = 2 * R * np.sin(step / 2)
            wo = W * of / 2.0
            zs = z0 + plinth + tier.get('rise', 60.0)   # springline
            # cap the arch so it fits the storey
            if zs + wo > z1 - 8:
                zs = z1 - 8 - wo

            rs = self.c.get('recess', 1.0)
            def F(s, zz, d=0.0):                 # point on the bay face
                return C + t * s + np.array([0, 0, zz]) - nin * d * rs

            # piers left / right
            add_face(mb_pier, [F(-W/2, z0), F(-wo, z0), F(-wo, z1), F(-W/2, z1)],
                     self.lit, uv_scale=uvp)
            add_face(mb_pier, [F(wo, z0), F(W/2, z0), F(W/2, z1), F(wo, z1)],
                     self.lit, uv_scale=uvp)
            # plinth beneath the opening
            add_face(mb_pier, [F(-wo, z0), F(wo, z0), F(wo, z0 + plinth), F(-wo, z0 + plinth)],
                     self.lit, uv_scale=uvp)

            # spandrel above the arch + arch intrados + jambs
            for k in range(SEG):
                s0 = -wo + 2 * wo * k / SEG
                s1 = -wo + 2 * wo * (k + 1) / SEG
                za0 = zs + np.sqrt(max(wo * wo - s0 * s0, 0.0))
                za1 = zs + np.sqrt(max(wo * wo - s1 * s1, 0.0))
                add_face(mb_wall, [F(s0, za0), F(s1, za1), F(s1, z1), F(s0, z1)],
                         self.lit, uv_scale=uvp)
                # intrados (underside of the arch ring), going back into the wall
                add_face(mb_trim, [F(s1, za1), F(s0, za0), F(s0, za0, depth), F(s1, za1, depth)],
                         self.lit, uv_scale=40)
            # jambs (vertical reveals)
            add_face(mb_trim, [F(-wo, z0 + plinth), F(-wo, zs), F(-wo, zs, depth), F(-wo, z0 + plinth, depth)],
                     self.lit, uv_scale=40)
            add_face(mb_trim, [F(wo, zs), F(wo, z0 + plinth), F(wo, z0 + plinth, depth), F(wo, zs, depth)],
                     self.lit, uv_scale=40)

            # the void behind the opening
            zt = zs + wo
            add_face(mb_void, [F(-wo, z0, depth), F(wo, z0, depth), F(wo, zt + 6, depth), F(-wo, zt + 6, depth)],
                     lambda p, n: 1.0, uv_scale=uvp)
            # occasional torch glow deep in a bay
            if rng.rand() < self.c.get('torch_p', 0.22):
                self.glows.append((C + nin * -depth * 0.6 + np.array([0, 0, z0 + plinth + 46]),
                                   rng.uniform(0.55, 1.0)))

    def _attic(self, mb_wall, mb_pier, mb_void, R, z0, at, nbays, step):
        h = at['h']
        z1 = z0 + h
        rng = np.random.RandomState(5)
        broken = at.get('broken', 0.0)
        uvp = self.c.get('uv_pier', 64)
        for i in range(nbays):
            if broken and rng.rand() < broken:
                continue
            a = (i + 0.5) * step
            ca, sa = np.cos(a), np.sin(a)
            t = np.array([-sa, ca, 0.0]); nin = np.array([-ca, -sa, 0.0])
            Rc = R * np.cos(step / 2)
            C = np.array([Rc * ca, Rc * sa, 0.0])
            W = 2 * R * np.sin(step / 2)
            zz1 = z1 - (rng.rand() * h * 0.5 if broken else 0.0)

            rs = self.c.get('recess', 1.0)
            def F(s, z, d=0.0):
                return C + t * s + np.array([0, 0, z]) - nin * d * rs
            add_face(mb_wall, [F(-W/2, z0), F(W/2, z0), F(W/2, zz1), F(-W/2, zz1)],
                     self.lit, uv_scale=uvp)
            # pilaster strip
            add_face(mb_pier, [F(-W/2 - 3, z0), F(-W/2 + 14, z0), F(-W/2 + 14, zz1), F(-W/2 - 3, zz1)],
                     self.lit, uv_scale=uvp)
            # small square window
            if zz1 > z0 + h * 0.7 and rng.rand() < 0.55:
                ww = W * 0.16; zc = z0 + h * 0.45
                add_face(mb_void, [F(-ww, zc, 20), F(ww, zc, 20), F(ww, zc + ww * 1.7, 20), F(-ww, zc + ww * 1.7, 20)],
                         lambda p, n: 1.0, uv_scale=uvp)


# ------------------------------------------------------------------ sky
class Sky:
    """Quake II skybox, sampled as a true cubemap.

    Face-to-axis mapping matches how the pinned env/ images stitch together:
    ft=+X, rt=-Y, bk=-X, lf=+Y, up=+Z.
    """
    FACES = [
        ('ft', (1, 0, 0), (0, 1, 0), (0, 0, 1)),
        ('lf', (0, 1, 0), (-1, 0, 0), (0, 0, 1)),
        ('bk', (-1, 0, 0), (0, -1, 0), (0, 0, 1)),
        ('rt', (0, -1, 0), (1, 0, 0), (0, 0, 1)),
        ('up', (0, 0, 1), (1, 0, 0), (0, 1, 0)),
    ]

    def __init__(self, name='unit1', gain=1.0, yaw_bias=0.0):
        f = fs()
        self.img = {}
        for side, _, _, _ in self.FACES:
            px, pal = read_pcx(f.read(f'env/{name}_{side}.pcx'))
            p = (pal.astype(np.float64) / 255.0) if pal is not None else palette()
            self.img[side] = np.clip(p[px] * gain, 0, 1)
        self.yaw_bias = yaw_bias

    def render(self, rend, yaw_off=0.0):
        H, W = rend.H, rend.W
        ys, xs = np.mgrid[0:H, 0:W]
        cx, cy = W / 2.0, H / 2.0
        fo = rend.focal
        dx = (xs + 0.5 - cx) / fo
        dy = -(ys + 0.5 - cy) / fo
        M = rend.M
        d = (M[0][None, None, :] * dx[..., None] +
             M[1][None, None, :] * dy[..., None] +
             M[2][None, None, :])
        d /= np.linalg.norm(d, axis=2, keepdims=True)
        a = np.radians(self.yaw_bias + yaw_off)
        if a:
            ca, sa = np.cos(a), np.sin(a)
            d = np.stack([d[..., 0] * ca - d[..., 1] * sa,
                          d[..., 0] * sa + d[..., 1] * ca, d[..., 2]], axis=2)
        mask = rend.depth > 1e29
        out = rend.color
        for side, fwd, right, up in self.FACES:
            fwd = np.asarray(fwd, float); right = np.asarray(right, float); up = np.asarray(up, float)
            dp = d @ fwd
            u = (d @ right) / np.where(np.abs(dp) < 1e-9, 1e-9, dp)
            v = (d @ up) / np.where(np.abs(dp) < 1e-9, 1e-9, dp)
            sel = mask & (dp > 0) & (np.abs(u) <= 1.0001) & (np.abs(v) <= 1.0001)
            if not sel.any():
                continue
            im = self.img[side]
            h, w = im.shape[:2]
            ui = np.clip(((u + 1) * 0.5 * w).astype(np.int64), 0, w - 1)
            vi = np.clip(((1 - (v + 1) * 0.5) * h).astype(np.int64), 0, h - 1)
            out[sel] = im[vi[sel], ui[sel]]
