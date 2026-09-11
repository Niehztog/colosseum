"""Post pass: torch glows, bloom, grade, vignette, optional 8-bit palette."""
import numpy as np
from PIL import Image


def project(rend, p):
    """World point -> (x,y,z) in resolved (non-supersampled) pixels."""
    v = (np.asarray(p, float) - rend.eye) @ rend.M.T
    if v[2] <= 1e-3:
        return None
    s = rend.ss
    x = (rend.W / 2 + rend.focal * v[0] / v[2]) / s
    y = (rend.H / 2 - rend.focal * v[1] / v[2]) / s
    return x, y, v[2]


def add_glows(img, rend, glows, color=(1.0, 0.62, 0.26), base=170.0):
    """Warm additive blobs where torches sit deep in the arcade bays."""
    H, W = img.shape[:2]
    ys, xs = np.mgrid[0:H, 0:W]
    col = np.asarray(color, float)
    for p, inten in glows:
        pr = project(rend, p)
        if pr is None:
            continue
        x, y, z = pr
        if x < -300 or x > W + 300 or y < -300 or y > H + 300:
            continue
        rad = base * 260.0 / max(z, 60.0)
        if rad < 2:
            continue
        d2 = (xs - x) ** 2 + (ys - y) ** 2
        f = np.exp(-d2 / (2 * (rad * 0.5) ** 2)) * inten
        img += f[..., None] * col * 0.85
    return img


def bloom(img, thresh=0.72, amount=0.5, radius=9):
    from PIL import ImageFilter
    b = np.clip(img - thresh, 0, None) / max(1e-6, (1 - thresh))
    bi = Image.fromarray((np.clip(b, 0, 1) * 255).astype(np.uint8))
    bi = bi.filter(ImageFilter.GaussianBlur(radius))
    bb = np.asarray(bi, np.float32) / 255.0
    return img + bb * amount


def grade(img, lift=0.0, gain=1.0, gamma=1.0, sat=1.0, tint=(1, 1, 1)):
    x = np.clip(img, 0, 4)
    x = (x * gain + lift)
    x = np.power(np.clip(x, 0, 4), gamma)
    if sat != 1.0:
        l = x @ np.array([0.299, 0.587, 0.114])
        x = l[..., None] + (x - l[..., None]) * sat
    return np.clip(x * np.asarray(tint, float), 0, 1)


def vignette(img, strength=0.42, power=2.2):
    H, W = img.shape[:2]
    ys, xs = np.mgrid[0:H, 0:W]
    nx = (xs - W / 2) / (W / 2); ny = (ys - H / 2) / (H / 2)
    r = np.sqrt(nx ** 2 * 0.86 + ny ** 2)
    v = 1.0 - strength * np.clip(r, 0, 1.4) ** power
    return img * v[..., None]


def bottom_fade(img, strength=0.34, frac=0.34, power=1.7):
    """Sink the bottom edge into shadow so the open arena floor reads as
    foreground rather than as a bright empty band."""
    if strength <= 0:
        return img
    H = img.shape[0]
    y = np.arange(H, dtype=np.float32)
    start = H * (1.0 - frac)
    t = np.clip((y - start) / max(H - start, 1.0), 0, 1) ** power
    return img * (1.0 - strength * t)[:, None, None]


def grain(img, amount=0.016, seed=1):
    rng = np.random.RandomState(seed)
    n = rng.normal(0, 1, img.shape[:2])[..., None]
    return np.clip(img + n * amount, 0, 1)


def q2_quantize(img, pal, dither=True, ncol=224):
    """Snap to the real Quake II 256-colour palette.

    Only the first 224 entries are used: the tail of the Q2 palette is the
    fullbright range (saturated greens/blues for HUD and effects) and a
    nearest-colour match will happily pick those for a bright sky, which
    stripes the gradient with colours that were never in the scene.
    """
    pi = Image.new('P', (1, 1))
    flat = (np.clip(pal[:ncol], 0, 1) * 255).astype(np.uint8).reshape(-1).tolist()
    pi.putpalette(flat + [0] * (768 - len(flat)))
    im = Image.fromarray((np.clip(img, 0, 1) * 255).astype(np.uint8))
    q = im.quantize(palette=pi, dither=Image.FLOYDSTEINBERG if dither else Image.NONE)
    return np.asarray(q.convert('RGB'), np.float32) / 255.0
