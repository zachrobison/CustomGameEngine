"""Sandbox — playground set. Sandbox already reuses every other game's
assets; these are the generic world props it (and the others) were missing:
trees and rocks (pine_tall.obj / tree_default.obj are empty stubs), a target
dummy, an explosive barrel, a crate, a bouncy pad and a spawn marker."""
import math
from agen import Asset, palette, rot_e

P = {
    "bark":    ((0.40, 0.27, 0.17), 0.0, 0.9),
    "pine":    ((0.16, 0.42, 0.26), 0.0, 0.8),
    "pinelt":  ((0.22, 0.52, 0.30), 0.0, 0.8),
    "leaf":    ((0.38, 0.62, 0.25), 0.0, 0.8),
    "leaflt":  ((0.50, 0.72, 0.30), 0.0, 0.8),
    "rock":    ((0.52, 0.52, 0.50), 0.0, 0.9),
    "rockdk":  ((0.40, 0.40, 0.39), 0.0, 0.9),
    "moss":    ((0.35, 0.50, 0.22), 0.0, 0.9),
    "straw":   ((0.88, 0.76, 0.42), 0.0, 0.9),
    "sack":    ((0.78, 0.68, 0.52), 0.0, 0.9),
    "red":     ((0.85, 0.12, 0.12), 0.1, 0.5),
    "white":   ((0.94, 0.94, 0.94), 0.0, 0.5),
    "yellow":  ((1.00, 0.82, 0.10), 0.0, 0.5),
    "black":   ((0.08, 0.08, 0.09), 0.2, 0.5),
    "wood":    ((0.62, 0.45, 0.27), 0.0, 0.8),
    "wooddk":  ((0.42, 0.29, 0.17), 0.0, 0.8),
    "steel":   ((0.55, 0.57, 0.60), 0.8, 0.4),
    "spring":  ((0.20, 0.80, 0.95), 0.0, 0.3),
    "orange":  ((1.00, 0.50, 0.10), 0.0, 0.4),
}
M = palette("sb", P)

def pine():
    """Stacked-cone pine, ~6.5 tall."""
    a = Asset("sb_tree_pine", M)
    a.cyl(0.25, 1.6, (0, 0.8, 0), "bark", seg=7, r2=0.18, smooth=False)
    for i, (r, h, y) in enumerate(((1.9, 2.0, 2.2), (1.5, 1.9, 3.4), (1.1, 1.8, 4.5), (0.65, 1.6, 5.6))):
        a.cyl(r, h, (0, y, 0), "pine" if i % 2 == 0 else "pinelt", seg=8, r2=0.0, smooth=False)
    return a

def round_tree():
    """Broadleaf tree: trunk + three blobby canopies, ~5 tall."""
    a = Asset("sb_tree_round", M)
    a.cyl(0.28, 2.6, (0, 1.3, 0), "bark", seg=7, r2=0.2, smooth=False)
    a.cyl(0.1, 1.0, (0.35, 2.4, 0), "bark", seg=5, rot=rot_e(rz=-40), smooth=False)
    a.sphere(1.6, (0, 3.6, 0), "leaf", seg=8, rings=6, smooth=False)
    a.sphere(1.1, (0.9, 3.1, 0.5), "leaflt", seg=7, rings=5, smooth=False)
    a.sphere(1.0, (-0.8, 3.3, -0.6), "leaflt", seg=7, rings=5, smooth=False)
    return a

def rock(name, s, seed):
    """Low-poly boulder (faceted, a little sunk into the ground)."""
    a = Asset(name, M)
    a.clip_ground = True
    a.sphere(1.0 * s, (0, 0.35 * s, 0), "rock", scale=(1.3, 0.8, 1.0), seg=7, rings=5,
             rot=rot_e(ry=seed * 37), smooth=False)
    a.sphere(0.6 * s, (0.7 * s, 0.25 * s, 0.3 * s), "rockdk", scale=(1, 0.8, 1.1), seg=6, rings=4,
             rot=rot_e(ry=seed * 53), smooth=False)
    a.sphere(0.5 * s, (-0.2 * s, 0.85 * s, -0.1 * s), "moss", scale=(1.3, 0.3, 1.1), seg=6, rings=3,
             smooth=False)
    return a

