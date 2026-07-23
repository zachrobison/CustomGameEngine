#pragma once
#include <string>
#include <vector>

class FileDialog {
public:
    // Opens a native OS file picker. Returns the chosen path, or "" if cancelled.
    static std::string open(const std::string& title = "Open File",
                            const std::vector<std::string>& extensions = {"glb","obj"});
};
