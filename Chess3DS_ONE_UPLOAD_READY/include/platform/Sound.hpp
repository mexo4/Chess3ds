#pragma once

#include <cstdint>
#include <memory>

namespace chess3ds::platform {

enum class SoundEffect : std::uint8_t { Move = 0, Capture, Check, Castle, GameEnd, Error, Count };

class Sound {
public:
    Sound();
    ~Sound();

    Sound(const Sound&) = delete;
    Sound& operator=(const Sound&) = delete;

    bool initialize();
    void shutdown();
    void setEnabled(bool enabled);
    bool enabled() const;
    void play(SoundEffect effect);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace chess3ds::platform
