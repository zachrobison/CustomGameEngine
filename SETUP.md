# VoxelEngine — Setup

A small C++/OpenGL game engine (Iron Command, Robot Souls, Horde, Racing, etc.).

## Requirements

**Platform: macOS only** (right now). The engine uses Apple's OpenGL framework
(`#include <OpenGL/gl3.h>`, `-framework OpenGL`) and a Homebrew library path, so
it builds on a Mac as-is. Windows/Linux would need a small porting pass.

You do **not** need to learn or download a programming language to run it — the
engine is written in **C++17** (with a little C for the bundled libraries), and
all you install is the C++ build toolchain below. (Python + Blender are only used
for making new 3D assets, not for playing.)

### Install these once

1. **Xcode Command Line Tools** (the C++ compiler + macOS OpenGL):
   ```
   xcode-select --install
   ```
2. **Homebrew** (package manager): https://brew.sh
3. **CMake, GLFW, GLM** (build tool + the two external libraries):
   ```
   brew install cmake glfw glm
   ```

> Apple Silicon Macs: Homebrew lives at `/opt/homebrew` (already what CMake
> expects). Intel Macs: change `/opt/homebrew/opt/glfw/include` in `CMakeLists.txt`
> to `/usr/local/opt/glfw/include`.

Everything else (Dear ImGui, tinygltf, nlohmann/json, miniaudio, stb) is bundled —
ImGui is downloaded automatically by CMake on the first build, so you need an
internet connection the first time.

## Build & run

```
cd engine
cmake -S . -B build
cmake --build build -j4
cd build && ./VoxelEngine
```

Pick a game from the menu. (First launch creates a save profile under
`~/.voxelengine/`.)

## Important: the games live outside the repo

The playable games/levels are stored per-user under
`~/.voxelengine/saves/<profile>/{games,maps}`, **not** in this source tree.
If you were given a `voxelengine-saves` folder, copy it to your home directory so
you get the same games:

```
cp -R voxelengine-saves ~/.voxelengine
```

Without it, the engine still runs but starts with only default/empty content.
