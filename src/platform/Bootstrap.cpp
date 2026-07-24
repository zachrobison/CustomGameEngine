#include "Bootstrap.h"
#include <filesystem>
#include <cstdlib>
#include <cstdint>

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

static fs::path homePath() {
#if defined(_WIN32)
    if (const char* h = std::getenv("USERPROFILE")) return h;
    const char* d = std::getenv("HOMEDRIVE");
    const char* p = std::getenv("HOMEPATH");
    if (d && p) return fs::path(std::string(d) + p);
    return ".";
#else
    const char* h = std::getenv("HOME");
    return h ? fs::path(h) : fs::path(".");
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

    // 2) First run: install the bundled games into ~/.voxelengine. Skipped once
    //    the player already has a save folder (so their progress is preserved).
    fs::path bundled = base / "content" / "voxelengine-saves";
    fs::path target  = homePath() / ".voxelengine";
    if (fs::exists(bundled, ec) && !fs::exists(target / "saves", ec)) {
        fs::create_directories(target, ec);
        fs::copy(bundled, target,
                 fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
    }
}
