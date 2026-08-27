#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace chess3ds::engine {

struct EngineResult {
    std::string bestMoveUci;
    int evaluationCp{0};
    int depth{0};
    std::uint64_t nodes{0};
};

class StockfishEngine {
public:
    StockfishEngine();
    ~StockfishEngine();

    StockfishEngine(const StockfishEngine&) = delete;
    StockfishEngine& operator=(const StockfishEngine&) = delete;

    bool initialize();
    void shutdown();
    bool isInitialized() const;

    // Level is clamped to 1..8. Thinking is asynchronous; poll() returns the
    // completed result without ever blocking the render loop.
    bool start(const std::string& fen, int level, int thinkTimeMs);
    bool poll(EngineResult& result);
    bool isThinking() const;
    void stop();

    static int skillForLevel(int level);
    static int defaultThinkTimeMs(int level);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace chess3ds::engine
