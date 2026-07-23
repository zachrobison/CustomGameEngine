#!/usr/bin/env python3
"""Generate a simple box-car GLB: body + cabin roof + 4 wheel blocks."""
import struct, json, math, os

verts = []  # flat list of floats: x y z nx ny nz r g b
idxs  = []  # flat list of uint16

def quad(p0, p1, p2, p3, n, col):
    base = len(verts) // 9
    for p in (p0, p1, p2, p3):
        verts.extend(p); verts.extend(n); verts.extend(col)
    idxs.extend([base, base+1, base+2, base, base+2, base+3])

def box(cx, cy, cz, hx, hy, hz, col):
    mn = [cx-hx, cy-hy, cz-hz]
    mx = [cx+hx, cy+hy, cz+hz]
    x0,y0,z0 = mn; x1,y1,z1 = mx
    # +Y (CCW seen from above)
    quad([x0,y1,z1],[x1,y1,z1],[x1,y1,z0],[x0,y1,z0], [0,1,0], col)
    # -Y (CCW seen from below)
    quad([x0,y0,z0],[x1,y0,z0],[x1,y0,z1],[x0,y0,z1], [0,-1,0], col)
    # +Z (front)
    quad([x0,y0,z1],[x1,y0,z1],[x1,y1,z1],[x0,y1,z1], [0,0,1], col)
    # -Z (back)
    quad([x1,y0,z0],[x0,y0,z0],[x0,y1,z0],[x1,y1,z0], [0,0,-1], col)
    # +X (right)
    quad([x1,y0,z1],[x1,y0,z0],[x1,y1,z0],[x1,y1,z1], [1,0,0], col)
    # -X (left)
    quad([x0,y0,z0],[x0,y0,z1],[x0,y1,z1],[x0,y1,z0], [-1,0,0], col)

# Dimensions (forward = +Z)
body_col  = [0.15, 0.35, 0.70]   # steel blue
cabin_col = [0.10, 0.22, 0.50]   # darker blue
wheel_col = [0.12, 0.12, 0.12]   # near-black rubber
rim_col   = [0.55, 0.55, 0.60]   # silver rim accent

box( 0.00, 0.40, 0.00,  1.80, 0.40, 0.90, body_col)   # main body
box( 0.00, 1.00, 0.10,  1.10, 0.30, 0.82, cabin_col)  # cabin roof

# Wheels: (±1.55, 0.25, ±0.70)
for wx, wz in [(-1.45, 0.72),(-1.45,-0.72),(1.45, 0.72),(1.45,-0.72)]:
    box(wx, 0.25, wz,  0.20, 0.25, 0.20, wheel_col)
    box(wx, 0.25, wz,  0.21, 0.12, 0.12, rim_col)

# --- pack binary ---
pos_data  = bytearray()
norm_data = bytearray()
col_data  = bytearray()
for i in range(0, len(verts), 9):
    for j in range(3): pos_data  += struct.pack('f', verts[i+j])
    for j in range(3): norm_data += struct.pack('f', verts[i+3+j])
    for j in range(3): col_data  += struct.pack('f', verts[i+6+j])

idx_data = bytearray()
for v in idxs: idx_data += struct.pack('H', v)
# pad to 4 bytes
while len(idx_data) % 4: idx_data += b'\x00'

nv = len(verts) // 9
ni = len(idxs)

# Compute bounding box of positions
px=[verts[i] for i in range(0,len(verts),9)]
py=[verts[i] for i in range(1,len(verts),9)]
pz=[verts[i] for i in range(2,len(verts),9)]
pos_min=[min(px),min(py),min(pz)]
pos_max=[max(px),max(py),max(pz)]

bin_data = pos_data + norm_data + col_data + idx_data
byte_offset = {
    'pos':  0,
    'norm': len(pos_data),
    'col':  len(pos_data)+len(norm_data),
    'idx':  len(pos_data)+len(norm_data)+len(col_data),
}

gltf = {
  "asset": {"version": "2.0"},
  "scene": 0,
  "scenes": [{"nodes":[0]}],
  "nodes":  [{"mesh": 0}],
  "meshes": [{"primitives":[{
      "attributes": {
          "POSITION":  0,
          "NORMAL":    1,
          "COLOR_0":   2
      },
      "indices": 3,
      "material": 0
  }]}],
  "materials": [{"pbrMetallicRoughness": {
      "baseColorFactor": [0.15, 0.35, 0.70, 1.0],
      "metallicFactor": 0.3,
      "roughnessFactor": 0.6
  }}],
  "accessors": [
      # 0 POSITION
      {"bufferView":0,"componentType":5126,"count":nv,"type":"VEC3",
       "min":pos_min,"max":pos_max},
      # 1 NORMAL
      {"bufferView":1,"componentType":5126,"count":nv,"type":"VEC3"},
      # 2 COLOR_0
      {"bufferView":2,"componentType":5126,"count":nv,"type":"VEC3"},
      # 3 indices
      {"bufferView":3,"componentType":5123,"count":ni,"type":"SCALAR"},
  ],
  "bufferViews": [
      {"buffer":0,"byteOffset":byte_offset['pos'], "byteLength":len(pos_data)},
      {"buffer":0,"byteOffset":byte_offset['norm'],"byteLength":len(norm_data)},
      {"buffer":0,"byteOffset":byte_offset['col'], "byteLength":len(col_data)},
      {"buffer":0,"byteOffset":byte_offset['idx'], "byteLength":len(idx_data)},
  ],
  "buffers": [{"byteLength": len(bin_data)}]
}

json_bytes = json.dumps(gltf, separators=(',',':')).encode()
# pad to 4 bytes
while len(json_bytes) % 4: json_bytes += b' '

def chunk(ctype, data):
    return struct.pack('<II', len(data), ctype) + data

json_chunk = chunk(0x4E4F534A, json_bytes)
bin_chunk  = chunk(0x004E4942, bin_data)

header = struct.pack('<III', 0x46546C67, 2, 12 + len(json_chunk) + len(bin_chunk))
glb = header + json_chunk + bin_chunk

out = os.path.join(os.path.dirname(__file__), 'assets/models/car.glb')
with open(out, 'wb') as f: f.write(glb)
print(f"Wrote {len(glb)} bytes → {out}  ({nv} verts, {ni//3} tris)")
