#include "KeyBinds.h"
#include <GLFW/glfw3.h>
#include "../vendor/json.hpp"
#include <fstream>

using json = nlohmann::json;

// Letters, digits, and a few specials — enough for action binds.
static const struct { const char* name; int code; } KEY_TABLE[] = {
    {"A",GLFW_KEY_A},{"B",GLFW_KEY_B},{"C",GLFW_KEY_C},{"D",GLFW_KEY_D},
    {"E",GLFW_KEY_E},{"F",GLFW_KEY_F},{"G",GLFW_KEY_G},{"H",GLFW_KEY_H},
    {"I",GLFW_KEY_I},{"J",GLFW_KEY_J},{"K",GLFW_KEY_K},{"L",GLFW_KEY_L},
    {"M",GLFW_KEY_M},{"N",GLFW_KEY_N},{"O",GLFW_KEY_O},{"P",GLFW_KEY_P},
    {"Q",GLFW_KEY_Q},{"R",GLFW_KEY_R},{"S",GLFW_KEY_S},{"T",GLFW_KEY_T},
    {"U",GLFW_KEY_U},{"V",GLFW_KEY_V},{"W",GLFW_KEY_W},{"X",GLFW_KEY_X},
    {"Y",GLFW_KEY_Y},{"Z",GLFW_KEY_Z},
    {"0",GLFW_KEY_0},{"1",GLFW_KEY_1},{"2",GLFW_KEY_2},{"3",GLFW_KEY_3},
    {"4",GLFW_KEY_4},{"5",GLFW_KEY_5},{"6",GLFW_KEY_6},{"7",GLFW_KEY_7},
    {"8",GLFW_KEY_8},{"9",GLFW_KEY_9},
    {"SPACE",GLFW_KEY_SPACE},{"TAB",GLFW_KEY_TAB},
    {"LSHIFT",GLFW_KEY_LEFT_SHIFT},{"LCTRL",GLFW_KEY_LEFT_CONTROL},
    {"LALT",GLFW_KEY_LEFT_ALT},{"CAPS",GLFW_KEY_CAPS_LOCK},
    {"F1",GLFW_KEY_F1},{"F2",GLFW_KEY_F2},{"F3",GLFW_KEY_F3},{"F4",GLFW_KEY_F4},
};

const char* KeyBinds::keyName(int glfwKey) {
    for (auto& k : KEY_TABLE) if (k.code == glfwKey) return k.name;
    return "?";
}

int KeyBinds::keyCode(const std::string& name) {
    std::string up = name;
    for (auto& c : up) c = (char)toupper((unsigned char)c);
    for (auto& k : KEY_TABLE) if (up == k.name) return k.code;
    return -1;
}

static const struct { const char* action; int code; } DEFAULTS[] = {
    {"dash",            GLFW_KEY_Q},
    {"grapple",         GLFW_KEY_C},
    {"rocket",          GLFW_KEY_X},
    {"interact",        GLFW_KEY_E},
    {"third_person",    GLFW_KEY_V},
    {"dimension_shift", GLFW_KEY_T},
    {"fly_toggle",      GLFW_KEY_G},
    {"reload",          GLFW_KEY_R},
};

int KeyBinds::key(const std::string& action) const {
    auto it = binds.find(action);
    if (it != binds.end()) return it->second;
    for (auto& d : DEFAULTS) if (action == d.action) return d.code;
    return GLFW_KEY_UNKNOWN;
}

void KeyBinds::set(const std::string& action, int glfwKey) {
    binds[action] = glfwKey;
}

void KeyBinds::loadOrDefaults(const std::string& path) {
    for (auto& d : DEFAULTS) binds[d.action] = d.code;

    std::ifstream f(path);
    if (f) {
        try {
            json j; f >> j;
            for (auto& [action, keyStr] : j.items()) {
                int code = keyCode(keyStr.get<std::string>());
                if (code >= 0) binds[action] = code;
            }
            return;   // loaded fine — don't rewrite the file
        } catch (...) { /* fall through: rewrite defaults */ }
    }
    save(path);
}

bool KeyBinds::save(const std::string& path) const {
    json j;
    for (auto& [action, code] : binds) j[action] = keyName(code);
    std::ofstream f(path);
    if (!f) return false;
    f << j.dump(2);
    return true;
}
