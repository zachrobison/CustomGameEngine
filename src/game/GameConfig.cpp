#include "GameConfig.h"
#include "../vendor/json.hpp"
#include <fstream>
#include <dirent.h>
#include <algorithm>

using json = nlohmann::json;

GameConfig GameConfig::load(const std::string& dir) {
    GameConfig g;
    // id = last path segment
    auto slash = dir.find_last_of('/');
    g.id = slash == std::string::npos ? dir : dir.substr(slash + 1);

    std::ifstream f(dir + "/game.json");
    if (!f) return g;
    try {
        json j; f >> j;
        g.name        = j.value("name", g.id);
        g.description = j.value("description", std::string());
        g.bootLevel   = j.value("bootLevel", std::string("default"));
        if (j.contains("features")) {
            auto& ft = j["features"];
            g.vehicle        = ft.value("vehicle", true);
            g.rocket         = ft.value("rocket", true);
            g.grapple        = ft.value("grapple", true);
            g.dash           = ft.value("dash", true);
            g.jetpack        = ft.value("jetpack", true);
            g.dimensionShift = ft.value("dimensionShift", true);
            g.thirdPerson    = ft.value("thirdPerson", true);
            g.isoCamera      = ft.value("isoCamera", false);
            g.rts            = ft.value("rts", false);
            g.factory        = ft.value("factory", false);
        }
        if (j.contains("weapons"))
            g.weapons = j["weapons"].get<std::vector<std::string>>();
    } catch (...) { /* defaults stand */ }
    return g;
}

std::vector<GameConfig> GameConfig::scan(const std::string& gamesRoot) {
    std::vector<GameConfig> out;
    DIR* d = opendir(gamesRoot.c_str());
    if (!d) return out;
    while (dirent* e = readdir(d)) {
        std::string nm = e->d_name;
        if (nm == "." || nm == ".." || e->d_type != DT_DIR) continue;
        out.push_back(load(gamesRoot + "/" + nm));
    }
    closedir(d);
    std::sort(out.begin(), out.end(),
              [](const GameConfig& a, const GameConfig& b) { return a.name < b.name; });
    return out;
}
