#include "ui/App.hpp"
#include "platform/Diagnostics.hpp"

#include <3ds.h>
#include <citro2d.h>
#include <citro3d.h>
#include <new>

// Keep the main-thread stack conservative on Old 3DS. The large App object is
// allocated on the heap below, so the UI does not need a 1 MiB main stack.
extern "C" {
u32 __stacksize__ = 512 * 1024;
}

int main() {
    gfxInitDefault();
    chess3ds::platform::writeRuntimeStage("graphics");

    if (!C3D_Init(C3D_DEFAULT_CMDBUF_SIZE)) {
        chess3ds::platform::writeRuntimeStage("citro3d_failed");
        gfxExit();
        return 1;
    }

    // Chess3DS draws far fewer than 4096 objects per frame. A smaller Citro2D
    // pool reduces linear-memory pressure on Old 3DS without affecting the UI.
    if (!C2D_Init(1024)) {
        chess3ds::platform::writeRuntimeStage("citro2d_failed");
        C3D_Fini();
        gfxExit();
        return 1;
    }

    C2D_Prepare();
    chess3ds::platform::writeRuntimeStage("app_alloc");

    auto* app = new (std::nothrow) chess3ds::ui::App();
    if (!app) {
        chess3ds::platform::writeRuntimeStage("app_alloc_failed");
        C2D_Fini();
        C3D_Fini();
        gfxExit();
        return 1;
    }

    chess3ds::platform::writeRuntimeStage("app");
    const int result = app->run();
    delete app;

    C2D_Fini();
    C3D_Fini();
    gfxExit();
    chess3ds::platform::clearRuntimeStage();
    return result;
}
