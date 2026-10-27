"""Raceway (stunt) — loud stunt-show cars and track furniture.
Cars are real-world size (~4.2 long, forward +Z): render at scale 1, not the
carMeshScale=3 the old tiny car.glb needs."""
import math
from agen import Asset, palette, rot_e

P = {
    "orange":  ((1.00, 0.45, 0.08), 0.3, 0.35),
    "blue":    ((0.10, 0.35, 0.90), 0.3, 0.35),
    "green":   ((0.15, 0.80, 0.35), 0.3, 0.35),
    "purple":  ((0.55, 0.20, 0.85), 0.3, 0.35),
    "yellow":  ((1.00, 0.85, 0.10), 0.0, 0.4),
    "white":   ((0.94, 0.94, 0.94), 0.0, 0.5),
    "black":   ((0.08, 0.08, 0.09), 0.2, 0.5),
    "tire":    ((0.07, 0.07, 0.08), 0.0, 0.9),
    "rim":     ((0.75, 0.77, 0.80), 1.0, 0.25),
    "glass":   ((0.15, 0.22, 0.30), 0.0, 0.05),
    "head":    ((1.00, 0.97, 0.85), 0.0, 0.2),
    "tail":    ((0.95, 0.10, 0.10), 0.0, 0.3),
    "concrete":((0.70, 0.69, 0.66), 0.0, 0.9),
    "red":     ((0.85, 0.12, 0.12), 0.0, 0.5),
    "boost":   ((0.20, 0.95, 1.00), 0.0, 0.2),
    "metal":   ((0.45, 0.47, 0.50), 0.8, 0.4),
    "cone":    ((1.00, 0.40, 0.05), 0.0, 0.6),
}
M = palette("rc", P)

def car(name, body, stripe):
    """Stunt buggy-coupe. 2.0 wide x 1.3 tall x 4.2 long, nose at +Z."""
    a = Asset(name, M)
    a.box((1.8, 0.45, 4.0), (0, 0.6, 0), body, bevel=0.15)                    # tub
    a.box((1.7, 0.2, 1.3), (0, 0.85, 1.25), body, bevel=0.1, rot=rot_e(rx=-6))  # hood
    a.box((1.5, 0.5, 1.6), (0, 1.05, -0.3), "glass", bevel=0.18)               # canopy
    a.box((1.52, 0.1, 1.2), (0, 1.32, -0.35), body, bevel=0.04)                # roof
    a.box((0.35, 0.03, 4.02), (0, 0.84, 0), stripe)                            # racing stripes
    a.box((0.35, 0.03, 1.22), (0, 1.38, -0.35), stripe)
    a.box((1.9, 0.08, 0.35), (0, 1.35, -1.85), "black")                        # wing
    for x in (-0.75, 0.75):
        a.box((0.06, 0.4, 0.25), (x, 1.15, -1.85), "black")
    a.box((1.9, 0.25, 0.25), (0, 0.4, 2.0), "black", bevel=0.05)               # bumpers
    a.box((1.9, 0.25, 0.25), (0, 0.4, -2.0), "black", bevel=0.05)
    for x in (-0.6, 0.6):
        a.box((0.35, 0.12, 0.05), (x, 0.68, 2.0), "head")
        a.box((0.35, 0.1, 0.05), (x, 0.68, -2.01), "tail")
    for x in (-0.95, 0.95):
        for z in (-1.35, 1.4):
            a.cyl(0.42, 0.36, (x, 0.42, z), "tire", rot=rot_e(rz=90), seg=18)
            a.cyl(0.24, 0.38, (x, 0.42, z), "rim", rot=rot_e(rz=90), seg=6, smooth=False)
    a.cyl(0.07, 0.3, (0.4, 0.55, -2.1), "metal", rot=rot_e(rx=90), seg=8)       # exhaust
    a.cyl(0.07, 0.3, (-0.4, 0.55, -2.1), "metal", rot=rot_e(rx=90), seg=8)
    return a

