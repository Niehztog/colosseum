"""Ten Colosseum banner variants.

Every pixel comes from retail Quake II data: male/female MD2 player models in
the Rocket Arena 2 team skins, w_* weapon models on the same frame indices,
.wal wall textures, env/ skyboxes, and the game's own palette.
"""
from banner import (blue, red, prop, S, W_RAIL, W_ROCKET, W_HYPER, W_SSG,
                    I_QUAD, I_MEGA, I_ARMOR, I_COMBAT)

BASE_SCENE = dict(
    W=1600, H=600, ss=2, fov=95,
    radius=1000, bays=32, podium=95, trim_h=20, trim_proj=16,
    tiers=[dict(h=215, open=0.62, plinth=26, rise=58, depth=60, seg=9),
           dict(h=195, open=0.60, plinth=22, rise=48, depth=54, seg=9)],
    attic=dict(h=105, broken=0.0),
    eye=(-820, 0, 42), target=(1000, 0, 285), floor_lm=0.055)

SUB = "DM  ·  DUEL  ·  TDM  ·  CTF  ·  ARENA"

# pickups scattered on the sand, the way a DM arena lays them out
def arena_props(quad=5.20, quad_pos=(110, 10, 24)):
    """Pickups on the sand.

    `quad` / `quad_pos` are solved per variant so the quad damage icon renders
    at the same on-screen size AND the same screen position as variant 10 --
    matching size alone is not enough, because each camera puts the item's base
    at a different height and it rides up into the tagline.
    """
    return [
        prop(I_QUAD,   quad_pos, 25, scale=quad, lum=0.135,
             glow=(0.30, 0.42, 1.0), glow_i=0.95),
        prop(W_RAIL,   (-130, -430, 20), 105, scale=1.30),
        prop(W_ROCKET, ( -95,  410, 20), -35, scale=1.30),
        prop(I_ARMOR,  ( 360, -300, 22), 60, scale=1.30),
        prop(I_MEGA,   ( 415,  285, 22), 0, scale=1.30, glow=(1.0, 0.30, 0.22), glow_i=0.45),
        prop(I_COMBAT, (-560,  470, 22), 200, scale=1.25),
    ]

V = []

# 1 -- sunset duel, the reference shot
V.append(dict(name="01-sunset-duel", scene=dict(BASE_SCENE,
    sky='unit1', sky_yaw=-38, sun=(-0.42, 0.42, 0.80), ambient=0.30, sun_i=0.80,
    props=arena_props(3.69, (-325, 7, 24)),
    fighters=[blue('w_rlauncher', 'run4',    S(0.795, 0.93), 143),
              red ('w_railgun',   'attack1', S(0.205, 0.93), 217)]),
    title=dict(px=104, tex='stone', y=0.055, tracking=0.07),
    sub=dict(text=SUB, px=21, gap=2),
    grade=dict(gain=1.10, gamma=0.94, sat=1.07)))

# 2 -- low on the sand, looking up
V.append(dict(name="02-low-angle", scene=dict(BASE_SCENE, fov=100,
    eye=(-770, 40, 28), target=(1000, -30, 330),
    sky='unit2', sky_yaw=105, sun=(-0.30, 0.55, 0.76), ambient=0.27, sun_i=0.85,
    props=arena_props(3.33, (-358, 30, 24)),
    fighters=[blue('w_railgun',      'attack1', S(0.805, 0.93), 150),
              red ('w_rlauncher',    'run2',    S(0.195, 0.92), 208)]),
    title=dict(px=110, tex='stone', y=0.055, tracking=0.05),
    sub=dict(text=SUB, px=21, gap=2),
    grade=dict(gain=1.12, gamma=0.92, sat=1.10), bloom=(0.74, 0.42, 11)))

# 3 -- rocket jump
V.append(dict(name="03-rocket-jump", scene=dict(BASE_SCENE,
    eye=(-800, -30, 58), target=(1000, 30, 285),
    sky='unit4', sky_yaw=140, sun=(-0.55, 0.28, 0.78), ambient=0.28, sun_i=0.82,
    props=arena_props(4.68, (-172, 0, 24)),
    fighters=[red ('w_rlauncher', 'jump2',   S(0.545, 0.99), 206, lift=104, shadow_a=0.26, shadow_r=26),
              blue('w_sshotgun',  'crattak1',S(0.815, 0.94), 146)]),
    title=dict(px=98, tex='rust', y=0.06, align='left', tracking=0.06,
               tint=(1.0, 0.92, 0.80)),
    sub=dict(text=SUB, px=20, align='left', gap=2),
    grade=dict(gain=1.08, gamma=0.95, sat=1.05)))

# 4 -- night arena, torch-lit
V.append(dict(name="04-night-torches", scene=dict(BASE_SCENE,
    sky='unit7', sky_yaw=90, sun=(-0.40, 0.30, 0.86), ambient=0.17, sun_i=0.42,
    torch_p=0.55, fog_start=520, fog_end=3000, fog_color=(0.05, 0.045, 0.05),
    props=arena_props(3.69, (-325, 7, 24)),
    fighters=[blue('w_chaingun',  'attack3', S(0.795, 0.93), 145),
              red ('w_railgun',   'stand20', S(0.205, 0.93), 214)]),
    title=dict(px=104, tex='steel', y=0.055, tracking=0.07,
               tint=(0.90, 0.95, 1.05), rim=(1.0, 0.6, 0.25), rim_a=0.7),
    sub=dict(text=SUB, px=21, gap=2),
    glow=dict(color=(1.0, 0.55, 0.20), base=200.0),
    bloom=(0.62, 0.60, 13), grade=dict(gain=1.16, gamma=0.90, sat=1.04),
    vignette=dict(strength=0.52)))

