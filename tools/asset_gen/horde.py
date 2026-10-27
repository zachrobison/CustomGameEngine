"""Horde Defense — frontier fort vs. a goblin raider horde.
Weathered wood + iron for the fort, mossy green raiders, brass voxel tools."""
import math
from agen import Asset, palette, rot_e

P = {
    "stone":    ((0.58, 0.56, 0.52), 0.0, 0.9),
    "stonedk":  ((0.40, 0.39, 0.37), 0.0, 0.9),
    "wood":     ((0.56, 0.39, 0.23), 0.0, 0.8),
    "wooddk":   ((0.34, 0.23, 0.14), 0.0, 0.8),
    "iron":     ((0.30, 0.31, 0.33), 0.8, 0.5),
    "banner":   ((0.20, 0.38, 0.72), 0.0, 0.7),
    "gold":     ((0.92, 0.74, 0.30), 1.0, 0.3),
    "flame":    ((1.00, 0.55, 0.15), 0.0, 0.3),
    "ember":    ((1.00, 0.85, 0.40), 0.0, 0.2),
    "skin":     ((0.42, 0.58, 0.28), 0.0, 0.7),
    "skindk":   ((0.30, 0.42, 0.20), 0.0, 0.7),
    "eye":      ((1.00, 0.85, 0.10), 0.0, 0.2),
    "leather":  ((0.38, 0.24, 0.15), 0.0, 0.7),
    "cloth":    ((0.55, 0.20, 0.16), 0.0, 0.8),
    "bone":     ((0.90, 0.86, 0.74), 0.0, 0.6),
    "brass":    ((0.80, 0.62, 0.28), 0.9, 0.35),
    "tank":     ((0.30, 0.75, 0.95), 0.0, 0.2),
    "pink":     ((0.95, 0.45, 0.60), 0.0, 0.5),
    "steel":    ((0.62, 0.64, 0.68), 0.8, 0.35),
    "turret":   ((0.35, 0.65, 0.95), 0.2, 0.4),
}
M = palette("hd", P)

def tower():
    """The keep you defend. ~4.4 wide, 9.5 tall; door faces +Z."""
    a = Asset("hd_tower", M)
    a.cyl(2.2, 0.6, (0, 0.3, 0), "stonedk", seg=8, smooth=False)
    a.cyl(1.9, 6.2, (0, 3.6, 0), "stone", seg=8, r2=1.7, smooth=False)
    for y in (2.2, 4.6):
        a.cyl(1.95 - (y - 0.6) * 0.03, 0.18, (0, y, 0), "stonedk", seg=8, smooth=False)
    a.cyl(2.3, 0.5, (0, 6.95, 0), "stonedk", seg=8, smooth=False)            # parapet ledge
    for i in range(8):                                                       # crenellations
        ang = 22.5 + 45 * i
        s, c = math.sin(math.radians(ang)), math.cos(math.radians(ang))
        a.box((0.7, 0.6, 0.4), (s * 2.05, 7.5, c * 2.05), "stone", rot=rot_e(ry=ang))
    a.cyl(1.6, 1.8, (0, 8.1, 0), "wood", seg=8, r2=0.05, smooth=False)        # roof cone
    a.cyl(0.05, 1.4, (0, 9.4, 0), "wooddk", seg=6)                           # flag pole
    a.box((0.02, 0.5, 0.8), (0, 9.75, 0.42), "banner")
    a.box((1.2, 2.0, 0.4), (0, 1.3, 1.82), "wooddk")                         # door
    a.cyl(0.6, 0.4, (0, 2.3, 1.82), "wooddk", rot=rot_e(rx=90), seg=12)       # arched top
    for y in (0.8, 1.8):
        a.box((1.3, 0.1, 0.1), (0, y, 2.04), "iron")
    for ang in (60, 180, 300):                                               # arrow slits
        s, c = math.sin(math.radians(ang)), math.cos(math.radians(ang))
        a.box((0.16, 0.8, 0.2), (s * 1.78, 4.0, c * 1.78), "wooddk", rot=rot_e(ry=ang))
    return a

def turret():
    """Placeable auto-turret (same envelope as Turrets.cpp boxes). Barrel to +Z."""
    a = Asset("hd_turret", M)
    for i in range(3):                                                       # tripod
        ang = 120 * i
        s, c = math.sin(math.radians(ang)), math.cos(math.radians(ang))
        a.box((0.08, 1.05, 0.08), (s * 0.25, 0.5, c * 0.25), "wooddk", rot=rot_e(ry=ang, rx=-14))
    a.cyl(0.18, 0.15, (0, 1.0, 0), "iron", seg=10)
    a.box((0.6, 0.4, 0.6), (0, 1.25, 0), "turret", bevel=0.06)
    a.box((0.64, 0.06, 0.64), (0, 1.07, 0), "iron")
    for x in (-0.06, 0.06):
        a.cyl(0.045, 0.75, (x, 1.22, 0.55), "steel", rot=rot_e(rx=90), seg=8)
    a.box((0.22, 0.1, 0.14), (0, 1.5, -0.05), "iron")                        # sight
    a.cyl(0.05, 0.03, (0, 1.5, 0.03), "ember", rot=rot_e(rx=90), seg=8)
    return a

