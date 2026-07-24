#include "FileDialog.h"
// Non-Apple fallback. macOS uses the native panel in FileDialog.mm; on
// Windows/Linux there's no native picker wired up yet (an editor-only feature),
// so this returns "" — the game and Iron Command don't depend on it.
#if !defined(__APPLE__)
std::string FileDialog::open(const std::string&, const std::vector<std::string>&) {
    return "";
}
#endif
