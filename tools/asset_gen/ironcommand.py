"""Iron Command — atomic-age 1950s sci-fi set (chrome / teal / coral / cream).
Sizes match the current cube footprints + colliders in src/game/Factory.cpp."""
import math
from agen import Asset, palette, rot_e

P = {
    "cream":  ((0.93, 0.89, 0.78), 0.0, 0.5),
    "chrome": ((0.78, 0.80, 0.84), 0.9, 0.25),
    "steel":  ((0.48, 0.51, 0.56), 0.7, 0.4),
    "dark":   ((0.16, 0.17, 0.20), 0.3, 0.6),
    "teal":   ((0.16, 0.60, 0.60), 0.1, 0.4),
    "coral":  ((0.95, 0.45, 0.38), 0.0, 0.5),
    "glow":   ((0.55, 1.00, 0.95), 0.0, 0.2),
    "fire":   ((1.00, 0.62, 0.22), 0.0, 0.3),
    "glass":  ((0.60, 0.86, 0.95), 0.0, 0.1),
    "rubber": ((0.10, 0.10, 0.11), 0.0, 0.9),
    "red":    ((0.78, 0.16, 0.20), 0.1, 0.4),
    "redglow":((1.00, 0.40, 0.32), 0.0, 0.2),
    "brass":  ((0.85, 0.66, 0.30), 0.8, 0.35),
    "ore":    ((0.45, 0.36, 0.30), 0.0, 0.9),
    "purple": ((0.55, 0.42, 0.80), 0.1, 0.4),
}
M = palette("ic", P)

# team colour swaps: rival assets reuse the builders with red in place of teal
HOME  = {"team": "teal", "accent": "coral", "light": "glow"}
RIVAL = {"team": "red",  "accent": "dark",  "light": "redglow"}

def fins(a, n, r, y, h, m, depth=0.08, length=0.9, phase=0.0):
    for i in range(n):
        ang = phase + 360.0 * i / n
        s, c = math.sin(math.radians(ang)), math.cos(math.radians(ang))
        a.box((depth, h, length), (s * r, y, c * r), m, rot=rot_e(ry=ang))

def hub(name, t):
    """Domed rocket silo. Collider 8 x 5 x 8; beacon tip ~6.4."""
    a = Asset(name, M)
    a.cyl(4.0, 0.4, (0, 0.2, 0), "dark", seg=8, smooth=False)          # octagonal pad
    a.cyl(3.3, 2.6, (0, 1.7, 0), "cream", seg=32)                      # silo drum
    for y in (0.55, 2.85):
        a.torus(3.32, 0.12, (0, y, 0), "chrome", seg=32)                # chrome bands
    a.torus(3.32, 0.18, (0, 1.7, 0), t["team"], seg=32)                # team stripe
    a.dome(3.3, (0, 3.0, 0), "chrome", scale=(1, 0.55, 1), seg=32)     # dome
    a.cyl(0.55, 1.2, (0, 4.4, 0), "cream", seg=16, r2=0.35)            # rocket nose
    a.cyl(0.35, 0.9, (0, 5.4, 0), t["accent"], seg=16, r2=0.02)
    a.sphere(0.28, (0, 6.0, 0), t["light"])                            # beacon
    fins(a, 4, 0.55, 4.1, 0.9, t["accent"], length=0.6, phase=45)
    # door + portholes at the front (+Z)
    a.box((1.6, 2.0, 0.5), (0, 1.4, 3.15), "steel")
    a.box((1.2, 1.7, 0.1), (0, 1.3, 3.42), "dark")
    for ang in (-50, 50, 130, 230):
        s, c = math.sin(math.radians(ang)), math.cos(math.radians(ang))
        a.cyl(0.38, 0.2, (s * 3.3, 2.1, c * 3.3), "chrome", rot=rot_e(rx=90, ry=ang))
        a.cyl(0.28, 0.24, (s * 3.3, 2.1, c * 3.3), t["light"], rot=rot_e(rx=90, ry=ang))
    # landing legs
    for ang in (0, 90, 180, 270):
        s, c = math.sin(math.radians(ang + 45)), math.cos(math.radians(ang + 45))
        a.box((0.35, 1.8, 0.35), (s * 3.55, 0.9, c * 3.55), "chrome", rot=rot_e(ry=ang + 45, rx=-12))
    return a

