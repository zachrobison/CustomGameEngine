#!/bin/bash
# ============================================================
#   IRON COMMAND  —  double-click to build & play
#   (Factory-RTS: C&C x Satisfactory. LAN free-for-all.)
# ============================================================
# First run builds the engine (takes a minute); later runs are instant.

cd "$(dirname "$0")" || exit 1
echo "======================================"
echo "        IRON  COMMAND"
echo "======================================"

# 1) Build tools present?
if ! command -v cmake >/dev/null 2>&1; then
  echo
  echo "Missing build tools. Run these once in Terminal, then try again:"
  echo "    xcode-select --install"
  echo "    brew install cmake glfw glm      (get Homebrew at https://brew.sh)"
  echo
  read -n 1 -s -r -p "Press any key to close..."
  exit 1
fi

# 2) Install the bundled games the first time
if [ ! -d "$HOME/.voxelengine" ] && [ -d "content/voxelengine-saves" ]; then
  echo "Installing games to ~/.voxelengine ..."
  cp -R content/voxelengine-saves "$HOME/.voxelengine"
fi

# 3) Build if needed
if [ ! -x "build/VoxelEngine" ]; then
  echo "Building the engine (first time only)..."
  cmake -S . -B build >/dev/null && cmake --build build -j4 || {
    echo "Build failed — see messages above."
    read -n 1 -s -r -p "Press any key to close..."; exit 1; }
fi

# 4) Launch straight into Iron Command (skips the game menu)
echo "Launching Iron Command..."
cd build && ./VoxelEngine ironcommand
