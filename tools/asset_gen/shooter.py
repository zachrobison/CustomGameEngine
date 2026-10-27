"""Interdimensional Shooter — rift tech. Dark violet armour, neon cyan/magenta
energy (one colour per dimension), rockets and a grapple."""
import math
from agen import Asset, palette, rot_e

P = {
    "armor":   ((0.20, 0.18, 0.28), 0.6, 0.4),
    "armorlt": ((0.42, 0.40, 0.52), 0.6, 0.4),
    "under":   ((0.08, 0.08, 0.10), 0.2, 0.6),
    "cyan":    ((0.20, 0.95, 1.00), 0.0, 0.2),
    "magenta": ((1.00, 0.20, 0.80), 0.0, 0.2),
    "white":   ((0.92, 0.94, 0.98), 0.1, 0.4),
    "steel":   ((0.60, 0.62, 0.68), 0.9, 0.3),
    "orange":  ((1.00, 0.50, 0.12), 0.0, 0.3),
    "red":     ((0.85, 0.12, 0.15), 0.1, 0.4),
    "rock":    ((0.33, 0.30, 0.38), 0.0, 0.9),
    "crystal": ((0.60, 0.40, 1.00), 0.0, 0.1),
}
M = palette("sh", P)

def trooper(name, eye, gun_long):
    """Rift trooper. ~1.9 tall, faces +Z. 'Shooter' variant has a long rifle."""
    a = Asset(name, M)
    for x in (-0.15, 0.15):
        a.box((0.18, 0.1, 0.32), (x, 0.05, 0.04), "under", bevel=0.03)
        a.box((0.17, 0.45, 0.2), (x, 0.33, 0), "armor", bevel=0.04)
        a.box((0.19, 0.12, 0.22), (x, 0.56, 0.03), "armorlt", bevel=0.03)     # knee pads
        a.box((0.18, 0.38, 0.2), (x, 0.8, 0), "under", bevel=0.03)
    a.box((0.48, 0.2, 0.28), (0, 1.04, 0), "armor", bevel=0.05)
    a.box((0.58, 0.48, 0.34), (0, 1.38, 0), "armor", bevel=0.08)              # torso
    a.box((0.36, 0.2, 0.05), (0, 1.45, 0.18), eye)                             # chest core
    a.box((0.4, 0.5, 0.18), (0, 1.38, -0.24), "armorlt", bevel=0.05)          # rift pack
    a.cyl(0.06, 0.4, (0, 1.4, -0.34), eye, seg=8)
    for x in (-0.38, 0.38):
        a.sphere(0.13, (x, 1.55, 0), "armorlt", scale=(1, 0.8, 1), seg=12, rings=6)
        a.cyl(0.07, 0.6, (x * 1.02, 1.2, 0.05), "under", seg=8, rot=rot_e(rx=-25))
    a.box((0.26, 0.28, 0.28), (0, 1.8, 0), "armor", bevel=0.07)               # helmet
    a.box((0.22, 0.06, 0.04), (0, 1.82, 0.14), eye)                            # visor
    a.box((0.04, 0.2, 0.12), (0.15, 1.88, -0.04), "armorlt")                   # fin
    L = 0.85 if gun_long else 0.5
    a.box((0.09, 0.14, L), (0.2, 1.15, 0.28 + L / 2 - 0.1), "steel", bevel=0.02)   # rifle
    a.cyl(0.03, 0.2, (0.2, 1.17, 0.28 + L), eye, rot=rot_e(rx=90), seg=8)
    if gun_long:
        a.cyl(0.05, 0.25, (0.2, 1.27, 0.35), "under", rot=rot_e(rx=90), seg=8)       # scope
    return a

def drone():
    """Hovering rift drone. Origin at its centre (it floats). ~1.1 across."""
    a = Asset("sh_drone", M)
    a.sphere(0.32, (0, 0, 0), "armor", seg=16, rings=10)
    a.torus(0.5, 0.06, (0, 0, 0), "armorlt", seg=24, sides=6)
    for ang in (0, 120, 240):
        s, c = math.sin(math.radians(ang)), math.cos(math.radians(ang))
        a.box((0.06, 0.06, 0.2), (s * 0.4, 0, c * 0.4), "under", rot=rot_e(ry=ang))
        a.cyl(0.08, 0.12, (s * 0.5, -0.05, c * 0.5), "magenta", seg=8)
    a.cyl(0.14, 0.08, (0, 0, 0.3), "magenta", rot=rot_e(rx=90), seg=12)       # eye
    a.cyl(0.06, 0.1, (0, 0, 0.35), "white", rot=rot_e(rx=90), seg=8)
    a.cyl(0.04, 0.25, (0, -0.4, 0), "steel", seg=6, r2=0.0, rot=rot_e(rx=180))
    return a