def miner():
    """Drill rig on ore. ~2.4 x 2.8."""
    a = Asset("ic_miner", M)
    a.cyl(1.2, 0.3, (0, 0.15, 0), "dark", seg=6, smooth=False)
    a.cyl(0.95, 1.3, (0, 0.95, 0), "cream", seg=24)
    a.torus(0.97, 0.08, (0, 1.6, 0), "chrome")
    for x in (-0.55, 0.55):                                          # derrick legs
        for z in (-0.55, 0.55):
            a.box((0.12, 2.0, 0.12), (x * 0.8, 2.0, z * 0.8), "chrome")
    a.cyl(0.55, 0.15, (0, 2.95, 0), "teal")
    a.cyl(0.25, 1.6, (0, 2.2, 0), "steel", seg=12)                   # drill shaft
    a.cyl(0.42, 0.7, (0, 0.35, 0), "coral", seg=12, r2=0.05)         # bit (peeks out)
    a.cyl(0.35, 0.7, (1.05, 0.75, 0), "teal", seg=16)                # side tank
    a.dome(0.35, (1.05, 1.1, 0), "teal")
    a.sphere(0.12, (0, 3.15, 0), "glow")
    return a

def smelter():
    """Round furnace with a chimney and glowing hatch. ~2.8 x 2.9."""
    a = Asset("ic_smelter", M)
    a.box((2.8, 0.3, 2.8), (0, 0.15, 0), "dark", bevel=0.05)
    a.cyl(1.25, 1.7, (0, 1.15, 0), "steel", seg=24, r2=1.0)
    a.torus(1.2, 0.09, (0, 0.5, 0), "chrome")
    a.dome(1.0, (0, 2.0, 0), "cream", scale=(1, 0.5, 1))
    a.cyl(0.55, 0.2, (0, 1.0, 1.15), "chrome", rot=rot_e(rx=90))     # hatch ring (front)
    a.cyl(0.42, 0.25, (0, 1.0, 1.17), "fire", rot=rot_e(rx=90))
    a.cyl(0.28, 1.6, (0.55, 2.6, -0.4), "cream", seg=12)              # chimney
    a.torus(0.29, 0.06, (0.55, 3.1, -0.4), "coral", seg=16, sides=6)
    a.cyl(0.36, 0.2, (0.55, 3.45, -0.4), "coral", seg=12)
    return a

def constructor():
    """Stamping press. ~3.2 x 2.0 x 2.2."""
    a = Asset("ic_constructor", M)
    a.box((3.2, 0.9, 2.2), (0, 0.45, 0), "cream", bevel=0.12)
    a.box((3.25, 0.12, 2.25), (0, 0.85, 0), "teal")
    for x in (-1.2, 1.2):
        a.box((0.35, 1.2, 0.5), (x, 1.5, 0), "chrome", bevel=0.05)
    a.box((2.8, 0.35, 0.8), (0, 1.9, 0), "steel", bevel=0.06)         # press head
    a.cyl(0.18, 0.6, (0, 1.5, 0), "chrome", seg=12)                   # ram
    a.box((1.4, 0.06, 1.2), (0, 0.93, 0), "dark")                     # die bed
    a.cyl(0.18, 0.12, (-0.9, 0.7, 1.1), "coral", rot=rot_e(rx=90))   # gauges
    a.cyl(0.18, 0.12, (-0.4, 0.7, 1.1), "glow", rot=rot_e(rx=90))
    return a

