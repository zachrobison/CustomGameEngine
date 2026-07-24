#pragma once

// Makes the game runnable by a plain double-click on any machine, with no
// toolchain or manual setup: it locates the folder the executable lives in,
// switches the working directory there so relative "assets/…" paths resolve,
// and installs the bundled games into the user's home folder on first run.
namespace Bootstrap {
    void run();   // call once, first thing in main()
}