def dummy():
    """Straw target dummy on a post with a bullseye, faces +Z. ~1.9 tall."""
    a = Asset("sb_target_dummy", M)
    a.box((0.8, 0.1, 0.8), (0, 0.05, 0), "wooddk")
    a.cyl(0.06, 1.1, (0, 0.6, 0), "wood", seg=8)
    a.cyl(0.28, 0.75, (0, 1.3, 0), "sack", seg=10, r2=0.3)
    a.box((1.2, 0.12, 0.12), (0, 1.5, 0), "wood")                            # arms
    for x in (-0.62, 0.62):
        a.cyl(0.08, 0.2, (x, 1.5, 0), "straw", rot=rot_e(rz=90), seg=6, r2=0.0)
    a.sphere(0.22, (0, 1.88, 0), "sack", seg=10, rings=8)
    for i, (r, c) in enumerate(((0.24, "white"), (0.17, "red"), (0.1, "white"), (0.045, "red"))):
        a.cyl(r, 0.02, (0, 1.3, 0.29 + i * 0.006), c, rot=rot_e(rx=90), seg=16)
    return a

def barrel():
    """Explosive barrel (red with a hazard band)."""
    a = Asset("sb_explosive_barrel", M)
    a.cyl(0.32, 0.95, (0, 0.475, 0), "red", seg=16)
    for y in (0.05, 0.9):
        a.torus(0.32, 0.025, (0, y, 0), "black", seg=16, sides=4)
    a.cyl(0.325, 0.2, (0, 0.48, 0), "yellow", seg=16)
    for i in range(6):
        ang = 60 * i
        a.box((0.06, 0.2, 0.02), (math.sin(math.radians(ang)) * 0.33, 0.48,
              math.cos(math.radians(ang)) * 0.33), "black", rot=rot_e(ry=ang, rz=30))
    a.cyl(0.06, 0.04, (0.15, 0.97, 0.05), "black", seg=8)
    return a

def crate():
    a = Asset("sb_crate", M)
    a.box((1.0, 1.0, 1.0), (0, 0.5, 0), "wood", bevel=0.03)
    for s in (-1, 1):
        a.box((1.02, 0.14, 1.02), (0, 0.5 + s * 0.38, 0), "wooddk")
    a.box((0.3, 0.3, 0.02), (0, 0.5, 0.51), "orange")
    return a

def bounce_pad():
    """Spring-loaded jump pad, 2 across."""
    a = Asset("sb_bounce_pad", M)
    a.cyl(1.0, 0.15, (0, 0.075, 0), "black", seg=16)
    for y in (0.2, 0.3, 0.4):
        a.torus(0.55, 0.04, (0, y, 0), "steel", seg=16, sides=4)
    a.cyl(0.85, 0.08, (0, 0.5, 0), "spring", seg=16)
    a.cyl(0.4, 0.09, (0, 0.51, 0), "white", seg=16)
    return a

def spawn_marker():
    """Player spawn pad with an arrow toward +Z."""
    a = Asset("sb_spawn_marker", M)
    a.cyl(1.2, 0.08, (0, 0.04, 0), "steel", seg=24)
    a.torus(1.1, 0.05, (0, 0.09, 0), "orange", seg=24, sides=4)
    a.box((0.25, 0.04, 0.9), (0, 0.1, 0.1), "white")
    a.box((0.5, 0.04, 0.12), (-0.16, 0.1, 0.55), "white", rot=rot_e(ry=-40))
    a.box((0.5, 0.04, 0.12), (0.16, 0.1, 0.55), "white", rot=rot_e(ry=40))
    return a

def build_all():
    return [pine(), round_tree(), rock("sb_rock_large", 1.4, 1), rock("sb_rock_small", 0.6, 2),
            dummy(), barrel(), crate(), bounce_pad(), spawn_marker()]
