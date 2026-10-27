"""Iron Command first-person view model: an atomic-age ray gun and the gloved
right hand that holds it (Iron Command palette: chrome / teal / coral / cream).

Real size (metres). Frame shared by both files so they line up under one matrix:
  origin = the top of the grip, under the body, where the hand closes;
  forward = +Z (the barrel), up = +Y, the holder's right = -X.
main.cpp places that origin in front of the eye and maps model -X to the
camera's right, +Y to up, +Z to forward (a pure rotation, scale 1).
"""
import math
from agen import Asset, palette, rot_e
from ironcommand import M as ICM

M = dict(ICM)
M.update(palette("vm", {
    "chromeDk": ((0.52, 0.55, 0.60), 0.9, 0.3),     # chrome in shadow / grooves
    "enamel":   ((0.12, 0.52, 0.55), 0.2, 0.35),    # deep teal enamel
    "bakelite": ((0.86, 0.80, 0.68), 0.0, 0.45),    # cream grip
    "glove":    ((0.92, 0.90, 0.85), 0.0, 0.6),
    "gauntlet": ((0.16, 0.56, 0.58), 0.1, 0.45),
    "suit":     ((0.86, 0.85, 0.80), 0.0, 0.6),
    "suitDk":   ((0.66, 0.66, 0.63), 0.0, 0.6),
}))

BODY_Y = 0.045          # body axis height above the grip top
GRIP_RAKE = 16          # degrees the grip leans back

def raygun():
    a = Asset("ic_raygun", M)
    y = BODY_Y
    # ── fuselage + barrel, one turned profile, tail to muzzle ──────────────
    C, D, T, K, G = "chrome", "chromeDk", "enamel", "coral", "glow"
    prof = [
        (0.0,   -0.150, C), (0.014, -0.149, C), (0.024, -0.142, C), (0.030, -0.130, D),   # tail cap
        (0.031, -0.120, K), (0.036, -0.118, K), (0.036, -0.108, C),                        # coral collar
        (0.039, -0.104, T), (0.046, -0.085, T), (0.050, -0.055, T), (0.051, -0.020, T),  # enamel belly
        (0.049,  0.010, C), (0.053,  0.012, C), (0.053,  0.026, D), (0.048,  0.028, C),  # chrome band
        (0.046,  0.045, C), (0.040,  0.075, C), (0.031,  0.105, C), (0.022,  0.130, D),  # tapering nose
        (0.013,  0.138, C), (0.011,  0.165, C),                                           # barrel
        (0.034,  0.170, K), (0.034,  0.177, C), (0.011,  0.183, C), (0.011,  0.198, C),  # emitter disc 1
        (0.027,  0.203, K), (0.027,  0.209, C), (0.011,  0.214, C), (0.011,  0.226, C),  # disc 2
        (0.020,  0.230, K), (0.020,  0.235, C), (0.010,  0.239, C), (0.010,  0.246, G),  # disc 3
        (0.014,  0.250, G), (0.012,  0.258, G), (0.0,    0.262, G),                       # glowing tip
    ]
    a.lathe(prof, (0, y, 0), seg=40)
    # ── fins: swept dorsal + two tail fins rolled 125° either side ─────────
    dorsal = [(-0.105, 0.0), (-0.150, 0.075), (-0.122, 0.080), (-0.020, 0.0)]
    a.slab(dorsal, 0.006, (0, y + 0.03, 0), "coral")
    tail = [(-0.095, 0.0), (-0.150, 0.068), (-0.128, 0.072), (-0.030, 0.0)]
    for roll in (125, -125):
        r = math.radians(roll)
        a.slab(tail, 0.006, (0.03 * math.sin(r), y + 0.03 * math.cos(r), 0), "coral", rot=rot_e(rz=roll))
    # ── side detail: a chrome-ringed glowing gauge on each flank ───────────
    for sx in (1, -1):
        a.cyl(0.017, 0.012, (sx * 0.047, y + 0.004, -0.045), "chrome", rot=rot_e(rz=90), seg=20)
        a.cyl(0.012, 0.014, (sx * 0.048, y + 0.004, -0.045), "glow", rot=rot_e(rz=90), seg=20)
    _grip(a)
    return a