def ramp():
    """Stunt ramp, 4 wide x 1.6 high x 6 long; launches toward +Z."""
    a = Asset("rc_ramp", M)
    # wedge is tallest at -Z; flip it so the lip is at +Z
    a.wedge((4.0, 1.6, 6.0), (0, 0, 0), "metal", rot=rot_e(ry=180))
    for x in (-2.05, 2.05):
        a.box((0.12, 0.25, 6.2), (x, 0.8, 0), "yellow", rot=rot_e(rx=-math.degrees(math.atan2(1.6, 6.0))))
    for i in range(5):                                                         # chevrons
        a.box((0.9, 0.03, 0.3), (-0.45, 0.25 + i * 0.3, -2.0 + i * 1.1), "yellow",
              rot=rot_e(rx=-15, ry=35))
        a.box((0.9, 0.03, 0.3), (0.45, 0.25 + i * 0.3, -2.0 + i * 1.1), "yellow",
              rot=rot_e(rx=-15, ry=-35))
    return a

def boost_pad():
    """Flush boost pad, 3 x 4, arrows point +Z."""
    a = Asset("rc_boost_pad", M)
    a.box((3.0, 0.06, 4.0), (0, 0.03, 0), "black")
    for z in (-1.2, 0.0, 1.2):
        for s in (-1, 1):
            a.box((1.2, 0.04, 0.35), (s * 0.42, 0.07, z), "boost", rot=rot_e(ry=-s * 35))
    for x in (-1.45, 1.45):
        a.box((0.1, 0.08, 4.0), (x, 0.04, 0), "yellow")
    return a

def tire_barrier():
    """Stack of tyres strapped together (3 wide x 2 high)."""
    a = Asset("rc_tire_barrier", M)
    for i in range(3):
        for j in range(2):
            a.torus(0.32, 0.12, (-0.64 + i * 0.64, 0.12 + j * 0.24, 0), "tire", seg=16, sides=6)
    a.box((2.0, 0.08, 0.02), (0, 0.3, 0.45), "red")
    a.box((2.0, 0.08, 0.02), (0, 0.3, -0.45), "red")
    return a

def jersey_barrier():
    """Concrete jersey barrier, 3 long along X."""
    a = Asset("rc_jersey_barrier", M)
    a.box((3.0, 0.3, 0.6), (0, 0.15, 0), "concrete")
    a.wedge((3.0, 0.5, 0.2), (0, 0.3, 0.2), "concrete", rot=rot_e(ry=180))
    a.wedge((3.0, 0.5, 0.2), (0, 0.3, -0.2), "concrete")
    a.box((3.0, 0.5, 0.2), (0, 0.55, 0), "concrete")
    a.box((3.0, 0.1, 0.22), (0, 0.85, 0), "red")
    return a

def cone():
    a = Asset("rc_cone", M)
    a.box((0.4, 0.04, 0.4), (0, 0.02, 0), "black")
    a.cyl(0.15, 0.62, (0, 0.35, 0), "cone", seg=14, r2=0.03)
    a.cyl(0.11, 0.12, (0, 0.38, 0), "white", seg=14, r2=0.085)
    return a

def checkpoint():
    """Checkpoint / finish arch, 12 wide x 6 tall, banner faces +Z."""
    a = Asset("rc_checkpoint", M)
    for x in (-6.0, 6.0):
        a.box((0.6, 6.0, 0.6), (x, 3.0, 0), "metal")
        a.box((0.9, 0.3, 0.9), (x, 0.15, 0), "black")
    a.box((12.6, 1.2, 0.4), (0, 6.0, 0), "black")
    for i in range(14):                                                        # checker band
        for j in range(2):
            a.box((0.86, 0.5, 0.05), (-5.6 + i * 0.86, 5.75 + j * 0.5, 0.21),
                  "white" if (i + j) % 2 else "black")
    for x in (-3, 0, 3):
        a.sphere(0.2, (x, 6.75, 0), "yellow", seg=10, rings=6)
    return a

def flag():
    a = Asset("rc_flag", M)
    a.cyl(0.05, 3.0, (0, 1.5, 0), "metal", seg=8)
    for i in range(4):
        for j in range(3):
            a.box((0.25, 0.2, 0.02), (0.15 + i * 0.25, 2.65 - j * 0.2, 0),
                  "white" if (i + j) % 2 else "black")
    return a

def build_all():
    return [car("rc_car", "orange", "white"), car("rc_car_ai_blue", "blue", "white"),
            car("rc_car_ai_green", "green", "black"), car("rc_car_ai_purple", "purple", "yellow"),
            ramp(), boost_pad(), tire_barrier(), jersey_barrier(), cone(), checkpoint(), flag()]
