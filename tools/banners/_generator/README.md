# Banner generator

Renders the `tools/banners/*.png` candidates from the **retail Quake II assets** -- no external art. Everything on screen is game data:

* `players/male` + `players/female` MD2s in the Rocket Arena 2 team skins (`r2blue.pcx`, `r2red.pcx`), with the `w_*.md2` weapon models drawn on the same frame index the engine uses, so they sit in the hands.
* `models/weapons/g_*` and `models/items/*` pickups on the sand.
* `textures/**.wal` walls, `env/unit*` skyboxes, and the palette from `pics/colormap.pcx` (also used for the 8-bit variant's quantisation).

**No image is committed.** The renders are reproducible, so this directory holds the renderer and nothing else; `.github/banner.png` carries the one copy the `README.md` displays, which is `02-low-angle` rendered here and renamed on the way -- the variant names are this renderer's, and the published name should not have to change when a different shot is chosen. Re-rendering a deleted banner reproduces it **byte for byte** -- every random source here is seeded with a constant, nothing dates the output, and the same pixels re-encode to the same file -- provided the assets and the Pillow version are the same ones.

## Assets

Three inputs, none of them in this repository:

| what | where | note |
|---|---|---|
| retail `pak0`--`pak3` and the loose `players/` tree | `$Q2DATA`, default `yquake2/release/baseq2/` beside this repository | the game's own data |
| `r2blue.pcx` / `r2red.pcx` under `players/male` and `players/female` | the same `$Q2DATA` tree | **Rocket Arena 2's team skins**, which the fighters wear in every shot; they ship loose in RA2's own client package and are not in any retail pak |
| id's vertex-normal table | `$ANORMS`, default `gladq2_src/anorms.h` | one header, read for the MD2 normals |

Without the two RA2 skins the render stops at the first fighter with `FileNotFoundError`; nothing else here is optional either, so a machine that cannot resolve all three cannot regenerate the banners at all.

## Running

Needs `numpy` and `Pillow` only:

    pip install --target=./pylibs numpy Pillow
    PYTHONPATH=./pylibs python3 -c "
    import sys, copy; sys.path.insert(0,'.')
    import banner, variants
    for v in variants.V:
        banner.render(copy.deepcopy(v), f\"../colosseum-banner-{v['name']}.png\", scale=1.0)
    "

`scale=` multiplies the output size (`scale=2` gives 3200x1200). Set `scene['ss']=1` for fast draft renders.

## Layout

`q2lib.py` PAK/PCX/WAL/MD2 readers ; `render.py` z-buffered software rasteriser ; `colosseum.py` the parametric arcade ring and skybox ; `scene.py` scene assembly and screen-space placement ; `logo.py` bevelled metal lettering ; `post.py` glows, bloom, grade, palette ; `variants.py` the ten shots.

Fighters and props are placed by **screen fraction** -- `S(0.20, 0.93)` means "feet 20% across, 93% down" -- so changing the camera can never push them out of frame.