def _grip(a):
    """Turned bakelite grip with finger ridges, raked back, chrome pommel; trigger + guard.
    Shared by every hand-held tool so the same glove fits them all."""
    gp = [(0.0, -0.002, "bakelite"), (0.024, 0.0, "bakelite"), (0.025, 0.020, "bakelite"),
          (0.022, 0.034, "bakelite"), (0.025, 0.048, "bakelite"), (0.022, 0.062, "bakelite"),
          (0.025, 0.076, "bakelite"), (0.023, 0.090, "bakelite"), (0.027, 0.104, "chrome"),
          (0.030, 0.110, "chrome"), (0.030, 0.118, "chromeDk"), (0.0, 0.121, "chromeDk")]
    a.lathe(gp, (0, 0.012, -0.03), rot=rot_e(rx=90 + GRIP_RAKE), scale=(0.78, 1.15, 1), seg=24)   # narrow, deep
    a.slab([(0.0, 0.0), (0.012, -0.004), (0.016, -0.026), (0.008, -0.03), (0.004, -0.008)],
           0.008, (0, -0.002, 0.004), "coral")
    a.torus(0.024, 0.004, (0, -0.012, 0.012), "chrome", rot=rot_e(rz=90), seg=20, sides=6)

# ── the hand ────────────────────────────────────────────────────────────────
def _grip_frame(side=1):
    """Point on the grip: x across (+ = toward the thumb side), d down the grip, w toward its front.
    side=+1 for a right hand (thumb on +X), -1 mirrors it into a left hand."""
    r = math.radians(GRIP_RAKE)
    top = (0.0, 0.012, -0.03)
    down = (0.0, -math.cos(r), -math.sin(r))
    front = (0.0, -math.sin(r), math.cos(r))
    def P(x, d, w):
        return (top[0] + side * x, top[1] + d * down[1] + w * front[1], top[2] + d * down[2] + w * front[2])
    return P

def glove(side=1):
    """The gloved hand as overlapping blobs; sculpt() later fuses it into one skin."""
    a = Asset("ic_hand_%s_glove" % ("right" if side > 0 else "left"), M)
    P = _grip_frame(side)
    rk = rot_e(rx=GRIP_RAKE)
    # back of the hand (holder's right) and the palm heel wrapping behind the grip
    a.sphere(1.0, P(-0.03, 0.055, -0.004), "glove", scale=(0.02, 0.055, 0.042), rot=rk, seg=20, rings=12)
    a.sphere(1.0, P(-0.008, 0.07, -0.038), "glove", scale=(0.03, 0.05, 0.022), rot=rk, seg=20, rings=12)
    # middle, ring, little fingers: three gloved segments curling round the front
    for i, d in enumerate((0.046, 0.071, 0.095)):
        rr = 0.0118 - i * 0.0009
        pts = [P(-0.031, d, 0.016), P(-0.012, d + 0.002, 0.04), P(0.014, d + 0.004, 0.039), P(0.029, d + 0.005, 0.016)]
        for p0, p1 in zip(pts, pts[1:]):
            a.capsule(p0, p1, rr, "glove", seg=12)
    # index finger along the trigger
    for p0, p1 in [(P(-0.032, 0.018, 0.018), P(-0.012, 0.02, 0.05)), (P(-0.012, 0.02, 0.05), P(0.004, 0.026, 0.06))]:
        a.capsule(p0, p1, 0.0115, "glove", seg=12)
    # thumb: from the web, over the grip's top-left, along the left flank
    for p0, p1 in [(P(-0.026, -0.004, -0.036), P(0.012, -0.006, -0.028)),
                   (P(0.012, -0.006, -0.028), P(0.03, 0.008, 0.002)),
                   (P(0.03, 0.008, 0.002), P(0.033, 0.022, 0.026))]:
        a.capsule(p0, p1, 0.0128, "glove", seg=12)
    # wrist, heading back toward the elbow
    a.capsule(P(-0.016, 0.085, -0.045), P(-0.026, 0.13, -0.075), 0.03, "glove", seg=16)
    return a

def sleeve(side=1):
    """Flared gauntlet cuff + ribbed spacesuit sleeve running back out of view."""
    a = Asset("ic_hand_%s_sleeve" % ("right" if side > 0 else "left"), M)
    P = _grip_frame(side)
    w0 = P(-0.026, 0.13, -0.075)                       # wrist end of the glove
    # forearm axis: back, down and out to the right
    fa = (-0.22 * side, -0.32, -0.92); L = math.sqrt(sum(c * c for c in fa)); fa = tuple(c / L for c in fa)
    R, _ = a._along((0, 0, 0), fa)                     # blender up -> forearm axis
    to_axis = R @ rot_e(rx=-90)                        # lathe axis (engine +Z = blender -Y) -> blender up first
    prof = [(0.030, -0.01, "gauntlet"), (0.036, 0.0, "gauntlet"), (0.052, 0.03, "gauntlet"),
            (0.060, 0.055, "coral"), (0.060, 0.063, "gauntlet"), (0.050, 0.068, "suitDk")]
    z = 0.068
    for k in range(5):                                  # accordion ribs
        prof += [(0.046, z + 0.004, "suit"), (0.050, z + 0.016, "suitDk"), (0.046, z + 0.028, "suit")]
        z += 0.028
    prof += [(0.048, z + 0.01, "chrome"), (0.056, z + 0.014, "chrome"), (0.056, z + 0.03, "chromeDk"),
             (0.050, z + 0.034, "suit"), (0.058, z + 0.4, "suit"), (0.0, z + 0.41, "suit")]
    a.lathe(prof, w0, rot=to_axis, seg=32)
    return a

