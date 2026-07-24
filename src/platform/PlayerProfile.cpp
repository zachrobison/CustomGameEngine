#include "PlayerProfile.h"
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <ctime>
#include <filesystem>

#ifdef HAVE_STEAMWORKS
  #include "steam/steam_api.h"
#endif

PlayerProfile& PlayerProfile::get() {
    static PlayerProfile inst;
    return inst;
}

// Portable home directory: HOME on macOS/Linux, USERPROFILE on Windows.
static std::string homeDir() {
    if (const char* h = getenv("HOME")) return h;
#if defined(_WIN32)
    if (const char* u = getenv("USERPROFILE")) return u;
    const char* d = getenv("HOMEDRIVE");
    const char* p = getenv("HOMEPATH");
    if (d && p) return std::string(d) + p;
#endif
    return ".";
}

static void mkdirp(const std::string& path) {
    std::error_code ec;
    std::filesystem::create_directories(path, ec);   // portable, recursive
}

// Generates a simple random hex ID that stays consistent across sessions
static std::string generateLocalId() {
    srand((unsigned)time(nullptr));
    char buf[33] = {};
    for (int i = 0; i < 32; i++)
        buf[i] = "0123456789abcdef"[rand() % 16];
    return std::string(buf, 32);
}

void PlayerProfile::loadOrCreateLocal() {
    std::string dir  = homeDir() + "/.voxelengine";
    std::string path = dir + "/profile.txt";
    mkdirp(dir);

    std::ifstream f(path);
    if (f) {
        std::getline(f, m_id);
        std::getline(f, m_name);
    }
    // No saved profile yet? If the download shipped a games folder under some
    // profile id (saves/<id>/games), adopt that id so the bundled games are
    // found — otherwise a fresh random id would point at an empty folder.
    if (m_id.empty()) {
        std::error_code ec;
        std::filesystem::path savesRoot = std::filesystem::path(dir) / "saves";
        for (const auto& e : std::filesystem::directory_iterator(savesRoot, ec)) {
            if (e.is_directory() &&
                std::filesystem::exists(e.path() / "games", ec)) {
                m_id   = e.path().filename().string();
                m_name = "Player";
                break;
            }
        }
    }
    if (m_id.empty()) {
        m_id   = generateLocalId();
        m_name = "Player";
    }
    {   // persist whichever id we settled on
        std::ofstream out(path);
        out << m_id << "\n" << m_name << "\n";
    }
}

void PlayerProfile::init() {
#ifdef HAVE_STEAMWORKS
    if (SteamAPI_Init()) {
        m_hasSteam = true;
        ISteamUser* u = SteamUser();
        m_id   = std::to_string(u->GetSteamID().ConvertToUint64());
        m_name = SteamFriends()->GetPersonaName();
        return;
    }
#endif
    loadOrCreateLocal();
}

std::string PlayerProfile::saveDir() const {
    std::string base = homeDir() + "/.voxelengine/saves/" + m_id;
    mkdirp(homeDir() + "/.voxelengine");
    mkdirp(homeDir() + "/.voxelengine/saves");
    mkdirp(base);
    return base;
}
