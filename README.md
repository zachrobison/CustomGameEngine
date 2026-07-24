# VoxelEngine

A small C++/OpenGL game engine and a set of games built on it:

- **Iron Command** — first-person factory-RTS (build a factory, mass-produce
  robots, drop onto a planet, destroy the enemy base). *Factory Wizards.*
- **Robot Souls** — isometric souls-like with a delayed-bullet parry.
- **Horde Defense**, **Racing**, **Shooter**, **Sandbox**.

Pick a game from the in-engine main menu.

## Easiest way to play (double-click)

No Terminal needed after the one-time toolchain install below:

- **`Iron Command.command`** (macOS) / **`Iron Command.bat`** (Windows) —
  double-click to build if needed and **launch straight into Iron Command**,
  skipping the menu.
- **`play.command`** — same, but opens the game menu so you can pick any game.

On the first run these install the bundled games and compile the engine (a
minute or two); after that they start instantly. macOS may ask you to allow the
script the first time (right-click → Open).

You can also boot any game by id from a terminal: `./VoxelEngine ironcommand`.

---

## Requirements

Builds on **macOS, Linux, and Windows** (C++17). You don't need to know a
language to run it — just install the toolchain + three libraries once.

**macOS**
```bash
xcode-select --install               # C++ compiler + OpenGL
# install Homebrew from https://brew.sh, then:
brew install cmake glfw glm
```
(GLEW is not needed on macOS — Apple ships a full GL header.)

**Linux (Debian/Ubuntu)**
```bash
sudo apt update
sudo apt install build-essential cmake libglfw3-dev libglm-dev libglew-dev
```

**Windows** — install **Visual Studio** (Community, free) with the
"Desktop development with C++" workload (gives the compiler + CMake), then use
**vcpkg** for the libraries:
```powershell
git clone https://github.com/microsoft/vcpkg; .\vcpkg\bootstrap-vcpkg.bat
.\vcpkg\vcpkg install glfw3 glm glew
```
Configure with the vcpkg toolchain (see the build section).

Everything else (Dear ImGui, tinygltf, nlohmann/json, miniaudio, stb) is bundled
or fetched automatically by CMake on the first build (needs internet once).

## Build & run

**macOS / Linux**
```bash
cmake -S . -B build
cmake --build build -j4
cd build && ./VoxelEngine
```

**Windows** (point CMake at vcpkg, then build/run)
```powershell
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=C:/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
build\Release\VoxelEngine.exe
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
