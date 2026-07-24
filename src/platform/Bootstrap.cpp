#include "Bootstrap.h"
#include "PlayerProfile.h"
#include <filesystem>
#include <cstdlib>
#include <cstdint>
#include <cstdio>

#if defined(_WIN32)
  #include <windows.h>
#elif defined(__APPLE__)
  #include <mach-o/dyld.h>
#else
  #include <unistd.h>
#endif

namespace fs = std::filesystem;

// Absolute path of the running executable (not argv[0], which is unreliable).
static fs::path executablePath() {
#if defined(_WIN32)
    char buf[MAX_PATH];
    DWORD n = GetModuleFileNameA(nullptr, buf, MAX_PATH);
    return n ? fs::path(std::string(buf, n)) : fs::path();
#elif defined(__APPLE__)
    char buf[4096]; uint32_t sz = sizeof(buf);
    if (_NSGetExecutablePath(buf, &sz) != 0) return {};
    std::error_code ec; fs::path p = fs::canonical(fs::path(buf), ec);
    return ec ? fs::path(buf) : p;
#else
    char buf[4096];
    ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf));
    return n > 0 ? fs::path(std::string(buf, (size_t)n)) : fs::path();
#endif
}


void Bootstrap::run() {
    std::error_code ec;

    // 1) Work from the executable's own directory so "assets/…" / "shaders/…"
    //    resolve no matter where the game was launched from (Finder, Explorer,
    //    a shortcut, another folder…).
    fs::path exe = executablePath();
    fs::path base = exe.empty() ? fs::current_path(ec) : exe.parent_path();
    if (fs::exists(base / "assets", ec))
        fs::current_path(base, ec);

    // 2) Point the game's data root straight at the bundled games folder that
    //    shipped next to the executable. This is the robust part: no copying to
    //    the home folder (which can silently fail on Windows) and no dependence
    //    on HOME/USERPROFILE resolution — the games are read from exactly where
    //    the download put them.
    fs::path bundled = base / "content" / "voxelengine-saves";
    if (fs::exists(bundled / "saves", ec)) {
        PlayerProfile::setDataRoot(bundled.string());
        std::fprintf(stderr, "[boot] data root = %s\n", bundled.string().c_str());
    } else {
        std::fprintf(stderr, "[boot] no bundled games at %s — using home folder\n",
                     bundled.string().c_str());
    }
}
