#pragma once

#include "chess/Game.hpp"
#include "engine/StockfishEngine.hpp"
#include "platform/Sound.hpp"
#include "platform/Storage.hpp"
#include "ui/Renderer.hpp"

#include <3ds.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace chess3ds::ui {

class App {
public:
    int run();

private:
    enum class Screen : std::uint8_t {
        MainMenu,
        Setup,
        Game,
        Pause,
        Settings,
        Statistics,
        Puzzles,
        About,
    };

    enum class MainAction : std::uint8_t {
        Continue,
        Cpu,
        Local,
        Puzzles,
        Settings,
        Statistics,
        About,
        Exit,
    };

    struct Puzzle {
        const char* titlePl;
        const char* titleEn;
        const char* fen;
        std::vector<std::string> solution;
    };

    Renderer renderer_;
    platform::Storage storage_;
    platform::Settings settings_;
    platform::Statistics statistics_;
    platform::Sound sound_;
    engine::StockfishEngine engine_;
    chess::Game game_;

    Screen screen_{Screen::MainMenu};
    bool running_{true};
    // The engine is loaded on demand. This keeps the menu usable even if a
    // target-specific Stockfish initialization problem occurs.
    bool engineAvailable_{true};
    bool hasSavedGame_{false};
    int menuIndex_{0};
    int setupIndex_{0};
    int settingsIndex_{0};
    int puzzleIndex_{0};
    platform::GameMode setupMode_{platform::GameMode::Cpu};
    platform::TimeControl setupTimeControl_{platform::TimeControl::Rapid10};
    int setupSide_{0}; // 0 white, 1 black, 2 random

    platform::GameMode gameMode_{platform::GameMode::Cpu};
    platform::TimeControl timeControl_{platform::TimeControl::Unlimited};
    chess::Color playerColor_{chess::Color::White};
    int gameEngineLevel_{4};
    std::int64_t whiteTimeMs_{0};
    std::int64_t blackTimeMs_{0};
    std::int64_t incrementMs_{0};
    bool flipped_{false};
    int cursorSquare_{12};
    int selectedSquare_{chess::NoSquare};
    std::vector<chess::Move> selectedMoves_;
    std::optional<chess::Move> lastMove_;
    MoveAnimation animation_;
    float animationElapsedMs_{0.0f};
    bool pendingAutoFlip_{false};
    std::optional<engine::EngineResult> pendingEngineResult_;
    int lastEvaluationCp_{0};
    int lastEngineDepth_{0};

    std::vector<chess::Move> promotionChoices_;
    int promotionIndex_{0};
    bool resultFinalized_{false};
    std::string toast_;
    std::uint64_t toastUntil_{0};
    std::string lastPgnPath_;

    bool puzzleMode_{false};
    bool puzzleSolved_{false};
    int activePuzzle_{0};
    std::size_t puzzleStep_{0};

    std::uint64_t previousTickMs_{0};

    void update(float deltaMs, std::uint64_t nowMs);
    void handleInput(std::uint32_t down, std::uint32_t held, const touchPosition& touch,
                     std::uint64_t nowMs);
    void render(std::uint64_t nowMs);

    void handleMainMenu(std::uint32_t down, const touchPosition& touch);
    void handleSetup(std::uint32_t down, const touchPosition& touch);
    void handleSettings(std::uint32_t down, const touchPosition& touch);
    void handlePuzzles(std::uint32_t down, const touchPosition& touch);
    void handleGame(std::uint32_t down, std::uint32_t held, const touchPosition& touch);
    void handlePause(std::uint32_t down, const touchPosition& touch);

    void renderMainMenu();
    void renderSetup();
    void renderSettings();
    void renderStatistics();
    void renderPuzzles();
    void renderAbout();
    void renderGame();
    void renderPause();

    void startNewGame();
    void resumeSavedGame();
    void startPuzzle(int index);
    void returnToMainMenu();
    void selectSquare(int square);
    void attemptMoveTo(int square);
    void choosePromotion(int index);
    void playMove(const chess::Move& move, bool engineMove = false);
    void undoMove();
    void startEngineIfNeeded();
    bool ensureEngineAvailable();
    void applyEngineResult(const engine::EngineResult& result);
    void saveCurrentGame();
    void finalizeResult();
    void exportCurrentPgn();
    void showToast(const std::string& text, std::uint64_t durationMs = 1800);

    bool isHumanTurn() const;
    bool clockEnabled() const;
    std::int64_t& activeClock();
    std::string clockText(std::int64_t milliseconds) const;
    std::string tr(const char* polish, const char* english) const;
    std::string themeName(platform::Theme theme) const;
    std::string timeControlName(platform::TimeControl control) const;
    std::string sideName(int side) const;
    std::string resultText(chess::GameResult result) const;
    std::vector<MainAction> mainActions() const;
    static const std::vector<Puzzle>& puzzles();
};

} // namespace chess3ds::ui
