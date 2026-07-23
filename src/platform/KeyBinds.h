#pragma once
#include <string>
#include <map>

// Rebindable action → key mapping, persisted as JSON in the profile save
// dir so each project can map the same physical keys to different actions.
// Actions: dash, grapple, rocket, interact, third_person, dimension_shift,
// fly_toggle, reload.
class KeyBinds {
public:
    // Loads path if it exists, else installs defaults and writes the file.
    void loadOrDefaults(const std::string& path);
    bool save(const std::string& path) const;

    // GLFW key code for an action (falls back to the default binding).
    int key(const std::string& action) const;
    void set(const std::string& action, int glfwKey);

    std::map<std::string, int>&       all()       { return binds; }
    const std::map<std::string, int>& all() const { return binds; }

    // "Q" ↔ GLFW_KEY_Q etc. keyCode returns -1 for unknown names.
    static const char* keyName(int glfwKey);
    static int         keyCode(const std::string& name);

private:
    std::map<std::string, int> binds;
};
