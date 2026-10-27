# asset_gen — procedural game models

Low-poly models for every game, built from code inside Blender and exported
to `assets/models/<game>/*.glb`. Source scene: `assets/source/game_assets.blend`.

| module           | game                     | output                      |
|------------------|--------------------------|-----------------------------|
| `ironcommand.py` | Iron Command (atomic-age)| `assets/models/ironcommand` |
| `robotsouls.py`  | Robot Souls-Like         | `assets/models/robotsouls`  |
| `horde.py`       | Horde Defense            | `assets/models/horde`       |
| `racing.py`      | Raceway (stunt)          | `assets/models/racing`      |
| `shooter.py`     | Interdimensional Shooter | `assets/models/shooter`     |
| `sandbox.py`     | Sandbox (shared props)   | `assets/models/sandbox`     |

## Conventions (what `GltfModel` expects)
- 1 unit = 1 m, Y-up, **forward = +Z**, origin at the footprint centre on the ground.
  Floating/held things (drone, rocket, rift rock, hanging light, held tools,
  jetpack) are centred on their mount/grip point instead.
- Flat base colours only — one glTF primitive per material, no textures.
- Static meshes (no rigs): render with `GltfModel`, scale 1.
  Racing cars are real-size, so use scale 1, not the old `carMeshScale = 3`.

## Rebuild (in Blender's Python console, or via BlenderMCP)
```python
import sys; sys.path.insert(0, "/path/to/engine/tools/asset_gen")
import agen
agen.run("ironcommand", "IronCommand")                 # build + lay out
agen.export_collection("IronCommand", "/path/to/engine/assets/models/ironcommand")
```
