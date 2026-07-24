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

// Portable home directory: USERPROFILE on Windows, HOME elsewhere. (Windows is
// checked first there because HOME is sometimes set by Git/MSYS to a different
// place than where the rest of the app writes.)
static std::string homeDir() {
#if defined(_WIN32)
    if (const char* u = getenv("USERPROFILE")) return u;
    const char* d = getenv("HOMEDRIVE");
    const char* p = getenv("HOMEPATH");
    if (d && p) return std::string(d) + p;
    if (const char* h = getenv("HOME")) return h;
    return ".";
#else
    if (const char* h = getenv("HOME")) return h;
    return ".";
#endif
}

// When set (by Bootstrap, to the games folder shipped next to the exe), this is
// the data root instead of ~/.voxelengine.
static std::string g_dataRoot;
void PlayerProfile::setDataRoot(const std::string& root) { g_dataRoot = root; }
static std::string baseDir() {
    return g_dataRoot.empty() ? (homeDir() + "/.voxelengine") : g_dataRoot;
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
    std::string dir  = baseDir();
    std::string path = dir + "/profile.txt";
    mkdirp(dir);

    std::ifstream f(path);
    if (f) {
        std::getline(f, m_id);
        std::getline(f, m_name);
    }

    // Make the download reliably playable. The games ship under a specific
    // profile id (saves/<id>/games). If the profile we loaded doesn't actually
    // have games — a fresh machine (no profile.txt) OR a machine left with an
    // empty random profile by an earlier broken build — adopt whichever shipped
    // profile does have games. This self-heals both cases.
    std::error_code ec;
    std::filesystem::path savesRoot = std::filesystem::path(dir) / "saves";
    auto profileHasGames = [&](const std::string& id) {
        return !id.empty() &&
               std::filesystem::exists(savesRoot / id / "games", ec);
    };
    if (!profileHasGames(m_id)) {
        for (const auto& e : std::filesystem::directory_iterator(savesRoot, ec)) {
            if (e.is_directory() &&
                std::filesystem::exists(e.path() / "games", ec)) {
                m_id   = e.path().filename().string();   // adopt the games profile
                if (m_name.empty()) m_name = "Player";
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
        out << m_id << "\n" << (m_name.empty() ? "Player" : m_name) << "\n";
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
    std::string root = baseDir();
    std::string base = root + "/saves/" + m_id;
    mkdirp(root);
    mkdirp(root + "/saves");
    mkdirp(base);
    return base;
}
