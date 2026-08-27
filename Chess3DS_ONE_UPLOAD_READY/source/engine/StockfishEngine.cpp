#include "engine/StockfishEngine.hpp"

#include "bitboard.h"
#include "endgame.h"
#include "position.h"
#include "search.h"
#include "thread.h"
#include "tt.h"
#include "uci.h"

#include <algorithm>
#include <atomic>
#include <deque>
#include <new>
#include <utility>

namespace PSQT { void init(); }

namespace chess3ds::engine {

struct StockfishEngine::Impl {
    bool initialized{false};
    bool initializationAttempted{false};
    std::atomic_bool thinking{false};
    ::Position position;
    StateListPtr states;
    Color searchSide{WHITE};
};

StockfishEngine::StockfishEngine() = default;
StockfishEngine::~StockfishEngine() { shutdown(); }

bool StockfishEngine::initialize() {
    if (!impl_) {
        impl_.reset(new (std::nothrow) Impl);
        if (!impl_) return false;
    }
    if (impl_->initialized) return true;
    if (impl_->initializationAttempted) return false;
    impl_->initializationAttempted = true;

    UCI::init(Options);
    PSQT::init();
    Bitboards::init();
    ::Position::init();
    Bitbases::init();
    Endgames::init();
    Threads.set(1);
    if (Threads.empty() || !TT.is_ready()) {
        if (!Threads.empty()) Threads.set(0);
        return false;
    }
    Options["SyzygyProbeLimit"] = std::string("0");
    Search::clear();

    impl_->initialized = true;
    return true;
}

void StockfishEngine::shutdown() {
    if (!impl_ || !impl_->initialized) return;
    stop();
    Threads.set(0);
    impl_->initialized = false;
}

bool StockfishEngine::isInitialized() const {
    return impl_ && impl_->initialized;
}

int StockfishEngine::skillForLevel(int level) {
    static constexpr int skills[8] = {0, 3, 6, 9, 12, 15, 18, 20};
    return skills[std::clamp(level, 1, 8) - 1];
}

int StockfishEngine::defaultThinkTimeMs(int level) {
    static constexpr int times[8] = {80, 120, 180, 280, 450, 700, 1050, 1500};
    return times[std::clamp(level, 1, 8) - 1];
}

bool StockfishEngine::start(const std::string& fen, int level, int thinkTimeMs) {
    if (!impl_ || !impl_->initialized || impl_->thinking.load(std::memory_order_acquire)) return false;

    Options["Skill Level"] = std::to_string(skillForLevel(level));
    impl_->states.reset(new std::deque<StateInfo>(1));
    impl_->position.set(fen, false, &impl_->states->back(), Threads.main());
    impl_->searchSide = impl_->position.side_to_move();

    Search::LimitsType limits;
    limits.startTime = now();
    limits.movetime = std::max(20, thinkTimeMs);

    impl_->thinking.store(true, std::memory_order_release);
    Threads.start_thinking(impl_->position, impl_->states, limits, false);
    return true;
}

bool StockfishEngine::poll(EngineResult& result) {
    if (!impl_ || !impl_->initialized) return false;
    if (!impl_->thinking.load(std::memory_order_acquire) || Threads.empty()
        || Threads.main()->is_searching()) return false;

    EngineResult completed;
    const MainThread* main = Threads.main();
    completed.depth = static_cast<int>(main->completedDepth);
    completed.nodes = Threads.nodes_searched();
    if (!main->rootMoves.empty() && !main->rootMoves.front().pv.empty()) {
        completed.bestMoveUci = UCI::move(main->rootMoves.front().pv.front(), false);
        int score = static_cast<int>(main->rootMoves.front().score);
        if (impl_->searchSide == BLACK) score = -score;
        completed.evaluationCp = std::clamp(score, -32000, 32000);
    }
    impl_->thinking.store(false, std::memory_order_release);
    result = std::move(completed);
    return true;
}

bool StockfishEngine::isThinking() const {
    return impl_ && impl_->initialized && impl_->thinking.load(std::memory_order_acquire);
}

void StockfishEngine::stop() {
    if (!impl_ || !impl_->initialized) return;
    if (impl_->thinking.load(std::memory_order_acquire)) {
        Threads.stop.store(true, std::memory_order_release);
        Threads.main()->wait_for_search_finished();
    }
    impl_->thinking.store(false, std::memory_order_release);
}

} // namespace chess3ds::engine
