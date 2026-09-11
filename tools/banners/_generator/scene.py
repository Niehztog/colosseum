"""Assemble and render one Colosseum banner scene."""
import numpy as np
from q2lib import MD2
from q2scene import BASE, md2_mesh, pcx_tex, wal_tex, MeshBuilder, pak_pcx_tex, fs
from render import Renderer
from colosseum import Arena, Sky, Lighting

P = BASE + "players/"
_md2 = {}
def md2(path):
    if path not in _md2:
        _md2[path] = MD2(open(P + path, 'rb').read())
    return _md2[path]


DEFAULT = dict(
    W=1600, H=500, ss=2, fov=90,
    radius=790, bays=28, podium=95, trim_h=20, trim_proj=16,
    tex_floor='textures/e2u2/floor2_8.wal',
    tex_wall='textures/e3u1/brick1_1.wal',
    tex_pier='textures/e2u2/rock25_1.wal',
    tex_trim='textures/e3u1/dfloor1_1.wal',
    uv_pier=96, void_f=0.10, torch_p=0.22, floor_rings=7,
    tiers=[dict(h=215, open=0.62, plinth=26, rise=58, depth=60, seg=9),
           dict(h=195, open=0.60, plinth=22, rise=48, depth=54, seg=9)],
    attic=dict(h=120, broken=0.0),
    broken=0.0,
    sky='unit1', sky_gain=1.0, sky_yaw=0.0,
    eye=(-620, 30, 58), target=(500, -20, 300),
    sun=(-0.45, 0.35, 0.82), ambient=0.30, sun_i=0.78, seed=3,
    fog_start=700, fog_end=4200, fog_color=(0.10, 0.08, 0.07),
    fighter_scale=1.30, fighter_lum=0.235, fighter_amb=0.42, fighter_shade=0.95,
    weapon_lum=0.250, weapon_amb=0.60, weapon_shade=0.80,
    prop_lum=0.225, prop_amb=0.55, prop_shade=0.85,
)


def ground_at(c, sx_frac, sy_frac, z=0.0):
    """World point on the plane z=`z` that lands at a given screen fraction.

    Placing fighters this way means a camera change can never push them out of
    frame: you say where on the banner the feet go, the distance follows.
    """
    from render import look_at
    M, eye = look_at(c['eye'], c['target'])
    right, up, fwd = M[0], M[1], M[2]
    W, H = c['W'], c['H']
    focal = (W / 2.0) / np.tan(np.radians(c['fov']) / 2.0)
    dx = sx_frac * W - W / 2.0
    dy = H / 2.0 - sy_frac * H
    d = right * dx + up * dy + fwd * focal
    d = d / np.linalg.norm(d)
    if abs(d[2]) < 1e-6:
        return np.array(c['target'], float)
    t = (z - eye[2]) / d[2]
    return eye + t * d


def resolve_positions(c):
    """Turn any screen-space placement into world coordinates."""
    for group in ('fighters', 'props'):
        for e in c.get(group) or []:
            if 'screen' in e:
                sx, sy = e['screen']
                p = ground_at(c, sx, sy)
                dflt = c.get('fighter_scale', 1.0) if group == 'fighters' else 1.0
                sc = e.get('scale', dflt)
                # the 24-unit Q2 player origin sits above the feet, so it has to
                # scale with the model or a bigger fighter sinks into the sand
                e['pos'] = (p[0], p[1], e.get('z', 24.0) * sc + e.get('lift', 0.0))
    return c


def build(cfg):
    c = dict(DEFAULT); c.update(cfg)
    c['fighters'] = [dict(f) for f in c.get('fighters', [])]
    c['props'] = [dict(p) for p in c.get('props', [])]
    resolve_positions(c)
    r = Renderer(c['W'], c['H'], fov=c['fov'], ss=c['ss'])
    r.fog_start = c['fog_start']; r.fog_end = c['fog_end']
    r.fog_color = np.asarray(c['fog_color'], float)
    r.set_camera(eye=c['eye'], target=c['target'])
    r.clear()
    lit = Lighting(sun=c['sun'], ambient=c['ambient'], sun_i=c['sun_i'], seed=c['seed'], cam=c['eye'])
    litf = Lighting(sun=c['sun'], ambient=c['ambient'] * 1.05, sun_i=c['sun_i'],
                    seed=c['seed'], cam=c['eye'], lm_amp=c.get('floor_lm', 0.055))
    ar = Arena(c, lit, litf)
    meshes = ar.build()
    for m in meshes:
        r.draw(m)
    return r, c, ar


_shadow = {}
def shadow_tex():
    """Soft round drop shadow used as a floor decal under each fighter."""
    if 'a' not in _shadow:
        n = 96
        y, x = np.mgrid[0:n, 0:n]
        d = np.sqrt(((x - n / 2) / (n / 2)) ** 2 + ((y - n / 2) / (n / 2)) ** 2)
        a = np.clip(1.0 - d, 0, 1) ** 1.5
        _shadow['a'] = a.astype(np.float64)
        _shadow['t'] = np.zeros((n, n, 3))
    return _shadow['t'], _shadow['a']


