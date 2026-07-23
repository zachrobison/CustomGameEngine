#include "FileDialog.h"
#import <AppKit/AppKit.h>

std::string FileDialog::open(const std::string& title,
                              const std::vector<std::string>& extensions) {
    @autoreleasepool {
        NSOpenPanel* panel = [NSOpenPanel openPanel];
        panel.title = [NSString stringWithUTF8String:title.c_str()];
        panel.allowsMultipleSelection = NO;
        panel.canChooseFiles          = YES;
        panel.canChooseDirectories    = NO;

        NSMutableArray<NSString*>* types = [NSMutableArray array];
        for (auto& e : extensions)
            [types addObject:[NSString stringWithUTF8String:e.c_str()]];
        [panel setAllowedFileTypes:types];

        if ([panel runModal] == NSModalResponseOK)
            return std::string(panel.URL.path.UTF8String);
        return "";
    }
}