def assembler():
    """Assembly bay with a robot arm. ~4.0 x 2.5 x 3.0."""
    a = Asset("ic_assembler", M)
    a.box((4.0, 1.2, 3.0), (0, 0.6, 0), "cream", bevel=0.15)
    a.box((4.05, 0.14, 3.05), (0, 1.1, 0), "coral")
    a.box((2.6, 0.08, 1.4), (0, 1.24, 0.4), "dark")                   # work bed
    a.cyl(0.35, 0.3, (-1.4, 1.35, -0.9), "chrome", seg=16)            # arm base
    a.box((0.22, 1.2, 0.22), (-1.4, 2.0, -0.9), "teal", rot=rot_e(rz=-10))
    a.sphere(0.18, (-1.3, 2.6, -0.9), "chrome")
    a.box((1.3, 0.18, 0.18), (-0.7, 2.55, -0.6), "teal", rot=rot_e(ry=-25, rz=-15))
    a.cyl(0.12, 0.35, (-0.1, 2.25, -0.35), "chrome", seg=8)           # tool head
    a.dome(0.6, (1.2, 1.2, -0.8), "glass", scale=(1, 0.9, 1))         # control bubble
    a.sphere(0.12, (1.2, 1.45, -0.8), "glow")
    return a

def barracks():
    """Quonset-hut robot hangar. ~7.0 x 3.4 x 8.4, door on +Z."""
    a = Asset("ic_barracks", M)
    a.clip_ground = True                                               # keep only the half above y=0
    a.box((7.2, 0.3, 8.6), (0, 0.15, 0), "dark")
    a.cyl(3.3, 8.0, (0, 0.0, 0), "chrome", rot=rot_e(rx=90), seg=32)  # half-barrel hut
    for z in (-3.0, -1.0, 1.0, 3.0):                                   # ribs
        a.torus(3.32, 0.07, (0, 0.0, z), "steel", rot=rot_e(rx=90), seg=32, sides=6)
    a.box((2.4, 2.4, 0.3), (0, 1.5, 4.05), "teal")                     # door frame (front)
    a.box((2.0, 2.1, 0.2), (0, 1.35, 4.15), "dark")
    a.box((0.3, 0.2, 8.0), (0, 3.33, 0), "coral")                      # roof spine
    a.cyl(0.2, 0.6, (0, 3.6, 2.5), "chrome", seg=12)                   # beacon mast
    a.sphere(0.3, (0, 4.0, 2.5), "glow")
    return a

def terminal():
    """Command console with a round CRT. ~2.4 x 1.8, screen facing +Z."""
    a = Asset("ic_terminal", M)
    a.box((2.2, 0.9, 1.6), (0, 0.45, -0.2), "cream", bevel=0.12)
    a.box((2.2, 0.15, 0.9), (0, 0.95, 0.35), "teal", rot=rot_e(rx=20))     # sloped desk
    for i, c in enumerate(("coral", "glow", "fire", "coral")):
        a.cyl(0.08, 0.08, (-0.6 + i * 0.4, 1.02, 0.45), c, rot=rot_e(rx=20), seg=10)
    a.box((1.5, 1.1, 0.8), (0, 1.5, -0.45), "steel", bevel=0.15)            # CRT housing
    a.cyl(0.45, 0.1, (0, 1.5, -0.03), "chrome", rot=rot_e(rx=90), seg=24)
    a.cyl(0.38, 0.12, (0, 1.5, 0.0), "glow", rot=rot_e(rx=90), seg=24)
    a.cyl(0.03, 0.5, (0.5, 2.25, -0.5), "chrome", seg=6)                    # antenna
    a.sphere(0.07, (0.5, 2.5, -0.5), "coral")
    return a

