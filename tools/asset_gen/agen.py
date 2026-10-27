"""Procedural low-poly asset builder for VoxelEngine (run inside Blender).

Every asset is one mesh object built from primitives straight into a bmesh,
one material per colour. GltfModel reads one baseColorFactor per primitive,
so a glTF primitive per material is exactly what the engine wants.

Conventions (match GltfModel / make_car.py):
  * engine units = metres, Y-up, forward = +Z (glTF)
  * Blender is Z-up; the glTF exporter maps Blender -Y -> glTF +Z, so the
    helpers here take engine-space coordinates (x, y-up, z-forward) and convert.
  * origin at the centre of the footprint, on the ground.
"""
import bpy, bmesh, math
from mathutils import Matrix, Vector, Euler

# ---------------------------------------------------------------- materials
def material(name, rgb, metal=0.0, rough=0.6):
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    m.use_nodes = True
    bsdf = next(n for n in m.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
    bsdf.inputs["Base Color"].default_value = (*rgb, 1.0)
    bsdf.inputs["Metallic"].default_value = metal
    bsdf.inputs["Roughness"].default_value = rough
    m.diffuse_color = (*rgb, 1.0)
    return m

def palette(prefix, spec):
    """spec: {key: (rgb, metal, rough)} -> {key: Material} named prefix_key."""
    return {k: material(f"{prefix}_{k}", *v) for k, v in spec.items()}

# ---------------------------------------------------------------- coordinates
def E(x, y, z):
    """Engine (x, y-up, z-forward) -> Blender (x, -z, y)."""
    return Vector((x, -z, y))

def rot_e(rx=0.0, ry=0.0, rz=0.0):
    """Rotation in engine axes, degrees: rx pitch about X, ry yaw about up, rz roll about forward."""
    return (Matrix.Rotation(math.radians(ry), 4, "Z")
            @ Matrix.Rotation(math.radians(rx), 4, "X")
            @ Matrix.Rotation(math.radians(-rz), 4, "Y"))

# ---------------------------------------------------------------- asset
class Asset:
    def __init__(self, name, mats):
        self.name, self.mats = name, mats
        self.order = []          # material keys in slot order
        self.bm = bmesh.new()
        self.clip_ground = False  # cut away everything below y=0
        self.scale = 1.0          # uniform scale applied at build (about the ground origin)

    def _slot(self, key):
        if key not in self.order:
            self.order.append(key)
        return self.order.index(key)

    def _tag(self, verts, key, smooth=False):
        idx = self._slot(key)
        faces = {f for v in verts for f in v.link_faces}
        for f in faces:
            f.material_index = idx
            f.smooth = smooth
        return faces

    def _M(self, loc, rot, scale):
        S = Matrix.Diagonal((scale[0], scale[2], scale[1], 1.0))  # engine xyz -> blender x,z(depth),y
        return Matrix.Translation(E(*loc)) @ rot @ S

    # -- primitives (all sizes in engine axes: sx width, sy height, sz depth)
    def box(self, size, loc, m, rot=None, bevel=0.0, segments=1, smooth=False):
        M = self._M(loc, rot or Matrix(), size)
        r = bmesh.ops.create_cube(self.bm, size=1.0, matrix=M)
        faces = self._tag(r["verts"], m, smooth)
        if bevel > 0:
            edges = list({e for f in faces for e in f.edges})
            res = bmesh.ops.bevel(self.bm, geom=edges, offset=bevel, segments=segments,
                                  affect="EDGES", profile=0.5)
            idx = self._slot(m)
            for f in res.get("faces", []):     # bevel faces inherit the material + shading
                f.material_index = idx; f.smooth = smooth or segments > 1
        return r

    def _along(self, p0, p1):
        """Rotation taking the primitive's up axis onto the direction p0->p1 (engine coords)."""
        d = E(*p1) - E(*p0)
        return Vector((0, 0, 1)).rotation_difference(d.normalized()).to_matrix().to_4x4(), d.length

    def tube(self, p0, p1, r, m, seg=12, r2=None, smooth=True):
        """Cylinder/cone from engine point p0 to p1."""
        R, L = self._along(p0, p1)
        mid = tuple((a + b) / 2 for a, b in zip(p0, p1))
        return self.cyl(r, L, mid, m, rot=R, seg=seg, r2=r2, smooth=smooth)

    def capsule(self, p0, p1, r, m, seg=12, r2=None):
        """Rounded limb: tube with a sphere on each end (fingers, arms)."""
        self.tube(p0, p1, r, m, seg=seg, r2=r2)
        self.sphere(r, p0, m, seg=seg, rings=max(6, seg // 2))
        self.sphere(r if r2 is None else r2, p1, m, seg=seg, rings=max(6, seg // 2))

    def cyl(self, r, h, loc, m, rot=None, seg=16, r2=None, smooth=True):
        """Upright cylinder/cone centred at loc. r2 = top radius."""
        M = self._M(loc, rot or Matrix(), (1, 1, 1))
        res = bmesh.ops.create_cone(self.bm, cap_ends=True, cap_tris=False,
                                    segments=seg, radius1=r,
                                    radius2=r if r2 is None else r2,
                                    depth=h, matrix=M)
        return self._tag(res["verts"], m, smooth)

    def sphere(self, r, loc, m, scale=(1, 1, 1), rot=None, seg=16, rings=10, smooth=True):
        M = self._M(loc, rot or Matrix(), scale)
        res = bmesh.ops.create_uvsphere(self.bm, u_segments=seg, v_segments=rings,
                                        radius=r, matrix=M)
        return self._tag(res["verts"], m, smooth)

    def dome(self, r, loc, m, scale=(1, 1, 1), seg=20, rings=10, smooth=True):
        """Upper hemisphere sitting on loc (flat side down)."""
        M = self._M(loc, Matrix(), scale)
        res = bmesh.ops.create_uvsphere(self.bm, u_segments=seg, v_segments=rings * 2,
                                        radius=r, matrix=M)
        below = [v for v in res["verts"] if (M.inverted() @ v.co).z < -1e-4]
        bmesh.ops.delete(self.bm, geom=below, context="VERTS")
        keep = [v for v in res["verts"] if v.is_valid]
        edges = [e for e in self.bm.edges if e.is_boundary and all(v in keep for v in e.verts)]
        if edges:
            bmesh.ops.edgeloop_fill(self.bm, edges=edges)
        return self._tag(keep, m, smooth)

    def torus(self, R, r, loc, m, rot=None, seg=24, sides=8):
        """Ring lying flat (axis = up) unless rotated."""
        M = self._M(loc, rot or Matrix(), (1, 1, 1))
        verts = []
        for i in range(seg):
            a = 2 * math.pi * i / seg
            for j in range(sides):
                b = 2 * math.pi * j / sides
                p = Vector(((R + r * math.cos(b)) * math.cos(a),
                            (R + r * math.cos(b)) * math.sin(a),
                            r * math.sin(b)))
                verts.append(self.bm.verts.new(M @ p))
        for i in range(seg):
            for j in range(sides):
                a = verts[i * sides + j]; b = verts[((i + 1) % seg) * sides + j]
                c = verts[((i + 1) % seg) * sides + (j + 1) % sides]; d = verts[i * sides + (j + 1) % sides]
                self.bm.faces.new((a, b, c, d))
        return self._tag(verts, m, True)

    def wedge(self, size, loc, m, rot=None):
        """Ramp: full height at -Z (back), zero at +Z (front)."""
        sx, sy, sz = size
        M = self._M(loc, rot or Matrix(), (1, 1, 1))
        P = [(-sx/2, 0, -sz/2), (sx/2, 0, -sz/2), (sx/2, 0, sz/2), (-sx/2, 0, sz/2),
             (-sx/2, sy, -sz/2), (sx/2, sy, -sz/2)]
        v = [self.bm.verts.new(M @ E(*p)) for p in P]
        for f in ((0, 3, 2, 1), (0, 1, 5, 4), (3, 0, 4), (1, 2, 5), (2, 3, 4, 5)):
            self.bm.faces.new([v[i] for i in f])
        return self._tag(v, m)

    def lathe(self, profile, loc, rot=None, scale=(1, 1, 1), seg=32):
        """Revolve a profile about the local forward (+Z) axis.
        profile: [(radius, z, material), ...] from back to front; a segment takes
        the material of its starting point. Zero radius at an end closes it."""
        M = self._M(loc, rot or Matrix(), scale)
        rings = []
        for r, z, _ in profile:
            if r <= 1e-6:
                rings.append([self.bm.verts.new(M @ E(0, 0, z))])
            else:
                rings.append([self.bm.verts.new(M @ E(r * math.cos(2 * math.pi * i / seg),
                                                      r * math.sin(2 * math.pi * i / seg), z))
                              for i in range(seg)])
        for k in range(len(rings) - 1):
            a, b, key = rings[k], rings[k + 1], profile[k][2]
            idx = self._slot(key)
            for i in range(seg):
                if len(a) == 1:   quad = (a[0], b[(i + 1) % seg], b[i])
                elif len(b) == 1: quad = (a[i], a[(i + 1) % seg], b[0])
                else:             quad = (a[i], a[(i + 1) % seg], b[(i + 1) % seg], b[i])
                f = self.bm.faces.new(quad); f.material_index = idx; f.smooth = True
        return rings

    def slab(self, outline, thick, loc, m, rot=None):
        """Flat cut-out shape (fins, plates): outline is [(z, y), ...] in the
        forward/up plane, extruded ±thick/2 along X."""
        M = self._M(loc, rot or Matrix(), (1, 1, 1))
        L = [self.bm.verts.new(M @ E(-thick / 2, y, z)) for z, y in outline]
        R = [self.bm.verts.new(M @ E(thick / 2, y, z)) for z, y in outline]
        faces = [self.bm.faces.new(L), self.bm.faces.new(list(reversed(R)))]
        n = len(outline)
        for i in range(n):
            faces.append(self.bm.faces.new((L[i], R[i], R[(i + 1) % n], L[(i + 1) % n])))
        idx = self._slot(m)
        for f in faces: f.material_index = idx; f.smooth = False
        return faces

    # -- output
    def build(self, collection, at=(0, 0, 0), sharp_deg=50):
        bm = self.bm
        if self.clip_ground:
            geom = bm.verts[:] + bm.edges[:] + bm.faces[:]
            bmesh.ops.bisect_plane(bm, geom=geom, dist=1e-5, plane_co=(0, 0, 0),
                                   plane_no=(0, 0, 1), clear_inner=True)
        if self.scale != 1.0:
            bmesh.ops.scale(bm, vec=(self.scale,) * 3, verts=bm.verts)
        bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-5)
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
        for e in bm.edges:  # crisp hard edges, soft curved surfaces
            if e.is_manifold and e.calc_face_angle(0) > math.radians(sharp_deg):
                e.smooth = False
        old = bpy.data.objects.get(self.name)
        if old:
            bpy.data.objects.remove(old, do_unlink=True)
        me = bpy.data.meshes.get(self.name) or bpy.data.meshes.new(self.name)
        me.clear_geometry()
        me.materials.clear()          # slots first: clearing after to_mesh zeroes face indices
        for k in self.order:
            me.materials.append(self.mats[k])
        bm.to_mesh(me); bm.free()
        ob = bpy.data.objects.new(self.name, me)
        ob.location = at
        collection.objects.link(ob)
        return ob

# ---------------------------------------------------------------- scene/export
def collection(name):
    c = bpy.data.collections.get(name)
    if not c:
        c = bpy.data.collections.new(name)
        bpy.context.scene.collection.children.link(c)
    return c

def lineup(objs, gap=1.5, row_y=0.0):
    """Lay objects out along Blender X for viewing; returns them."""
    x = 0.0
    for ob in objs:
        w = ob.dimensions.x
        ob.location = (x + w / 2, row_y, 0)
        x += w + gap
    return objs

def export(ob, path, extra=()):
    """Export an object (plus `extra` parts sharing its frame) to one GLB at its
    own origin (locations temporarily zeroed)."""
    obs = [ob, *extra]
    keep = [o.location.copy() for o in obs]
    for o in obs: o.location = (0, 0, 0)
    for o in bpy.context.view_layer.objects:
        o.select_set(False)
    for o in obs: o.select_set(True)
    bpy.context.view_layer.objects.active = ob
    try:
        bpy.ops.export_scene.gltf(filepath=path, export_format="GLB", use_selection=True,
                                  export_yup=True, export_apply=True, export_texcoords=False,
                                  export_normals=True, export_materials="EXPORT",
                                  export_cameras=False, export_lights=False, export_extras=False)
    finally:
        for o, k in zip(obs, keep): o.location = k

def run(modname, colname, row_y=0.0, gap=1.0):
    """Reload a game module, rebuild all its assets into a collection, lay them out."""
    import importlib, sys
    importlib.reload(sys.modules[__name__])
    mod = importlib.reload(importlib.import_module(modname))
    col = collection(colname)
    objs = [a.build(col) for a in mod.build_all()]
    return lineup(objs, gap=gap, row_y=row_y)

def export_collection(colname, outdir):
    """Export every mesh object in a collection to outdir/<name>.glb."""
    import os
    os.makedirs(outdir, exist_ok=True)
    out = []
    for ob in bpy.data.collections[colname].objects:
        if ob.type == "MESH":
            p = os.path.join(outdir, ob.name + ".glb")
            export(ob, p)
            out.append((ob.name, os.path.getsize(p)))
    return out

def viewmodel_camera(origin_blender, fwd=0.55, right=0.26, down=0.24, s=0.6, name="VM_Eye"):
    """Camera at the player's eye for a first-person pose: the view model's
    origin sits at eye + fwd*f + right*r - down*u, scaled s, model +Z forward
    and model -X to the right. 70° vertical FOV, 16:9."""
    import math
    cam = bpy.data.objects.get(name)
    if not cam:
        cam = bpy.data.objects.new(name, bpy.data.cameras.new(name))
        bpy.context.scene.collection.objects.link(cam)
    ox, oy, oz = origin_blender
    cam.location = (ox + right / s, oy + fwd / s, oz + down / s)   # engine (x, y, z) -> blender (x, -z, y)
    cam.rotation_euler = (math.radians(90), 0, math.radians(180))  # look along engine +Z (blender -Y)
    cam.data.sensor_fit = "VERTICAL"
    cam.data.angle_y = math.radians(70)
    cam.data.clip_start = 0.05 / s
    sc = bpy.context.scene
    sc.camera = cam
    sc.render.resolution_x, sc.render.resolution_y = 1600, 900
    return cam


def sculpt(ob, voxel=0.0025, smooth=4, keep_faces=4000):
    """Fuse an object's overlapping primitives into one smooth organic surface:
    voxel remesh -> smooth -> decimate, baked into the mesh (one material)."""
    mods = []
    r = ob.modifiers.new("remesh", "REMESH"); r.mode = "VOXEL"; r.voxel_size = voxel; mods.append(r)
    if smooth:
        sm = ob.modifiers.new("smooth", "SMOOTH"); sm.factor = 0.6; sm.iterations = smooth; mods.append(sm)
    dg = bpy.context.evaluated_depsgraph_get()
    n = len(ob.evaluated_get(dg).data.polygons)
    if n > keep_faces:
        d = ob.modifiers.new("decimate", "DECIMATE"); d.ratio = keep_faces / n; mods.append(d)
    dg = bpy.context.evaluated_depsgraph_get()
    me = bpy.data.meshes.new_from_object(ob.evaluated_get(dg))
    for m in mods: ob.modifiers.remove(m)
    old = ob.data; ob.data = me; me.name = old.name
    bpy.data.meshes.remove(old)
    for p in me.polygons: p.use_smooth = True
    return ob
