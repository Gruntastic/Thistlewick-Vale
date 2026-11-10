"""Tiny pure-Python mesh builder used by make_placeholders.py and env_kit.py.

All coordinates are centimeters in final export space (+X forward, +Z up). Every face carries a
palette cell (ART_CONTRACT section 2) and an optional secondary-tint flag (UV1.x > 0.5, section 6).
Geometry is flat shaded; triangulation happens in ue5_export.export_asset.
"""
import math
import random

import bmesh
import bpy
from mathutils import Euler, Matrix, Vector

import ue5_export as ex


class MeshBuilder:
    def __init__(self, name):
        self.name = name
        self.verts = []
        self.faces = []
        self.cells = []
        self.secondary = []

    def _add(self, pts, faces, cell, sec, rot, origin):
        m = Euler(tuple(math.radians(a) for a in rot), "XYZ").to_matrix() if rot else Matrix.Identity(3)
        o = Vector(origin)
        base = len(self.verts)
        for p in pts:
            self.verts.append(tuple(o + m @ Vector(p)))
        for f in faces:
            self.faces.append(tuple(base + i for i in f))
            self.cells.append(cell)
            self.secondary.append(sec)

    def tri_count(self):
        return sum(len(f) - 2 for f in self.faces)

    def box(self, center, size, cell, rot=None, sec=False):
        sx, sy, sz = (s / 2 for s in size)
        pts = [(-sx, -sy, -sz), (sx, -sy, -sz), (sx, sy, -sz), (-sx, sy, -sz),
               (-sx, -sy, sz), (sx, -sy, sz), (sx, sy, sz), (-sx, sy, sz)]
        faces = [(0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)]
        self._add(pts, faces, cell, sec, rot, center)

    def box_min(self, mn, mx, cell, sec=False):
        c = tuple((a + b) / 2 for a, b in zip(mn, mx))
        s = tuple(b - a for a, b in zip(mn, mx))
        self.box(c, s, cell, sec=sec)

    def frustum(self, base, r0, r1, h, sides, cell, rot=None, sec=False, rz=0.0, caps=True):
        """Z-axis frustum, bottom centre at `base`. r1 ~ 0 gives a cone."""
        pts, faces = [], []
        for i in range(sides):
            a = 2 * math.pi * i / sides + math.radians(rz)
            pts.append((r0 * math.cos(a), r0 * math.sin(a), 0))
        if r1 <= 1e-6:
            pts.append((0, 0, h))
            for i in range(sides):
                faces.append((i, (i + 1) % sides, sides))
        else:
            for i in range(sides):
                a = 2 * math.pi * i / sides + math.radians(rz)
                pts.append((r1 * math.cos(a), r1 * math.sin(a), h))
            for i in range(sides):
                j = (i + 1) % sides
                faces.append((i, j, sides + j, sides + i))
            if caps:
                faces.append(tuple(range(sides, 2 * sides)))
        if caps:
            faces.append(tuple(reversed(range(sides))))
        self._add(pts, faces, cell, sec, rot, base)

    def limb(self, p0, p1, r0, r1, sides, cell, sec=False, caps=True):
        """Frustum between two points (branches, legs, rails, antlers)."""
        a, b = Vector(p0), Vector(p1)
        d = b - a
        if d.length < 1e-4:
            return
        e = d.to_track_quat("Z", "Y").to_euler("XYZ")
        self.frustum(tuple(a), r0, r1, d.length, sides, cell, rot=tuple(math.degrees(x) for x in e),
                     sec=sec, caps=caps)

    def blob(self, center, radii, cell, seed=0, sub=0, jitter=0.12, sec=False):
        """Jittered icosphere: sub=0 is 20 tris, sub=1 is 80 tris."""
        t = (1 + 5 ** 0.5) / 2
        v = [(-1, t, 0), (1, t, 0), (-1, -t, 0), (1, -t, 0), (0, -1, t), (0, 1, t), (0, -1, -t), (0, 1, -t),
             (t, 0, -1), (t, 0, 1), (-t, 0, -1), (-t, 0, 1)]
        f = [(0, 11, 5), (0, 5, 1), (0, 1, 7), (0, 7, 10), (0, 10, 11), (1, 5, 9), (5, 11, 4), (11, 10, 2),
             (10, 7, 6), (7, 1, 8), (3, 9, 4), (3, 4, 2), (3, 2, 6), (3, 6, 8), (3, 8, 9), (4, 9, 5),
             (2, 4, 11), (6, 2, 10), (8, 6, 7), (9, 8, 1)]
        v = [tuple(Vector(p).normalized()) for p in v]
        if sub >= 1:
            cache, nf = {}, []

            def mid(a, b):
                k = (min(a, b), max(a, b))
                if k not in cache:
                    v.append(tuple(((Vector(v[a]) + Vector(v[b])) / 2).normalized()))
                    cache[k] = len(v) - 1
                return cache[k]
            for a, b, c in f:
                ab, bc, ca = mid(a, b), mid(b, c), mid(c, a)
                nf += [(a, ab, ca), (b, bc, ab), (c, ca, bc), (ab, bc, ca)]
            f = nf
        rnd = random.Random(seed)
        pts = []
        for p in v:
            k = 1 + rnd.uniform(-jitter, jitter)
            pts.append((p[0] * radii[0] * k, p[1] * radii[1] * k, p[2] * radii[2] * k))
        self._add(pts, f, cell, sec, None, center)

    def gable(self, base_min, base_max, ridge_h, cell, overhang=0.0, ridge_axis="x", sec=False):
        """Gable roof (triangular prism). base_min/max are (x, y, z) of the eave rectangle."""
        x0, y0, z = base_min
        x1, y1, _ = base_max
        x0 -= overhang; y0 -= overhang; x1 += overhang; y1 += overhang
        if ridge_axis == "x":
            ym = (y0 + y1) / 2
            pts = [(x0, y0, z), (x1, y0, z), (x1, y1, z), (x0, y1, z), (x0, ym, z + ridge_h), (x1, ym, z + ridge_h)]
            faces = [(0, 1, 5, 4), (2, 3, 4, 5), (3, 0, 4), (1, 2, 5), (0, 3, 2, 1)]
        else:
            xm = (x0 + x1) / 2
            pts = [(x0, y0, z), (x1, y0, z), (x1, y1, z), (x0, y1, z), (xm, y0, z + ridge_h), (xm, y1, z + ridge_h)]
            faces = [(0, 1, 4), (2, 3, 5), (1, 2, 5, 4), (3, 0, 4, 5), (0, 3, 2, 1)]
        self._add(pts, faces, cell, sec, None, (0, 0, 0))

    def pyramid(self, base_center, w, d, h, cell, rot=None, sec=False):
        pts = [(-w / 2, -d / 2, 0), (w / 2, -d / 2, 0), (w / 2, d / 2, 0), (-w / 2, d / 2, 0), (0, 0, h)]
        faces = [(0, 1, 4), (1, 2, 4), (2, 3, 4), (3, 0, 4), (0, 3, 2, 1)]
        self._add(pts, faces, cell, sec, rot, base_center)

    def wedge(self, mn, mx, cell, sec=False):
        """Ramp inside mn..mx that is full height at +X and drops to zero at -X (snouts, slopes)."""
        x0, y0, z0 = mn
        x1, y1, z1 = mx
        pts = [(x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0), (x1, y0, z1), (x1, y1, z1)]
        faces = [(0, 1, 4), (3, 5, 2), (0, 4, 5, 3), (1, 2, 5, 4), (0, 3, 2, 1)]
        self._add(pts, faces, cell, sec, None, (0, 0, 0))

    def build(self):
        mesh = bpy.data.meshes.new(self.name)
        mesh.from_pydata(self.verts, [], self.faces)
        mesh.update()
        obj = bpy.data.objects.new(self.name, mesh)
        bpy.context.scene.collection.objects.link(obj)
        bm = bmesh.new()
        bm.from_mesh(mesh)
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces)  # consistent outward winding
        bm.to_mesh(mesh)
        bm.free()
        for p in mesh.polygons:
            p.use_smooth = False
        ex.init_uvs(obj)
        by = {}
        for i, (c, s) in enumerate(zip(self.cells, self.secondary)):
            by.setdefault((c, s), []).append(i)
        for (c, s), idx in by.items():
            ex.assign_palette(obj, idx, c, secondary=s)
        return obj