# ── the Fabricator: the build tool, held in the left glove ───────────────────
FAB_Y = 0.05            # housing / projector axis height above the grip top
RING_Z = 0.118          # where the gyroscope rings sit along the barrel

def fabricator():
    a = Asset("ic_fabricator", M)
    y = FAB_Y
    _grip(a)
    # housing: a soft cream block with a chrome waist and coral trim
    a.box((0.078, 0.072, 0.17), (0, y, -0.01), "cream", bevel=0.022, segments=3, smooth=True)
    a.box((0.082, 0.074, 0.016), (0, y, -0.02), "chrome", bevel=0.006, segments=2, smooth=True)
    a.box((0.08, 0.008, 0.15), (0, y - 0.03, -0.01), "coral", bevel=0.003, segments=1)
    # projector barrel flaring into a parabolic emitter dish
    C, D, K, T = "chrome", "chromeDk", "coral", "enamel"
    prof = [(0.0, 0.07, D), (0.026, 0.07, C), (0.030, 0.078, C), (0.030, 0.092, K), (0.026, 0.096, D),
            (0.024, 0.098, C), (0.024, 0.14, C), (0.030, 0.146, C), (0.036, 0.158, C), (0.050, 0.178, C),
            (0.062, 0.196, K), (0.062, 0.202, K), (0.056, 0.204, D), (0.036, 0.196, D), (0.016, 0.188, D),
            (0.0, 0.186, D)]
    a.lathe(prof, (0, y, 0), seg=40)
    a.tube((0, y, 0.186), (0, y, 0.236), 0.0045, "chrome", seg=10, r2=0.002)      # emitter needle
    a.sphere(0.009, (0, y, 0.24), "glow", seg=14, rings=8)
    for x in (0.026, -0.026):                                                     # needle struts
        a.tube((x * 1.6, y, 0.196), (0, y, 0.228), 0.0018, "chrome", seg=6)
    # blueprint screen on top, tilted back toward the holder
    scr = (0, y + 0.04, -0.045)
    a.cyl(0.03, 0.016, scr, "chrome", rot=rot_e(rx=-38), seg=28)
    a.cyl(0.024, 0.018, scr, "glow", rot=rot_e(rx=-38), seg=28)
    for k in (-1, 0, 1):                                                          # blueprint grid
        a.box((0.04, 0.002, 0.003), (0, scr[1] + 0.0095 + k * 0.006, scr[2] - 0.0075 + k * 0.0075),
              "enamel", rot=rot_e(rx=-38))
        a.box((0.003, 0.002, 0.034), (k * 0.01, scr[1] + 0.0095, scr[2] - 0.0075), "enamel", rot=rot_e(rx=-38))
    # glass canisters on both flanks
    for x in (0.046, -0.046):
        a.tube((x, y - 0.008, -0.07), (x, y - 0.008, 0.03), 0.011, "glow", seg=14)
        for z in (-0.072, 0.032):
            a.cyl(0.013, 0.008, (x, y - 0.008, z), "chrome", rot=rot_e(rx=90), seg=14)
    # whip antenna with a coral bulb
    a.tube((-0.03, y + 0.03, -0.08), (-0.034, y + 0.11, -0.1), 0.0018, "chrome", seg=6)
    a.sphere(0.006, (-0.034, y + 0.112, -0.1), "coral", seg=10, rings=6)
    return a

def fabricator_rings():
    """Two tilted gyroscope rings with glowing beads; main.cpp spins them about the barrel."""
    a = Asset("ic_fabricator_rings", M)
    for tilt, col in ((32, "enamel"), (-32, "coral")):
        a.torus(0.044, 0.0035, (0, FAB_Y, RING_Z), col, rot=rot_e(ry=tilt) @ rot_e(rx=90), seg=36, sides=6)
        for k in range(3):
            ang = math.radians(120 * k + (60 if tilt < 0 else 0))
            # bead positions on the tilted ring (computed in the ring's own plane)
            px, py = 0.044 * math.cos(ang), 0.044 * math.sin(ang)
            ct, st = math.cos(math.radians(tilt)), math.sin(math.radians(tilt))
            a.sphere(0.006, (px * ct, FAB_Y + py, RING_Z - px * st), "glow", seg=10, rings=6)
    return a

def build_all():
    return [raygun(), glove(1), sleeve(1), fabricator(), fabricator_rings(), glove(-1), sleeve(-1)]