# 5 -- the ruin: broken attic, more sky
V.append(dict(name="05-ruin", scene=dict(BASE_SCENE,
    eye=(-840, 0, 66), target=(1000, 0, 300), broken=0.30,
    attic=dict(h=140, broken=0.45),
    sky='unit1', sky_yaw=168, sun=(-0.50, 0.40, 0.76), ambient=0.32, sun_i=0.80,
    props=arena_props(5.40, (-116, 10, 24)),
    fighters=[blue('w_railgun',  'taunt1', S(0.795, 0.93), 148),
              red ('w_glauncher','run4',   S(0.205, 0.93), 212)]),
    title=dict(px=104, tex='stone', font='cond', xscale=1.14, y=0.055, tracking=0.11,
               tint=(1.02, 0.98, 0.92)),
    sub=dict(text=SUB, px=21, gap=2),
    grade=dict(gain=1.10, gamma=0.94, sat=1.08)))

# 6 -- bright brick, high sun
V.append(dict(name="06-brick-day", scene=dict(BASE_SCENE,
    tex_wall='textures/e3u1/brick1_2.wal', tex_pier='textures/e3u1/brick1_1.wal',
    tex_trim='textures/e2u2/rock25_1.wal',
    sky='unit5', sky_yaw=-60, sun=(-0.35, 0.30, 0.89), ambient=0.38, sun_i=0.82,
    torch_p=0.12, props=arena_props(3.69, (-325, 7, 24)),
    fighters=[blue('w_railgun',   'run2',    S(0.795, 0.93), 144),
              red ('w_rlauncher', 'attack1', S(0.205, 0.93), 216)]),
    title=dict(px=104, tex='stone', y=0.055, tracking=0.07),
    sub=dict(text=SUB, px=21, gap=2),
    grade=dict(gain=1.06, gamma=0.97, sat=1.05), vignette=dict(strength=0.34)))

# 7 -- the facade from outside
V.append(dict(name="07-facade", scene=dict(BASE_SCENE, fov=95,
    recess=-1.0, ground_radius=4600, floor_rings=11,
    podium=60, trim_h=16,
    tiers=[dict(h=180, open=0.62, plinth=22, rise=48, depth=56, seg=9),
           dict(h=165, open=0.60, plinth=18, rise=40, depth=50, seg=9)],
    attic=None, floor_lm=0.07,
    eye=(-1607, -905, 122), target=(0, 0, 214),
    sky='unit1', sky_yaw=232, sun=(-0.72, -0.36, 0.60), ambient=0.34, sun_i=0.86,
    props=[prop(W_RAIL, (-1180, -545, 20), 100, scale=1.3),
           prop(I_QUAD, (-1080, -760, 24), 20, scale=5.2, lum=0.135,
                glow=(0.30, 0.42, 1.0), glow_i=0.9)],
    fighters=[blue('w_railgun',   'run4',    S(0.715, 0.935), 190),
              red ('w_rlauncher', 'attack4', S(0.285, 0.965), 232)]),
    title=dict(px=100, tex='stone', y=0.055, tracking=0.07),
    sub=dict(text=SUB, px=21, gap=2),
    grade=dict(gain=1.10, gamma=0.94, sat=1.06)))

# 8 -- rendered the way the game did: 8-bit palette, dithered
V.append(dict(name="08-8bit", scene=dict(BASE_SCENE, fov=90,
    eye=(-800, 20, 48), target=(1000, -20, 272),
    sky='unit2', sky_yaw=196, sun=(-0.45, 0.38, 0.80), ambient=0.31, sun_i=0.80,
    props=arena_props(4.32, (-169, 14, 24)),
    fighters=[blue('w_rlauncher','attack2', S(0.795, 0.93), 147),
              red ('w_railgun',  'run4',    S(0.205, 0.93), 213)]),
    title=dict(px=104, tex='stone', y=0.055, tracking=0.07),
    sub=dict(text=SUB, px=21, gap=2),
    quantize=True, dither=False, grain=0.0,
    grade=dict(gain=1.12, gamma=0.93, sat=1.08)))

# 9 -- wide sweep of the ring, type carries the frame
V.append(dict(name="09-wide-ring", scene=dict(BASE_SCENE, fov=110,
    eye=(-720, 0, 84), target=(1000, 0, 320),
    sky='unit9', sky_yaw=60, sun=(-0.48, 0.36, 0.80), ambient=0.30, sun_i=0.78,
    props=arena_props(5.55, (-145, 11, 24)),
    fighters=[blue('w_railgun',     'run4',    S(0.80, 0.93), 140),
              red ('w_rlauncher',   'attack3', S(0.20, 0.93), 220)]),
    title=dict(px=118, tex='stone', y=0.085, tracking=0.10),
    sub=dict(text=SUB, px=22, gap=4),
    grade=dict(gain=1.10, gamma=0.94, sat=1.06), vignette=dict(strength=0.46)))

# 10 -- close duel, fighters dominate
V.append(dict(name="10-close-duel", scene=dict(BASE_SCENE, fov=86,
    eye=(-700, 0, 62), target=(1000, 0, 250),
    sky='unit1', sky_yaw=-38, sun=(-0.42, 0.42, 0.80), ambient=0.31, sun_i=0.80,
    props=arena_props(5.20),
    fighters=[blue('w_railgun',   'attack1', S(0.80, 0.94), 142),
              red ('w_rlauncher', 'attack4', S(0.20, 0.94), 218)]),
    title=dict(px=98, tex='stone', y=0.055, tracking=0.08),
    sub=dict(text=SUB, px=20, gap=2),
    grade=dict(gain=1.10, gamma=0.94, sat=1.07)))
