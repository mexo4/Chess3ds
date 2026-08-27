#include "platform/Diagnostics.hpp"

#ifdef __3DS__
#include <cstdio>
#include <sys/stat.h>
#endif

namespace chess3ds::platform {
namespace {

constexpr const char* RuntimeStagePath = "sdmc:/3ds/Chess3DS/last_stage.txt";

} // namespace

void writeRuntimeStage(const char* stage) noexcept {
#ifdef __3DS__
    if (!stage) return;
    ::mkdir("sdmc:/3ds", 0777);
    ::mkdir("sdmc:/3ds/Chess3DS", 0777);
    std::FILE* file = std::fopen(RuntimeStagePath, "wb");
    if (!file) return;
    std::fputs(stage, file);
    std::fputc('\n', file);
    std::fclose(file);
#else
    (void)stage;
#endif
}

void clearRuntimeStage() noexcept {
#ifdef __3DS__
    std::remove(RuntimeStagePath);
#endif
}

} // namespace chess3ds::platform