def robot(name, t):
    """Tin-can robot. ~0.8 wide x 1.4 tall, faces +Z."""
    a = Asset(name, M)
    for x in (-0.17, 0.17):
        a.cyl(0.09, 0.45, (x, 0.3, 0), "steel", seg=8)                 # legs
        a.box((0.2, 0.1, 0.3), (x, 0.05, 0.04), "dark", bevel=0.03)    # feet
    a.cyl(0.3, 0.6, (0, 0.8, 0), "chrome", seg=20)                     # body can
    a.torus(0.3, 0.04, (0, 0.55, 0), t["team"], seg=20, sides=6)
    a.torus(0.3, 0.04, (0, 1.05, 0), t["team"], seg=20, sides=6)
    a.cyl(0.11, 0.06, (0, 0.85, 0.3), t["light"], rot=rot_e(rx=90), seg=12)  # chest light
    for x in (-0.36, 0.36):                                            # arms (blaster on right)
        a.cyl(0.06, 0.5, (x, 0.82, 0.05), "steel", rot=rot_e(rx=70), seg=8)
    a.cyl(0.07, 0.25, (0.36, 0.73, 0.32), t["accent"], rot=rot_e(rx=90), seg=8)
    a.dome(0.22, (0, 1.12, 0), "chrome")                                # head dome
    a.box((0.3, 0.08, 0.06), (0, 1.22, 0.19), t["light"])               # visor
    a.cyl(0.015, 0.25, (0, 1.42, 0), "chrome", seg=6)                   # antenna
    a.sphere(0.04, (0, 1.56, 0), t["accent"])
    return a

def turret():
    """Ray turret. ~1.0 x 1.4, barrel to +Z."""
    a = Asset("ic_turret", M)
    a.cyl(0.5, 0.25, (0, 0.12, 0), "dark", seg=6, smooth=False)
    a.cyl(0.3, 0.7, (0, 0.6, 0), "cream", seg=16, r2=0.22)
    a.sphere(0.32, (0, 1.05, 0), "chrome")
    a.cyl(0.07, 0.6, (0, 1.05, 0.45), "steel", rot=rot_e(rx=90), seg=10)
    for z in (0.3, 0.45, 0.6):
        a.torus(0.11, 0.03, (0, 1.05, z), "teal", rot=rot_e(rx=90), seg=12, sides=6)
    a.sphere(0.07, (0, 1.05, 0.78), "glow")
    return a

def mine():
    """Flying-saucer proximity mine. ~1.0 x 0.45."""
    a = Asset("ic_mine", M)
    a.sphere(0.5, (0, 0.15, 0), "steel", scale=(1, 0.28, 1), seg=20)
    a.torus(0.47, 0.035, (0, 0.15, 0), "coral", seg=20, sides=6)
    a.dome(0.2, (0, 0.24, 0), "redglow")
    for i in range(6):
        ang = math.radians(60 * i)
        a.sphere(0.04, (math.sin(ang) * 0.36, 0.22, math.cos(ang) * 0.36), "fire", seg=6, rings=4)
    return a

def fence():
    """Energy fence panel. 2.6 wide x 2.1 tall x 0.32 deep."""
    a = Asset("ic_fence", M)
    for x in (-1.2, 1.2):
        a.cyl(0.14, 2.0, (x, 1.0, 0), "chrome", seg=10)
        a.sphere(0.18, (x, 2.05, 0), "teal")
        a.box((0.4, 0.2, 0.4), (x, 0.1, 0), "dark", bevel=0.04)
    a.box((2.3, 0.12, 0.12), (0, 0.25, 0), "steel")
    a.box((2.3, 0.12, 0.12), (0, 1.85, 0), "steel")
    for i in range(5):                                                  # glowing bars
        a.box((2.3, 0.05, 0.04), (0, 0.55 + i * 0.3, 0), "glow")
    return a

def tripwire():
    """Two posts and a laser line. Posts at x = ±1."""
    a = Asset("ic_tripwire", M)
    for x in (-1.0, 1.0):
        a.cyl(0.06, 0.8, (x, 0.4, 0), "chrome", seg=8)
        a.box((0.16, 0.1, 0.16), (x, 0.05, 0), "dark")
        a.sphere(0.08, (x, 0.78, 0), "coral", seg=10, rings=6)
    a.box((2.0, 0.025, 0.025), (0, 0.75, 0), "redglow")
    return a

