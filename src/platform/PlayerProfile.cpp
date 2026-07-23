#include "PlayerProfile.h"
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <ctime>
#include <sys/stat.h>

#ifdef HAVE_STEAMWORKS
  #include "steam/steam_api.h"
#endif

PlayerProfile& PlayerProfile::get() {
    static PlayerProfile inst;
    return inst;
}

static std::string homeDir() {
    const char* h = getenv("HOME");
    return h ? h : ".";
}

static void mkdirp(const std::string& path) {
    mkdir(path.c_str(), 0755);
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
    if (m_id.empty()) {
        m_id   = generateLocalId();
        m_name = "Player";
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
