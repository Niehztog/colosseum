"""Render the Colosseum project banner. Everything on screen comes from the
retail Quake II assets: MD2 player models, .wal wall textures, env/ skyboxes
and the game's own 256-colour palette."""
import sys, os, time
import numpy as np
from PIL import Image
from scene import build, add_fighters, add_shadows, add_props
from colosseum import Sky
from q2scene import palette, wal_tex
import post, logo

# The banners live one level up, beside this generator's directory.
OUT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

TITLE_TEX = {
    'stone': 'textures/e2u2/rock25_1.wal',
    'brick': 'textures/e3u1/brick1_1.wal',
    'steel': 'textures/e2u1/mmtl19_1.wal',
    'rust':  'textures/e2u3/rmtl37_1.wal',
}


def render(v, outfile, scale=1.0):
    t0 = time.time()
    cfg = dict(v['scene'])
    cfg['W'] = int(cfg.get('W', 1600) * scale)
    cfg['H'] = int(cfg.get('H', 600) * scale)
    r, c, ar = build(cfg)
    prop_glows = add_props(r, c)
    add_shadows(r, c)
    add_fighters(r, c)
    Sky(c['sky'], c['sky_gain']).render(r, c['sky_yaw'])
    img = np.clip(r.resolve(), 0, 1).astype(np.float32)

    g = v.get('glow', {})
    img = post.add_glows(img, r, ar.glows,
                         color=g.get('color', (1.0, 0.62, 0.26)),
                         base=g.get('base', 170.0) * scale)
    for pos, inten, col, grad in prop_glows:
        img = post.add_glows(img, r, [(pos, inten)], color=col,
                             base=g.get('prop_base', 95.0) * grad * scale)
    b = v.get('bloom', (0.72, 0.42, 10))
    img = post.bloom(img, b[0], b[1], b[2] * scale)
    img = post.grade(img, **v.get('grade', {}))
    img = post.vignette(img, **v.get('vignette', {}))
    img = post.bottom_fade(img, **v.get('floor_fade', {}))

    # ---- title
    T = v['title']
    tex = wal_tex(TITLE_TEX[T.get('tex', 'stone')])
    rgb, a = logo.render_title(
        T.get('text', 'COLOSSEUM'), int(T.get('px', 96) * scale), tex,
        font=T.get('font', 'black'), tracking=T.get('tracking', 0.06),
        xscale=T.get('xscale', 1.0), tint=T.get('tint', (1.0, 0.97, 0.92)),
        bevel=T.get('bevel', 4.0) * scale, sharp=T.get('sharp', 13.0),
        tex_lum=T.get('lum', 0.70), tex_scale=T.get('tex_scale', 1.0),
        ambient=0.40, diff=1.05, spec=0.9, spec_p=22,
        outline=int(T.get('outline', 3) * scale),
        shadow=(int(6 * scale), int(8 * scale)), shadow_blur=int(7 * scale),
        rim=T.get('rim', (1.0, 0.55, 0.2)), rim_a=T.get('rim_a', 0.55))
    H, W = img.shape[:2]
    th, tw = a.shape
    ax = T.get('align', 'center')
    x = {'center': (W - tw) / 2, 'left': W * 0.055, 'right': W - tw - W * 0.055}[ax]
    x += T.get('dx', 0) * scale
    y = T.get('y', 0.10) * H + T.get('dy', 0) * scale
    img = logo.paste(img, rgb, a, x, y, T.get('opacity', 1.0))

    # ---- tagline
    S = v.get('sub')
    if S:
        stex = wal_tex(TITLE_TEX[S.get('tex', 'steel')])
        srgb, sa = logo.render_title(
            S['text'], int(S.get('px', 22) * scale), stex,
            font=S.get('font', 'cond'), tracking=S.get('tracking', 0.30),
            tint=S.get('tint', (1.0, 0.93, 0.82)), bevel=1.6 * scale, sharp=8.0,
            tex_lum=S.get('lum', 0.78), ambient=0.55, diff=0.9, spec=0.5, spec_p=18,
            outline=int(1 * scale), shadow=(int(2 * scale), int(3 * scale)),
            shadow_blur=int(3 * scale), rim_a=0.0)
        sh, sw = sa.shape
        sx = {'center': (W - sw) / 2, 'left': W * 0.055, 'right': W - sw - W * 0.055}[S.get('align', ax)]
        sx += S.get('dx', 0) * scale
        img = logo.paste(img, srgb, sa, sx, y + th + S.get('gap', -6) * scale, S.get('opacity', 0.95))

    img = post.grain(img, v.get('grain', 0.015))
    if v.get('quantize'):
        img = post.q2_quantize(img, palette(), dither=v.get('dither', True))

    os.makedirs(OUT, exist_ok=True)
    Image.fromarray((np.clip(img, 0, 1) * 255).astype(np.uint8)).save(outfile)
    print(f"  {os.path.basename(outfile):34s} {img.shape[1]}x{img.shape[0]}  {time.time()-t0:5.1f}s")
    return img


# --------------------------------------------------------------- fighter kit
def F(model, skin, weapon, frame, pos, yaw, **kw):
    d = dict(model=model, skin=skin, weapon=weapon, frame=frame, yaw=yaw)
    if isinstance(pos, S):
        d['screen'] = tuple(pos)
    else:
        d['pos'] = pos
    d.update(kw); return d

class S(tuple):
    """Marker: place by screen fraction (x, y of the feet) instead of world xyz."""
    __slots__ = ()
    def __new__(cls, x, y): return super().__new__(cls, (x, y))


def prop(model, pos, yaw=0.0, **kw):
    d = dict(model=model, yaw=yaw)
    if isinstance(pos, S):
        d['screen'] = tuple(pos)
    else:
        d['pos'] = pos
    d.update(kw); return d

W_RAIL   = 'models/weapons/g_rail/tris.md2'
W_ROCKET = 'models/weapons/g_rocket/tris.md2'
W_HYPER  = 'models/weapons/g_hyperb/tris.md2'
W_SSG    = 'models/weapons/g_shotg2/tris.md2'
I_QUAD   = 'models/items/quaddama/tris.md2'
I_MEGA   = 'models/items/mega_h/tris.md2'
I_ARMOR  = 'models/items/armor/body/tris.md2'
I_COMBAT = 'models/items/armor/combat/tris.md2'


def blue(weapon, frame, pos, yaw, **kw):
    return F('male', 'r2blue', weapon, frame, pos, yaw, **kw)

def red(weapon, frame, pos, yaw, **kw):
    return F('female', 'r2red', weapon, frame, pos, yaw, **kw)