def avatar():
    """Spaceman avatar for other players. ~1.85 tall, faces +Z."""
    a = Asset("ic_avatar", M)
    for x in (-0.13, 0.13):
        a.cyl(0.1, 0.8, (x, 0.45, 0), "cream", seg=10)
        a.box((0.2, 0.14, 0.32), (x, 0.07, 0.04), "dark", bevel=0.04)
    a.cyl(0.26, 0.7, (0, 1.2, 0), "cream", seg=16, r2=0.3)              # torso
    a.box((0.4, 0.3, 0.1), (0, 1.25, 0.26), "teal", bevel=0.03)          # chest pack
    a.cyl(0.05, 0.05, (0.08, 1.3, 0.32), "coral", rot=rot_e(rx=90), seg=8)
    a.box((0.42, 0.55, 0.22), (0, 1.25, -0.32), "chrome", bevel=0.06)    # air tanks
    for x in (-0.36, 0.36):
        a.cyl(0.07, 0.62, (x, 1.18, 0), "cream", seg=8, rot=rot_e(rz=8 if x > 0 else -8))
        a.sphere(0.08, (x * 1.05, 0.85, 0), "dark", seg=8, rings=6)
    a.torus(0.2, 0.05, (0, 1.57, 0), "chrome", seg=16, sides=6)          # collar
    a.sphere(0.27, (0, 1.78, 0), "glass", seg=20, rings=12)              # fishbowl helmet
    a.sphere(0.13, (0, 1.75, 0), "cream", seg=10, rings=8)               # head inside
    a.cyl(0.015, 0.2, (0.15, 2.05, 0), "chrome", seg=6)
    a.sphere(0.035, (0.15, 2.16, 0), "coral", seg=8, rings=6)
    return a

def items():
    """Belt items, ~0.32 across (cube placeholder size)."""
    out = []
    a = Asset("ic_item_ore", M); a.sphere(0.17, (0, 0.17, 0), "ore", scale=(1.1, 0.85, 0.95), seg=6, rings=4, smooth=False)
    a.sphere(0.06, (0.08, 0.26, 0.06), "teal", seg=5, rings=3, smooth=False); out.append(a)
    a = Asset("ic_item_ingot", M); a.box((0.22, 0.12, 0.36), (0, 0.06, 0), "brass", bevel=0.03); out.append(a)
    a = Asset("ic_item_screw", M); a.cyl(0.11, 0.06, (0, 0.27, 0), "chrome", seg=6, smooth=False)
    a.cyl(0.045, 0.24, (0, 0.12, 0), "steel", seg=8); out.append(a)
    a = Asset("ic_item_plate", M); a.box((0.36, 0.05, 0.36), (0, 0.025, 0), "steel", bevel=0.015)
    a.box((0.2, 0.02, 0.2), (0, 0.06, 0), "teal"); out.append(a)
    a = Asset("ic_item_rod", M); a.cyl(0.05, 0.38, (0, 0.06, 0), "chrome", rot=rot_e(rz=90), seg=8); out.append(a)
    a = Asset("ic_item_part", M); a.box((0.24, 0.24, 0.24), (0, 0.12, 0), "cream", bevel=0.04)
    a.cyl(0.07, 0.28, (0, 0.12, 0), "coral", rot=rot_e(rx=90), seg=8)
    a.cyl(0.07, 0.28, (0, 0.12, 0), "coral", rot=rot_e(rz=90), seg=8); out.append(a)
    a = Asset("ic_item_robot", M); a.cyl(0.11, 0.2, (0, 0.1, 0), "chrome", seg=12)
    a.dome(0.09, (0, 0.2, 0), "chrome"); a.box((0.12, 0.03, 0.03), (0, 0.24, 0.08), "glow"); out.append(a)
    return out

def build_all():
    return [hub("ic_hub", HOME), hub("ic_hub_rival", RIVAL), miner(), smelter(),
            constructor(), assembler(), barracks(), terminal(),
            robot("ic_robot", HOME), robot("ic_robot_rival", RIVAL),
            turret(), mine(), fence(), tripwire(), avatar()] + items()
