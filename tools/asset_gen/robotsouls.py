"""Robot Souls-Like — robot vs. the pig mafia in a grimy garage.
Gritty palette: gunmetal robot with amber eyes, mob red/gold, oil, rust, concrete."""
import math
from agen import Asset, palette, rot_e

P = {
    "gunmetal": ((0.30, 0.32, 0.36), 0.8, 0.4),
    "plate":    ((0.52, 0.54, 0.56), 0.7, 0.45),
    "joint":    ((0.12, 0.12, 0.14), 0.4, 0.6),
    "amber":    ((1.00, 0.70, 0.15), 0.0, 0.2),
    "hazard":   ((0.95, 0.75, 0.10), 0.0, 0.5),
    "blade":    ((0.82, 0.84, 0.88), 1.0, 0.15),
    "grip":     ((0.10, 0.08, 0.07), 0.0, 0.8),
    "concrete": ((0.55, 0.54, 0.52), 0.0, 0.9),
    "rust":     ((0.55, 0.27, 0.14), 0.3, 0.8),
    "oil":      ((0.06, 0.06, 0.07), 0.2, 0.3),
    "wood":     ((0.55, 0.38, 0.22), 0.0, 0.8),
    "wooddark": ((0.36, 0.24, 0.14), 0.0, 0.8),
    "mobred":   ((0.62, 0.08, 0.10), 0.1, 0.5),
    "gold":     ((0.90, 0.72, 0.28), 1.0, 0.3),
    "cash":     ((0.45, 0.66, 0.40), 0.0, 0.7),
    "tire":     ((0.09, 0.09, 0.10), 0.0, 0.9),
    "bluepaint":((0.18, 0.28, 0.45), 0.4, 0.4),
    "glass":    ((0.35, 0.45, 0.50), 0.0, 0.1),
    "lamp":     ((1.00, 0.95, 0.80), 0.0, 0.2),
    "leather":  ((0.30, 0.14, 0.09), 0.0, 0.6),
    "green":    ((0.20, 0.32, 0.22), 0.0, 0.6),
}
M = palette("rs", P)

def robot():
    """Player robot (static pose). ~1.8 tall, faces +Z. Knife hand on the right."""
    a = Asset("rs_robot", M)
    for x in (-0.16, 0.16):
        a.box((0.2, 0.12, 0.34), (x, 0.06, 0.05), "joint", bevel=0.03)   # feet
        a.box((0.16, 0.42, 0.18), (x, 0.36, 0), "plate", bevel=0.03)     # shins
        a.sphere(0.1, (x, 0.6, 0), "joint", seg=10, rings=6)              # knees
        a.box((0.18, 0.36, 0.2), (x, 0.8, 0), "gunmetal", bevel=0.03)    # thighs
    a.box((0.46, 0.14, 0.26), (0, 1.02, 0), "joint", bevel=0.03)          # pelvis
    a.cyl(0.08, 0.16, (0, 1.14, 0), "joint", seg=8)                       # waist
    a.box((0.56, 0.4, 0.32), (0, 1.38, 0), "gunmetal", bevel=0.06)        # chest
    a.box((0.4, 0.22, 0.04), (0, 1.42, 0.17), "plate", bevel=0.02)        # chest plate
    for i in range(4):                                                    # hazard stripes
        a.box((0.06, 0.05, 0.02), (-0.15 + i * 0.1, 1.26, 0.17), "hazard", rot=rot_e(rz=35))
    for x in (-0.36, 0.36):
        a.sphere(0.11, (x, 1.5, 0), "plate", seg=12, rings=8)            # shoulders
        a.cyl(0.06, 0.32, (x * 1.05, 1.3, 0), "joint", seg=8)             # upper arms
        a.cyl(0.075, 0.32, (x * 1.08, 1.0, 0.05), "gunmetal", seg=8, rot=rot_e(rx=-15))
        a.box((0.12, 0.12, 0.14), (x * 1.1, 0.82, 0.1), "joint", bevel=0.02)   # fists
    a.box((0.03, 0.04, 0.3), (0.4, 0.82, 0.28), "blade")                  # reverse-grip knife
    a.cyl(0.05, 0.08, (0, 1.62, 0), "joint", seg=8)                       # neck
    a.box((0.3, 0.24, 0.28), (0, 1.76, 0), "gunmetal", bevel=0.05)        # head
    a.box((0.24, 0.06, 0.04), (0, 1.78, 0.14), "amber")                   # visor slit
    a.box((0.04, 0.12, 0.04), (0.17, 1.82, -0.05), "plate")               # ear bolt
    a.box((0.04, 0.12, 0.04), (-0.17, 1.82, -0.05), "plate")
    a.box((0.34, 0.36, 0.14), (0, 1.36, -0.22), "plate", bevel=0.04)      # back pack
    a.cyl(0.05, 0.2, (0.1, 1.62, -0.24), "rust", seg=8)                   # exhaust stacks
    a.cyl(0.05, 0.2, (-0.1, 1.62, -0.24), "rust", seg=8)
    return a

