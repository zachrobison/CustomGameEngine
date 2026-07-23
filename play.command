#!/bin/bash
# VoxelEngine launcher — double-click this file in Finder to build & play.
# (First run builds the engine, which takes a minute; later runs are instant.)

cd "$(dirname "$0")" || exit 1
echo "=== VoxelEngine ==="

# 1) Make sure the build tools are installed
if ! command -v cmake >/dev/null 2>&1; then
  echo
  echo "Missing build tools. Please run these once in Terminal, then try again:"
  echo "    xcode-select --install"
  echo "    brew install cmake glfw glm     (install Homebrew first from https://brew.sh)"
  echo
  read -n 1 -s -r -p "Press any key to close..."
  exit 1
fi

# 2) Install the games into your home folder the first time
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

# 4) Play
echo "Launching..."
cd build && ./VoxelEngine