def goblin(name, ranged):
    """Raider. ~1.4 tall (ranged one carries a blunderbuss). Faces +Z."""
    a = Asset(name, M)
    for x in (-0.12, 0.12):
        a.box((0.16, 0.08, 0.26), (x, 0.04, 0.04), "leather", bevel=0.02)
        a.cyl(0.07, 0.42, (x, 0.28, 0), "skindk", seg=8)
    a.box((0.42, 0.22, 0.26), (0, 0.55, 0), "cloth", bevel=0.04)             # loincloth/belt
    a.sphere(0.25, (0, 0.82, 0.02), "skin", scale=(1, 1.1, 0.85), seg=12, rings=8)  # pot belly
    a.box((0.48, 0.12, 0.08), (0, 0.6, 0.18), "leather")                     # belt
    a.sphere(0.04, (0, 0.6, 0.23), "gold", seg=6, rings=4)
    a.sphere(0.22, (0, 1.2, 0.04), "skin", scale=(1.05, 0.95, 1), seg=12, rings=8)  # head
    for x in (-1, 1):
        a.cyl(0.07, 0.32, (x * 0.3, 1.28, 0), "skin", rot=rot_e(rz=-x * 70), seg=6, r2=0.0)  # ears
        a.sphere(0.04, (x * 0.08, 1.24, 0.22), "eye", seg=6, rings=4)
        a.cyl(0.06, 0.45, (x * 0.3, 0.82, 0.04), "skindk", rot=rot_e(rz=x * 12), seg=8)    # arms
        a.sphere(0.075, (x * 0.34, 0.58, 0.06), "skin", seg=8, rings=6)                     # fists
    a.cyl(0.05, 0.12, (0, 1.16, 0.26), "skindk", rot=rot_e(rx=70), seg=6, r2=0.02)          # nose
    for x in (-0.06, 0.06):
        a.cyl(0.018, 0.06, (x, 1.08, 0.2), "bone", seg=5, r2=0.0)                           # tusks
    if ranged:
        a.cyl(0.05, 0.8, (0.32, 0.72, 0.3), "wooddk", rot=rot_e(rx=85), seg=8)               # blunderbuss
        a.cyl(0.08, 0.2, (0.32, 0.75, 0.72), "brass", rot=rot_e(rx=85), seg=10, r2=0.11)
        a.sphere(0.24, (0, 1.32, 0.0), "iron", scale=(1.05, 0.55, 1.05), seg=12, rings=6)   # pot helmet
        a.box((0.06, 0.6, 0.6), (-0.3, 0.9, -0.2), "leather", rot=rot_e(rx=20))             # bandolier-ish pack
    else:
        a.cyl(0.035, 0.7, (0.36, 0.75, 0.3), "wooddk", rot=rot_e(rx=60), seg=6)              # club
        a.cyl(0.11, 0.3, (0.36, 0.92, 0.6), "wood", rot=rot_e(rx=60), seg=8, r2=0.07)
        for i in range(3):
            a.cyl(0.02, 0.08, (0.36 + 0.09 * math.cos(i * 2.1), 0.98, 0.62 + 0.09 * math.sin(i * 2.1)),
                  "iron", rot=rot_e(rx=60), seg=4, r2=0.0)
        a.cyl(0.2, 0.1, (0, 1.33, 0), "bone", seg=8, r2=0.12)                                # skull cap
    return a

def sprayer():
    """Voxel sprayer (build tool), held: grip at origin, nozzle to +Z. ~0.55 long."""
    a = Asset("hd_sprayer", M)
    a.box((0.06, 0.16, 0.08), (0, -0.08, -0.02), "wooddk", rot=rot_e(rx=-15))  # grip
    a.box((0.1, 0.12, 0.36), (0, 0.05, 0.08), "brass", bevel=0.02)
    a.cyl(0.07, 0.22, (0, 0.17, -0.02), "tank", seg=12)                       # paint tank
    a.dome(0.07, (0, 0.28, -0.02), "brass", seg=12, rings=4)
    a.cyl(0.035, 0.2, (0, 0.05, 0.34), "steel", rot=rot_e(rx=90), seg=8)
    a.cyl(0.035, 0.08, (0, 0.05, 0.47), "brass", rot=rot_e(rx=90), seg=8, r2=0.06)
    a.box((0.02, 0.05, 0.04), (0, -0.02, 0.08), "iron")                       # trigger
    return a