def knife():
    """Combat knife. ~0.36 long along +Z, grip at origin."""
    a = Asset("rs_knife", M)
    a.cyl(0.022, 0.12, (0, 0, -0.02), "grip", rot=rot_e(rx=90), seg=8)
    a.box((0.08, 0.016, 0.02), (0, 0, 0.045), "plate")                     # guard
    a.box((0.008, 0.035, 0.2), (0, 0, 0.155), "blade")
    a.wedge((0.008, 0.035, 0.05), (0, -0.0175, 0.28), "blade", rot=rot_e(rx=0))
    a.sphere(0.022, (0, 0, -0.085), "plate", seg=8, rings=6)
    return a

def shutter():
    """Roll-up garage shutter that opens when the Don dies. 6 wide x 4.2 tall."""
    a = Asset("rs_garage_shutter", M)
    for x in (-3.1, 3.1):
        a.box((0.25, 4.2, 0.3), (x, 2.1, 0), "rust")
    a.cyl(0.35, 6.4, (0, 4.15, -0.05), "gunmetal", rot=rot_e(rz=90), seg=16)   # roll housing
    for i in range(16):
        a.box((6.0, 0.22, 0.08), (0, 0.13 + i * 0.245, 0.02 if i % 2 else 0), "plate" if i % 2 else "concrete")
    a.box((6.0, 0.12, 0.14), (0, 0.06, 0.02), "hazard")
    a.box((0.5, 0.08, 0.1), (0, 0.5, 0.08), "joint")                            # handle
    return a

def oil_drum():
    a = Asset("rs_oil_drum", M)
    a.cyl(0.3, 0.9, (0, 0.45, 0), "bluepaint", seg=20)
    for y in (0.03, 0.3, 0.6, 0.87):
        a.torus(0.3, 0.02, (0, y, 0), "gunmetal", seg=20, sides=4)
    a.cyl(0.05, 0.03, (0.15, 0.91, 0.05), "joint", seg=8)
    a.box((0.2, 0.2, 0.02), (0, 0.5, 0.3), "hazard")
    return a

def crate():
    a = Asset("rs_crate", M)
    a.box((1.0, 1.0, 1.0), (0, 0.5, 0), "wood", bevel=0.02)
    for s in (-1, 1):                                                         # frame planks
        a.box((1.04, 0.12, 0.04), (0, 0.06, s * 0.5), "wooddark")
        a.box((1.04, 0.12, 0.04), (0, 0.94, s * 0.5), "wooddark")
        a.box((1.3, 0.12, 0.04), (0, 0.5, s * 0.5), "wooddark", rot=rot_e(rz=45 * s))
        a.box((0.04, 0.12, 1.04), (s * 0.5, 0.06, 0), "wooddark")
        a.box((0.04, 0.12, 1.04), (s * 0.5, 0.94, 0), "wooddark")
    return a

