# VoxelEngine

A small C++/OpenGL game engine and a set of games built on it:

- **Iron Command** — first-person factory-RTS (build a factory, mass-produce
  robots, drop onto a planet, destroy the enemy base). *Factory Wizards.*
- **Robot Souls** — isometric souls-like with a delayed-bullet parry.
- **Horde Defense**, **Racing**, **Shooter**, **Sandbox**.

Pick a game from the in-engine main menu.

## ▶ Play now — no setup (recommended)

Grab a ready-to-run build from the **[Releases page](../../releases/latest)** —
nothing to install, no compiler, no libraries:

- **Windows:** download `IronCommand-Windows.zip`, unzip, double-click
  `VoxelEngine.exe`. (First launch, Windows SmartScreen may warn — click
  *More info → Run anyway*.)
- **macOS:** download `IronCommand-macOS.zip`, unzip, then **right-click
  `Iron Command.app` → Open** the first time (it's an unsigned free app).

That's it — it drops straight into Iron Command and installs the games on first
run. LAN free-for-all: one person Hosts, everyone else Joins by the host's LAN IP.

*(Releases are built automatically by GitHub Actions on macOS + Windows — see
`.github/workflows/release.yml`.)*

---

## Build it yourself (developers only)

Only needed if you want to change the code. The double-click launchers
`Iron Command.command` / `.bat` (or `play.command` for the menu) build and run
from source; you can also `./VoxelEngine ironcommand` to boot a game by id.

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
