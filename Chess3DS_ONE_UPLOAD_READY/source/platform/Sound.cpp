#include "platform/Sound.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <vector>

#ifdef __3DS__
#include <3ds.h>
#endif

namespace chess3ds::platform {
namespace {

constexpr int SampleRate = 22050;
constexpr float Pi = 3.14159265358979323846f;

struct ToneSpec {
    float startHz;
    float endHz;
    int durationMs;
    float volume;
};

constexpr std::array<ToneSpec, static_cast<std::size_t>(SoundEffect::Count)> Specs{{
    {620.0f, 700.0f, 55, 0.32f},
    {270.0f, 150.0f, 95, 0.42f},
    {880.0f, 1120.0f, 130, 0.35f},
    {420.0f, 760.0f, 110, 0.35f},
    {520.0f, 1040.0f, 360, 0.38f},
    {180.0f, 120.0f, 120, 0.32f},
}};

std::vector<std::int16_t> makeTone(SoundEffect effect) {
    const ToneSpec spec = Specs[static_cast<std::size_t>(effect)];
    const int count = SampleRate * spec.durationMs / 1000;
    std::vector<std::int16_t> samples(static_cast<std::size_t>(count));
    float phase = 0.0f;
    for (int i = 0; i < count; ++i) {
        const float progress = static_cast<float>(i) / std::max(1, count - 1);
        float frequency = spec.startHz + (spec.endHz - spec.startHz) * progress;
        if (effect == SoundEffect::GameEnd) {
            const int section = std::min(2, i * 3 / count);
            static constexpr float chord[3] = {523.25f, 659.25f, 783.99f};
            frequency = chord[section];
        }
        phase += 2.0f * Pi * frequency / SampleRate;
        const float attack = std::min(1.0f, progress * 18.0f);
        const float decay = (1.0f - progress) * (1.0f - progress);
        const float value = std::sin(phase) * attack * decay * spec.volume;
        samples[static_cast<std::size_t>(i)] = static_cast<std::int16_t>(value * 32767.0f);
    }
    return samples;
}

} // namespace

struct Sound::Impl {
    bool initialized{false};
    bool attempted{false};
    bool enabled{true};
#ifdef __3DS__
    std::array<std::int16_t*, static_cast<std::size_t>(SoundEffect::Count)> buffers{};
    std::array<std::size_t, static_cast<std::size_t>(SoundEffect::Count)> sampleCounts{};
    std::array<ndspWaveBuf, static_cast<std::size_t>(SoundEffect::Count)> waveBuffers{};
#endif
};

Sound::Sound() : impl_(new Impl) {}
Sound::~Sound() { shutdown(); }

bool Sound::initialize() {
    if (impl_->initialized) return true;
    if (impl_->attempted) return false;
    impl_->attempted = true;
#ifdef __3DS__
    if (R_FAILED(ndspInit())) return false;
    impl_->initialized = true;
    ndspSetOutputMode(NDSP_OUTPUT_STEREO);
    std::memset(impl_->waveBuffers.data(), 0, sizeof(impl_->waveBuffers));
    for (std::size_t i = 0; i < static_cast<std::size_t>(SoundEffect::Count); ++i) {
        const auto samples = makeTone(static_cast<SoundEffect>(i));
        impl_->sampleCounts[i] = samples.size();
        impl_->buffers[i] = static_cast<std::int16_t*>(linearAlloc(samples.size() * sizeof(std::int16_t)));
        if (!impl_->buffers[i]) {
            shutdown();
            return false;
        }
        std::copy(samples.begin(), samples.end(), impl_->buffers[i]);
        DSP_FlushDataCache(impl_->buffers[i], samples.size() * sizeof(std::int16_t));

        ndspChnSetInterp(static_cast<int>(i), NDSP_INTERP_LINEAR);
        ndspChnSetRate(static_cast<int>(i), SampleRate);
        ndspChnSetFormat(static_cast<int>(i), NDSP_FORMAT_MONO_PCM16);
        float mix[12]{};
        mix[0] = 0.75f;
        mix[1] = 0.75f;
        ndspChnSetMix(static_cast<int>(i), mix);
    }
#endif
    impl_->initialized = true;
    return true;
}

void Sound::shutdown() {
    if (!impl_ || !impl_->initialized) return;
#ifdef __3DS__
    for (std::size_t i = 0; i < static_cast<std::size_t>(SoundEffect::Count); ++i)
        ndspChnReset(static_cast<int>(i));
    ndspExit();
    for (auto*& buffer : impl_->buffers) {
        if (buffer) linearFree(buffer);
        buffer = nullptr;
    }
#endif
    impl_->initialized = false;
}

void Sound::setEnabled(bool enabled) { impl_->enabled = enabled; }
bool Sound::enabled() const { return impl_->enabled; }

void Sound::play(SoundEffect effect) {
    if (!impl_->enabled) return;
    if (!impl_->initialized && !initialize()) return;
#ifdef __3DS__
    const std::size_t index = static_cast<std::size_t>(effect);
    if (index >= static_cast<std::size_t>(SoundEffect::Count) || !impl_->buffers[index]) return;
    ndspChnWaveBufClear(static_cast<int>(index));
    ndspWaveBuf& wave = impl_->waveBuffers[index];
    std::memset(&wave, 0, sizeof(wave));
    wave.data_vaddr = impl_->buffers[index];
    wave.nsamples = static_cast<std::uint32_t>(impl_->sampleCounts[index]);
    ndspChnWaveBufAdd(static_cast<int>(index), &wave);
#else
    (void)effect;
#endif
}

} // namespace chess3ds::platform