def add_shadows(r, c):
    from render import Mesh
    t, a = shadow_tex()
    for f in c['fighters']:
        p = np.asarray(f['pos'], float)
        rad = f.get('shadow_r', 34.0) * f.get('scale', 1.0)
        z = 1.2
        V, U, L = [], [], []
        c0 = (p[0] - rad, p[1] - rad, z); c1 = (p[0] + rad, p[1] - rad, z)
        c2 = (p[0] + rad, p[1] + rad, z); c3 = (p[0] - rad, p[1] + rad, z)
        uv = [(0, 0), (1, 0), (1, 1), (0, 1)]
        V += [[c0, c1, c2], [c0, c2, c3]]
        U += [[uv[0], uv[1], uv[2]], [uv[0], uv[2], uv[3]]]
        L += [[1, 1, 1], [1, 1, 1]]
        r.draw(Mesh(np.array(V), np.array(U), np.array(L), t, unlit=True,
                    alpha=f.get('shadow_a', 0.62), alpha_tex=a))


_pak_md2 = {}
def pak_md2(path):
    if path not in _pak_md2:
        _pak_md2[path] = MD2(fs().read(path))
    return _pak_md2[path]


def add_props(r, c):
    """Weapon and item pickups standing on the sand, as a DM arena would have.
    Returns the coloured glows any of them cast (quad damage, mega health)."""
    glows = []
    for p in c.get('props', []):
        if p.get('glow'):
            sc = p.get('scale', 1.0)
            # Keep the glow on the model: the offset tracks scale so it doesn't
            # sit at the item's feet. The radius grows sub-linearly (sqrt) --
            # scaling it 1:1 with a large item floods the whole frame.
            gz = p.get('glow_z', 8.0 * sc)
            glows.append((np.asarray(p['pos'], float) + np.array([0, 0, gz]),
                          p.get('glow_i', 0.8), p['glow'],
                          p.get('glow_r', float(np.sqrt(sc)))))
        m = pak_md2(p['model'])
        skin = p.get('skin') or m.skins[0]
        tex = pak_pcx_tex(skin)
        r.draw(md2_mesh(m, p.get('frame', 0), tex, origin=p['pos'],
                        yaw=p.get('yaw', 0.0), scale=p.get('scale', 1.0),
                        light_dir=p.get('light', (-0.5, 0.4, 0.75)),
                        ambient=p.get('amb', c.get('prop_amb', 0.55)),
                        shade=p.get('shade', c.get('prop_shade', 0.85)),
                        lum=p.get('lum', c.get('prop_lum', 0.225))))
    return glows


def weapon_skin(wep, model):
    """The skin a w_*.md2 actually declares.

    Each held weapon names its own texture -- w_railgun.md2 asks for
    models/weapons/g_rail/skin.pcx -- and they are not interchangeable: pasting
    the generic players/<model>/weapon.pcx over all of them maps the UVs onto
    the wrong art, which is why the weapons read as dark smears.
    """
    for name in (wep.skins[0] if wep.skins else None,):
        if not name:
            continue
        try:
            return pak_pcx_tex(name)
        except Exception:
            pass
        try:
            return pcx_tex(BASE + name)
        except Exception:
            pass
    return pcx_tex(P + model + "/weapon.pcx")


def add_fighters(r, c):
    """Two RA2-skinned players with real Quake II weapons, in the foreground."""
    out = []
    for f in c['fighters']:
        body = md2(f['model'] + "/tris.md2")
        wep = md2(f['model'] + "/" + f['weapon'] + ".md2")
        skin = pcx_tex(P + f['model'] + "/" + f['skin'] + ".pcx")
        wskin = weapon_skin(wep, f['model'])
        fi = body.frame_named(f['frame'])
        fw = min(fi, wep.num_frames - 1)
        sc = f.get('scale', c.get('fighter_scale', 1.0))
        common = dict(origin=f['pos'], yaw=f['yaw'], scale=sc,
                      light_dir=f.get('light', (-0.5, 0.4, 0.75)))
        r.draw(md2_mesh(body, fi, skin, **common,
                        ambient=f.get('amb', c.get('fighter_amb', 0.42)),
                        shade=f.get('shade', c.get('fighter_shade', 0.95)),
                        lum=f.get('lum', c.get('fighter_lum', 0.235))))
        # the weapon skin is darker than the player skin and reads as a black
        # blob at banner size, so it is lit flatter and brighter than the body
        r.draw(md2_mesh(wep, fw, wskin, **common,
                        ambient=f.get('w_amb', c.get('weapon_amb', 0.60)),
                        shade=f.get('w_shade', c.get('weapon_shade', 0.80)),
                        lum=f.get('w_lum', c.get('weapon_lum', 0.265))))
    return out