def tire_stack():
    a = Asset("rs_tire_stack", M)
    for i in range(4):
        a.torus(0.33, 0.12, (0.02 * (i % 2), 0.12 + i * 0.23, 0.02 * (i // 2)), "tire", seg=20, sides=8)
    return a

def workbench():
    """Work bench with vise and tools. 2.2 x 1.0 x 0.8."""
    a = Asset("rs_workbench", M)
    a.box((2.2, 0.08, 0.8), (0, 0.92, 0), "wood")
    a.box((2.1, 0.05, 0.7), (0, 0.3, 0), "wooddark")
    for x in (-1.0, 1.0):
        for z in (-0.32, 0.32):
            a.box((0.08, 0.92, 0.08), (x, 0.46, z), "gunmetal")
    a.box((2.2, 0.9, 0.04), (0, 1.4, -0.38), "rust")                        # pegboard
    a.box((0.2, 0.14, 0.18), (0.7, 1.03, 0.25), "bluepaint", bevel=0.02)    # vise
    a.box((0.04, 0.04, 0.24), (0.7, 1.03, 0.4), "plate")
    a.box((0.04, 0.3, 0.02), (-0.6, 1.45, -0.35), "plate")                  # hung wrench
    a.box((0.3, 0.05, 0.05), (-0.2, 1.6, -0.35), "joint")                   # hung hammer
    a.box((0.08, 0.1, 0.1), (-0.05, 1.6, -0.35), "plate")
    a.cyl(0.12, 0.06, (-0.6, 0.98, 0.1), "oil", seg=12)                     # oil can
    return a

def toolchest():
    """Rolling tool chest. 1.0 x 1.2 x 0.6."""
    a = Asset("rs_toolchest", M)
    a.box((1.0, 1.05, 0.6), (0, 0.65, 0), "mobred", bevel=0.03)
    for i in range(5):
        a.box((0.92, 0.015, 0.02), (0, 0.32 + i * 0.2, 0.3), "joint")
        a.box((0.3, 0.03, 0.03), (0, 0.4 + i * 0.2, 0.32), "plate")
    for x in (-0.42, 0.42):
        for z in (-0.22, 0.22):
            a.cyl(0.06, 0.05, (x, 0.06, z), "tire", rot=rot_e(rz=90), seg=10)
    a.box((1.0, 0.04, 0.04), (0, 1.25, 0.3), "plate")
    return a

def sedan():
    """The mob's sedan, up on blocks. ~4.6 long along Z, 1.4 tall."""
    a = Asset("rs_sedan", M)
    a.box((1.8, 0.55, 4.6), (0, 0.65, 0), "oil", bevel=0.12)                 # body
    a.box((1.6, 0.5, 2.2), (0, 1.15, -0.2), "oil", bevel=0.12)               # cabin
    a.box((1.62, 0.38, 2.0), (0, 1.15, -0.2), "glass")
    a.box((1.84, 0.06, 4.64), (0, 0.72, 0), "gold")                          # pinstripe
    a.box((1.5, 0.22, 0.06), (0, 0.6, 2.31), "plate")                        # grille
    for x in (-0.6, 0.6):
        a.cyl(0.1, 0.05, (x, 0.75, 2.31), "lamp", rot=rot_e(rx=90), seg=12)
    for x in (-0.86, 0.86):
        for z in (-1.4, 1.5):
            a.cyl(0.35, 0.24, (x, 0.42, z), "tire", rot=rot_e(rz=90), seg=16)
            a.cyl(0.18, 0.26, (x, 0.42, z), "plate", rot=rot_e(rz=90), seg=12)
            a.box((0.3, 0.12, 0.3), (x * 0.9, 0.06, z), "concrete")         # blocks
    return a

def cash_stack():
    """Pallet of mob cash."""
    a = Asset("rs_cash_stack", M)
    a.box((1.0, 0.12, 0.8), (0, 0.06, 0), "wooddark")
    for i in range(3):
        for x in (-0.25, 0.25):
            for z in (-0.18, 0.18):
                a.box((0.44, 0.14, 0.3), (x, 0.19 + i * 0.15, z), "cash", bevel=0.01)
                a.box((0.06, 0.145, 0.31), (x, 0.19 + i * 0.15, z), "gold")
    return a

def don_desk():
    """The Don's desk: leather chair, lamp, gold trim."""
    a = Asset("rs_don_desk", M)
    a.box((2.0, 0.08, 0.9), (0, 0.8, 0), "wooddark")
    a.box((2.04, 0.04, 0.94), (0, 0.76, 0), "gold")
    for x in (-0.75, 0.75):
        a.box((0.45, 0.76, 0.85), (x, 0.38, 0), "wood")
    a.box((1.0, 0.6, 0.05), (0, 0.45, 0.4), "wood")                          # modesty panel
    a.cyl(0.1, 0.03, (0.6, 0.85, -0.15), "gold", seg=12)                     # lamp
    a.cyl(0.015, 0.3, (0.6, 1.0, -0.15), "gold", seg=6)
    a.cyl(0.15, 0.12, (0.6, 1.18, -0.15), "green", seg=12, r2=0.08)
    a.box((0.3, 0.02, 0.22), (-0.3, 0.85, 0.05), "cash")
    a.box((0.8, 0.12, 0.7), (0, 0.5, -0.9), "leather", bevel=0.05)           # chair seat
    a.box((0.8, 1.1, 0.14), (0, 1.0, -1.25), "leather", bevel=0.05)          # chair back
    a.cyl(0.05, 0.4, (0, 0.24, -0.9), "gunmetal", seg=8)
    a.cyl(0.3, 0.04, (0, 0.03, -0.9), "gunmetal", seg=5, smooth=False)
    return a

def hanging_light():
    """Industrial pendant light hung from a chain (origin at ceiling point, hangs down to y=-1.2)."""
    a = Asset("rs_hanging_light", M)
    for i in range(8):
        a.torus(0.04, 0.012, (0, -0.06 - i * 0.09, 0), "gunmetal", rot=rot_e(rx=90 * (i % 2)), seg=8, sides=4)
    a.cyl(0.35, 0.3, (0, -0.95, 0), "bluepaint", seg=16, r2=0.08)
    a.sphere(0.12, (0, -1.1, 0), "lamp", seg=10, rings=6)
    return a

def build_all():
    return [robot(), knife(), shutter(), oil_drum(), crate(), tire_stack(), workbench(),
            toolchest(), sedan(), cash_stack(), don_desk(), hanging_light()]
