#include "ui/App.hpp"
#include "platform/Diagnostics.hpp"

#include <3ds.h>
#include <citro2d.h>
#include <citro3d.h>

// libctru defaults to a very small main-thread stack.  Chess3DS uses C++
// containers and a sizeable UI state, so reserve enough room for Old 3DS too.
extern "C" {
u32 __stacksize__ = 1024 * 1024;
}

int main() {
    chess3ds::platform::writeRuntimeStage("main");
    gfxInitDefault();
    chess3ds::platform::writeRuntimeStage("graphics");
    if (!C3D_Init(C3D_DEFAULT_CMDBUF_SIZE)) {
        chess3ds::platform::writeRuntimeStage("citro3d_failed");
        gfxExit();
        return 1;
    }
    if (!C2D_Init(C2D_DEFAULT_MAX_OBJECTS)) {
        chess3ds::platform::writeRuntimeStage("citro2d_failed");
        C3D_Fini();
        gfxExit();
        return 1;
    }
    C2D_Prepare();
    chess3ds::platform::writeRuntimeStage("app");

    chess3ds::ui::App app;
    const int result = app.run();

    C2D_Fini();
    C3D_Fini();
    gfxExit();
    chess3ds::platform::clearRuntimeStage();
    return result;
}