def rocket():
    """Rocket projectile. 0.9 long, nose +Z, origin at centre."""
    a = Asset("sh_rocket", M)
    a.cyl(0.07, 0.6, (0, 0, 0), "white", rot=rot_e(rx=90), seg=10)
    a.cyl(0.07, 0.22, (0, 0, 0.41), "red", rot=rot_e(rx=90), seg=10, r2=0.0)
    a.torus(0.07, 0.015, (0, 0, 0.15), "red", rot=rot_e(rx=90), seg=10, sides=4)
    for ang in (0, 90, 180, 270):
        s, c = math.sin(math.radians(ang)), math.cos(math.radians(ang))
        a.box((0.015, 0.12, 0.18), (s * 0.1, c * 0.1, -0.24), "steel", rot=rot_e(rz=ang))
    a.cyl(0.05, 0.12, (0, 0, -0.36), "orange", rot=rot_e(rx=-90), seg=8, r2=0.0)
    return a

def launcher():
    """Shoulder rocket launcher (viewmodel/held). Grip at origin, muzzle +Z."""
    a = Asset("sh_launcher", M)
    a.cyl(0.11, 1.0, (0, 0.12, 0.2), "armor", rot=rot_e(rx=90), seg=14)
    a.torus(0.11, 0.025, (0, 0.12, 0.7), "cyan", rot=rot_e(rx=90), seg=14, sides=4)
    a.torus(0.11, 0.025, (0, 0.12, -0.3), "armorlt", rot=rot_e(rx=90), seg=14, sides=4)
    a.box((0.06, 0.16, 0.08), (0, -0.04, 0), "under", rot=rot_e(rx=-15))
    a.box((0.06, 0.14, 0.07), (0, -0.02, 0.35), "under")
    a.box((0.05, 0.1, 0.22), (-0.13, 0.22, 0.25), "steel")                    # sight
    a.cyl(0.035, 0.03, (-0.13, 0.22, 0.37), "cyan", rot=rot_e(rx=90), seg=8)
    return a

def grapple():
    """Wrist grapple launcher with a three-prong hook (hook toward +Z)."""
    a = Asset("sh_grapple", M)
    a.box((0.12, 0.1, 0.3), (0, 0, 0), "armor", bevel=0.02)
    a.cyl(0.04, 0.2, (0, 0.03, 0.22), "steel", rot=rot_e(rx=90), seg=8)
    a.cyl(0.04, 0.03, (0, 0.06, -0.08), "cyan", seg=8)
    for ang in (0, 120, 240):                                                 # prongs splayed around +Z
        s, c = math.sin(math.radians(ang)), math.cos(math.radians(ang))
        a.cyl(0.012, 0.12, (s * 0.03, 0.03 + c * 0.03, 0.37), "steel", seg=5, r2=0.0,
              rot=rot_e(rz=ang) @ rot_e(rx=60))
    return a

def rift_pylon(name, glow):
    """Dimension anchor pylon — one per dimension colour. ~3.2 tall."""
    a = Asset(name, M)
    a.cyl(0.9, 0.3, (0, 0.15, 0), "under", seg=6, smooth=False)
    a.cyl(0.6, 0.3, (0, 0.45, 0), "armor", seg=6, smooth=False)
    for i in range(3):
        ang = 120 * i
        s, c = math.sin(math.radians(ang)), math.cos(math.radians(ang))
        a.box((0.18, 2.4, 0.18), (s * 0.45, 1.7, c * 0.45), "armorlt", rot=rot_e(ry=ang))
    a.sphere(0.3, (0, 1.9, 0), glow, scale=(1, 1.8, 1), seg=6, rings=4, smooth=False)  # crystal
    a.torus(0.6, 0.04, (0, 1.2, 0), glow, seg=18, sides=4)
    a.torus(0.6, 0.04, (0, 2.6, 0), glow, seg=18, sides=4)
    a.cyl(0.25, 0.4, (0, 3.0, 0), "armor", seg=6, r2=0.0, smooth=False)
    return a

def cover():
    """Waist-high cover block with energy strip. 3 x 1.2 x 1."""
    a = Asset("sh_cover", M)
    a.box((3.0, 1.1, 1.0), (0, 0.55, 0), "armor", bevel=0.1)
    a.box((2.6, 0.12, 1.02), (0, 0.9, 0), "cyan")
    a.box((3.05, 0.2, 1.05), (0, 0.1, 0), "under")
    return a

def rift_rock():
    """Floating shard of the other dimension (origin at centre)."""
    a = Asset("sh_rift_rock", M)
    a.sphere(0.8, (0, 0, 0), "rock", scale=(1.2, 0.7, 1.0), seg=7, rings=5, smooth=False)
    a.sphere(0.3, (0.3, 0.45, 0.1), "crystal", scale=(0.6, 1.8, 0.6), seg=5, rings=3, smooth=False)
    a.sphere(0.2, (-0.4, 0.4, -0.2), "crystal", scale=(0.6, 1.6, 0.6), seg=5, rings=3, smooth=False)
    a.cyl(0.5, 0.9, (0, -0.75, 0), "rock", seg=6, r2=0.0, rot=rot_e(rx=180), smooth=False)
    return a

def build_all():
    return [trooper("sh_trooper", "cyan", False), trooper("sh_shooter", "magenta", True),
            drone(), rocket(), launcher(), grapple(),
            rift_pylon("sh_pylon_cyan", "cyan"), rift_pylon("sh_pylon_magenta", "magenta"),
            cover(), rift_rock()]
