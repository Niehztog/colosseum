"""Turn Quake II assets into renderable meshes."""
import numpy as np, os, glob
from q2lib import PakFS, read_pcx, read_wal, MD2, load_anorms
from render import Mesh, rot_z, norm

# Retail assets and the Gladiator SDK's normal table, read from checkouts
# beside this repository.  $Q2DATA and $ANORMS override, as they do in tools/,
# so nothing here resolves only on the machine it was written on.
_ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "..")

BASE = os.environ.get("Q2DATA", os.path.join(_ROOT, "yquake2", "release", "baseq2")).rstrip("/") + "/"
ANORMS = load_anorms(os.environ.get("ANORMS", os.path.join(_ROOT, "gladq2_src", "anorms.h")))

_fs = None
def fs():
    global _fs
    if _fs is None:
        _fs = PakFS(sorted(glob.glob(BASE + "*.pak")))
    return _fs

_pal = None
def palette():
    """The Quake II global palette."""
    global _pal
    if _pal is None:
        _, p = read_pcx(fs().read("pics/colormap.pcx"))
        _pal = p.astype(np.float64) / 255.0
    return _pal

_texcache = {}
def wal_tex(name):
    """textures/xxx/yyy.wal -> (H,W,3) float RGB."""
    if name in _texcache: return _texcache[name]
    px, _ = read_wal(fs().read(name))
    t = palette()[px]
    _texcache[name] = t
    return t

def pcx_tex(path):
    if path in _texcache: return _texcache[path]
    px, pal = read_pcx(open(path, 'rb').read())
    p = (pal.astype(np.float64) / 255.0) if pal is not None else palette()
    t = p[px]
    _texcache[path] = t
    return t


def pak_pcx_tex(name):
    """Skin PCX read out of the pak archives (models keep their own palette)."""
    key = 'pak:' + name
    if key in _texcache:
        return _texcache[key]
    px, pal = read_pcx(fs().read(name))
    p = (pal.astype(np.float64) / 255.0) if pal is not None else palette()
    t = p[px]
    _texcache[key] = t
    return t


def md2_mesh(md2, frame, skin_tex, origin=(0,0,0), yaw=0.0, scale=1.0,
             light_dir=(-0.4,0.6,0.9), ambient=0.32, shade=0.85, tint=(1,1,1),
             gain=1.0, lum=None):
    """Build a Mesh from one MD2 frame, lit the way the game lights models."""
    fr = md2.frames[frame]
    xyz = fr['xyz'] * scale
    R = rot_z(yaw)
    xyz = xyz @ R.T + np.asarray(origin, float)
    nrm = ANORMS[fr['ni']] @ R.T
    ld = norm(light_dir)
    d = np.clip(nrm @ ld, 0, 1)
    inten = ambient + shade * d
    v = xyz[md2.tri_xyz]                       # (T,3,3)
    uv = md2.st[md2.tri_st]                    # (T,3,2)
    li = inten[md2.tri_xyz]                    # (T,3)
    li = li[:, :, None] * np.asarray(tint, float)[None, None, :]
    li = li.mean(axis=2) if False else li      # keep per-channel below
    # per-channel light: fold tint into texture instead
    # Headroom above 1.0 on purpose: the weapon and item skins are very dark
    # 8-bit art, and clamping here would throw away the brightening.
    if lum:
        # Q2 model skins are very dark 8-bit art (weapon skins average ~0.08),
        # so normalise to a target mean rather than guessing a fixed multiplier.
        cur = float(np.clip(skin_tex, 0, 1).mean())
        gain = gain * (lum / max(cur, 1e-3))
    tex = skin_tex * np.asarray(tint, float)[None, None, :] * gain
    return Mesh(v, uv, inten[md2.tri_xyz], np.clip(tex, 0, 4))


def quad(p0, p1, p2, p3, uv_scale=1.0, uv0=(0,0), light=1.0, tex=None, uvs=None):
    """Two triangles from four corners; UVs derived from world size unless given."""
    p = [np.asarray(x, float) for x in (p0,p1,p2,p3)]
    if uvs is None:
        e1 = np.linalg.norm(p[1]-p[0]); e2 = np.linalg.norm(p[3]-p[0])
        u = e1/uv_scale; v = e2/uv_scale
        uvs = [(uv0[0],uv0[1]),(uv0[0]+u,uv0[1]),(uv0[0]+u,uv0[1]+v),(uv0[0],uv0[1]+v)]
    L = light if np.iterable(light) else [light]*4
    V=[[p[0],p[1],p[2]],[p[0],p[2],p[3]]]
    U=[[uvs[0],uvs[1],uvs[2]],[uvs[0],uvs[2],uvs[3]]]
    Li=[[L[0],L[1],L[2]],[L[0],L[2],L[3]]]
    return V,U,Li


class MeshBuilder:
    """Accumulate quads/tris that share one texture."""
    def __init__(self, tex):
        self.tex=tex; self.V=[]; self.U=[]; self.L=[]
    def add_quad(self,*a,**kw):
        V,U,L = quad(*a,**kw); self.V+=V; self.U+=U; self.L+=L
    def add_tri(self,p,uv,l):
        self.V.append(p); self.U.append(uv); self.L.append(l)
    def mesh(self, unlit=False):
        if not self.V: return None
        return Mesh(np.array(self.V), np.array(self.U), np.array(self.L), self.tex, unlit=unlit)
