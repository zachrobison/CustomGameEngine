# VoxelEngine

A small C++/OpenGL game engine and a set of games built on it:

- **Iron Command** — first-person factory-RTS (build a factory, mass-produce
  robots, drop onto a planet, destroy the enemy base). *Factory Wizards.*
- **Robot Souls** — isometric souls-like with a delayed-bullet parry.
- **Horde Defense**, **Racing**, **Shooter**, **Sandbox**.

Pick a game from the in-engine main menu.

---

## Requirements (macOS)

Currently **macOS only** — it uses Apple's OpenGL framework directly. You don't
need to know any language to run it; the engine is **C++17**. Install the
toolchain once:

```bash
xcode-select --install          # C++ compiler + OpenGL
# install Homebrew from https://brew.sh, then:
brew install cmake glfw glm
```

> Intel Macs: in `CMakeLists.txt`, change `/opt/homebrew/opt/glfw/include`
> to `/usr/local/opt/glfw/include`.

Everything else (Dear ImGui, tinygltf, nlohmann/json, miniaudio, stb) is bundled
or fetched automatically by CMake on the first build (needs internet once).

## Build & run

```bash
cmake -S . -B build
cmake --build build -j4
cd build && ./VoxelEngine
```

## Get the games

The playable games live under `~/.voxelengine/`. This repo ships them in
`content/` — copy them into your home folder once:

```bash
cp -R content/voxelengine-saves ~/.voxelengine
```

Then launch and pick a game from the menu.

## Making assets (optional)

New 3D models/animations are authored in **Blender** (FBX → GLB) — only needed if
you want to add art, not to play.
