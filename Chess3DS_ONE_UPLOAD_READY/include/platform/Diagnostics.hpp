#pragma once

namespace chess3ds::platform {

// Leaves a tiny breadcrumb on the SD card. If a hardware-only crash occurs,
// the last completed startup/search stage can be inspected without a debugger.
void writeRuntimeStage(const char* stage) noexcept;
void clearRuntimeStage() noexcept;

} // namespace chess3ds::platform
