"""Beveled metal title lettering, filled with a real Quake II texture."""
import numpy as np
from PIL import Image, ImageDraw, ImageFont, ImageFilter

FONTS = {
    'black': "/usr/share/fonts/truetype/roboto/unhinted/RobotoTTF/Roboto-Black.ttf",
    'cond': "/usr/share/fonts/truetype/roboto/unhinted/RobotoCondensed-Bold.ttf",
    'bold': "/usr/share/fonts/truetype/roboto/unhinted/RobotoTTF/Roboto-Bold.ttf",
    'dejavu': "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
}


def text_mask(text, font_path, px, tracking=0.0, scale=4, xscale=1.0, pad=40):
    """High-res antialiased coverage mask for a tracked string."""
    f = ImageFont.truetype(font_path, int(px * scale))
    widths = [f.getbbox(ch)[2] - f.getbbox(ch)[0] if ch != ' ' else int(px * scale * 0.3)
              for ch in text]
    advs = [f.getlength(ch) for ch in text]
    tr = tracking * px * scale
    total = sum(advs) + tr * (len(text) - 1)
    asc, desc = f.getmetrics()
    W = int(total + pad * scale * 2)
    H = int((asc + desc) + pad * scale * 2)
    im = Image.new('L', (W, H), 0)
    d = ImageDraw.Draw(im)
    x = pad * scale
    for i, ch in enumerate(text):
        d.text((x, pad * scale), ch, font=f, fill=255)
        x += advs[i] + tr
    bb = im.getbbox()
    im = im.crop((bb[0] - 6 * scale, bb[1] - 6 * scale, bb[2] + 6 * scale, bb[3] + 6 * scale))
    w, h = im.size
    out = im.resize((max(1, int(w / scale * xscale)), max(1, int(h / scale))), Image.LANCZOS)
    return np.asarray(out, np.float32) / 255.0


def _blur(a, r):
    if r <= 0:
        return a
    im = Image.fromarray((np.clip(a, 0, 1) * 255).astype(np.uint8))
    return np.asarray(im.filter(ImageFilter.GaussianBlur(r)), np.float32) / 255.0


def _dilate(a, r):
    im = Image.fromarray((np.clip(a, 0, 1) * 255).astype(np.uint8))
    k = int(r) * 2 + 1
    return np.asarray(im.filter(ImageFilter.MaxFilter(min(k, 9))), np.float32) / 255.0


def bevel_metal(mask, tex, bevel=5.0, light=(-0.55, -0.72, 0.42), tint=(1, 1, 1),
                ambient=0.42, diff=0.95, spec=0.75, spec_p=28.0, tex_scale=1.0,
                inner_dark=0.30, tex_lum=0.62, sharp=9.0, rim=(0.0, 0.0, 0.0), rim_a=0.0):
    """Emboss `mask` into a lit metal plate textured with `tex`."""
    H, W = mask.shape
    h = _blur(mask, bevel)
    h = h * mask                       # bevel dies at the outer edge
    h = _blur(h, max(1.0, bevel * 0.4))
    gy, gx = np.gradient(h)
    s = sharp
    nx, ny, nz = -gx * s, -gy * s, np.ones_like(h)
    ln = np.sqrt(nx * nx + ny * ny + nz * nz)
    nx, ny, nz = nx / ln, ny / ln, nz / ln
    L = np.asarray(light, float); L = L / np.linalg.norm(L)
    d = np.clip(nx * L[0] + ny * L[1] + nz * L[2], 0, 1)
    # blinn specular against a viewer straight on
    hv = L + np.array([0, 0, 1.0]); hv /= np.linalg.norm(hv)
    sp = np.clip(nx * hv[0] + ny * hv[1] + nz * hv[2], 0, 1) ** spec_p
    shade = ambient + diff * d + spec * sp
    # tile the Quake II texture across the letters
    th, tw = tex.shape[:2]
    ys, xs = np.mgrid[0:H, 0:W]
    base = tex[np.mod((ys / tex_scale).astype(int), th), np.mod((xs / tex_scale).astype(int), tw)]
    # Quake II wall textures are dark; lift them to a usable type weight
    if tex_lum:
        cur = float(np.clip(base, 0, 1).mean())
        base = np.clip(base * (tex_lum / max(cur, 1e-3)), 0, 1.6)
    col = base * shade[..., None] * np.asarray(tint, float)
    # darken the very core so the bevel reads
    col *= (1.0 - inner_dark * np.clip(h * 1.1 - 0.1, 0, 1))[..., None]
    if rim_a:
        # warm light catching the top edge, tying the type to the sky
        edge = np.clip(-gy * sharp, 0, None)
        edge = edge / max(float(edge.max()), 1e-6)
        col = col + np.asarray(rim, float) * (edge ** 1.4)[..., None] * rim_a
    return np.clip(col, 0, 4), mask


def render_title(text, px, tex, font='black', tracking=0.06, xscale=1.0,
                 outline=3, outline_col=(0.05, 0.04, 0.04),
                 shadow=(6, 8), shadow_blur=7, shadow_a=0.72,
                 glow=None, glow_a=0.0, **kw):
    """-> (rgb float, alpha float) ready to composite."""
    m = text_mask(text, FONTS[font], px, tracking=tracking, xscale=xscale)
    pad = int(max(outline * 3 + 8, shadow_blur * 3 + max(abs(shadow[0]), abs(shadow[1])) + 8, 24))
    m = np.pad(m, ((pad, pad), (pad, pad)))
    col, mask = bevel_metal(m, tex, **kw)

    H, W = m.shape
    rgb = np.zeros((H, W, 3), np.float32)
    a = np.zeros((H, W), np.float32)

    if glow is not None and glow_a > 0:
        g = _blur(m, 16) * glow_a
        rgb += np.asarray(glow, float) * g[..., None]
        a = np.clip(a + g, 0, 1)

    if shadow_a > 0:
        sm = np.roll(np.roll(m, shadow[1], axis=0), shadow[0], axis=1)
        sm = _blur(sm, shadow_blur) * shadow_a
        rgb = rgb * (1 - sm[..., None])
        a = np.clip(a + sm, 0, 1)

    if outline > 0:
        om = m
        for _ in range(int(outline)):
            om = _dilate(om, 1)
        om = np.clip(om, 0, 1)
        rgb = rgb * (1 - om[..., None]) + np.asarray(outline_col, float) * om[..., None]
        a = np.clip(a + om, 0, 1)

    rgb = rgb * (1 - m[..., None]) + col * m[..., None]
    a = np.clip(a + m, 0, 1)
    return rgb, a


def paste(img, rgb, alpha, x, y, opacity=1.0):
    """Alpha-composite a title onto the banner at top-left (x,y), clipped."""
    H, W = img.shape[:2]
    h, w = alpha.shape
    x0, y0 = int(x), int(y)
    sx0, sy0 = max(0, -x0), max(0, -y0)
    dx0, dy0 = max(0, x0), max(0, y0)
    ww = min(w - sx0, W - dx0); hh = min(h - sy0, H - dy0)
    if ww <= 0 or hh <= 0:
        return img
    a = (alpha[sy0:sy0 + hh, sx0:sx0 + ww] * opacity)[..., None]
    c = rgb[sy0:sy0 + hh, sx0:sx0 + ww]
    img[dy0:dy0 + hh, dx0:dx0 + ww] = img[dy0:dy0 + hh, dx0:dx0 + ww] * (1 - a) + c * a
    return img