def eraser():
    """Voxel eraser, held: grip at origin, emitter dish to +Z."""
    a = Asset("hd_eraser", M)
    a.box((0.06, 0.16, 0.08), (0, -0.08, -0.02), "wooddk", rot=rot_e(rx=-15))
    a.box((0.12, 0.12, 0.3), (0, 0.05, 0.06), "iron", bevel=0.02)
    for z in (0.0, 0.08, 0.16):
        a.torus(0.07, 0.015, (0, 0.05, z), "brass", rot=rot_e(rx=90), seg=12, sides=4)
    a.cyl(0.03, 0.12, (0, 0.05, 0.26), "steel", rot=rot_e(rx=90), seg=8)
    a.cyl(0.03, 0.06, (0, 0.05, 0.34), "pink", rot=rot_e(rx=90), seg=12, r2=0.11)   # dish
    a.sphere(0.03, (0, 0.05, 0.36), "ember", seg=8, rings=4)
    return a

def jetpack():
    """Back-mounted jetpack. Straps toward +Z (wearer's back is at z≈0.1)."""
    a = Asset("hd_jetpack", M)
    a.box((0.3, 0.42, 0.12), (0, 0.0, 0.0), "leather", bevel=0.03)
    for x in (-0.12, 0.12):
        a.cyl(0.09, 0.5, (x, 0.0, -0.12), "brass", seg=12)
        a.dome(0.09, (x, 0.25, -0.12), "brass", seg=12, rings=4)
        a.cyl(0.07, 0.12, (x, -0.31, -0.12), "iron", seg=10, r2=0.1)
        a.cyl(0.06, 0.18, (x, -0.46, -0.12), "flame", seg=8, r2=0.0, rot=rot_e(rx=180))
    for x in (-0.1, 0.1):
        a.box((0.05, 0.5, 0.02), (x, 0.0, 0.08), "wooddk")
    return a

def barricade():
    """Spiked log barricade (cheval de frise). 2.4 wide."""
    a = Asset("hd_barricade", M)
    a.cyl(0.13, 2.4, (0, 0.55, 0), "wooddk", rot=rot_e(rz=90), seg=8)
    for i, x in enumerate((-0.9, -0.3, 0.3, 0.9)):
        tilt = 40 if i % 2 else -40
        a.cyl(0.06, 1.5, (x, 0.55, 0), "wood", rot=rot_e(rx=tilt), seg=6, r2=0.01)
        a.cyl(0.06, 1.5, (x + 0.15, 0.55, 0), "wood", rot=rot_e(rx=-tilt), seg=6, r2=0.01)
    for x in (-1.1, 1.1):
        a.box((0.1, 0.1, 0.8), (x, 0.05, 0), "wooddk")
    return a

def brazier():
    """Iron fire brazier on a post. ~1.4 tall."""
    a = Asset("hd_brazier", M)
    a.cyl(0.25, 0.08, (0, 0.04, 0), "iron", seg=8, smooth=False)
    a.cyl(0.05, 1.0, (0, 0.55, 0), "iron", seg=8)
    a.cyl(0.32, 0.25, (0, 1.15, 0), "iron", seg=10, r2=0.22, rot=rot_e(rx=180))
    for i in range(5):
        ang = 72 * i
        a.cyl(0.12 - i * 0.01, 0.35 + 0.05 * (i % 2), (math.sin(math.radians(ang)) * 0.1, 1.38,
              math.cos(math.radians(ang)) * 0.1), "flame", seg=6, r2=0.0)
    a.cyl(0.1, 0.5, (0, 1.45, 0), "ember", seg=6, r2=0.0)
    return a

def supply_crate():
    """Wave-reward crate with iron bands and a blue seal."""
    a = Asset("hd_supply_crate", M)
    a.box((0.9, 0.7, 0.7), (0, 0.35, 0), "wood", bevel=0.02)
    for x in (-0.3, 0.3):
        a.box((0.06, 0.72, 0.72), (x, 0.35, 0), "iron")
    a.box((0.92, 0.06, 0.72), (0, 0.69, 0), "wooddk")
    a.cyl(0.08, 0.03, (0, 0.4, 0.36), "banner", rot=rot_e(rx=90), seg=10)
    return a

def build_all():
    b = goblin("hd_brute", False)   # big raider for later waves, ~2.3 tall
    b.scale = 1.6
    return [tower(), turret(), goblin("hd_grunt", False), goblin("hd_shooter", True), b,
            sprayer(), eraser(), jetpack(), barricade(), brazier(), supply_crate()]
