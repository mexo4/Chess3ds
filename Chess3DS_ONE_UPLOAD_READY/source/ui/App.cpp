#include "ui/App.hpp"
#include "platform/Diagnostics.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <sstream>

namespace chess3ds::ui {
namespace {

Rect gridButtonRect(int index) {
    const int column = index & 1;
    const int row = index / 2;
    return {10.0f + column * 155.0f, 12.0f + row * 55.0f, 145.0f, 45.0f};
}

Rect listButtonRect(int index, float height = 32.0f, float startY = 10.0f) {
    return {12.0f, startY + index * (height + 4.0f), 296.0f, height};
}

bool isDraw(chess::GameResult result) {
    return result == chess::GameResult::DrawStalemate
        || result == chess::GameResult::DrawRepetition
        || result == chess::GameResult::DrawFiftyMove
        || result == chess::GameResult::DrawInsufficientMaterial;
}

chess::Color winner(chess::GameResult result) {
    switch (result) {
        case chess::GameResult::WhiteCheckmate:
        case chess::GameResult::BlackResigned:
        case chess::GameResult::BlackTimeout: return chess::Color::White;
        default: return chess::Color::Black;
    }
}

std::string basenameOf(const std::string& path) {
    const std::size_t separator = path.find_last_of("/\\");
    return separator == std::string::npos ? path : path.substr(separator + 1);
}

} // namespace

int App::run() {
    if (!renderer_.initialize()) return 1;
    platform::writeRuntimeStage("renderer");
    storage_.initialize();
    storage_.loadSettings(settings_);
    storage_.loadStatistics(statistics_);
    sound_.setEnabled(settings_.sound);
    // Sound and Stockfish are deliberately initialized only when first used.
    // Both may create platform services/threads, so the main menu must render
    // successfully before either is touched.
    platform::writeRuntimeStage("menu");

    previousTickMs_ = osGetTime();
    while (running_ && aptMainLoop()) {
        const std::uint64_t now = osGetTime();
        const float delta = static_cast<float>(std::min<std::uint64_t>(100, now - previousTickMs_));
        previousTickMs_ = now;

        hidScanInput();
        touchPosition touch{};
        hidTouchRead(&touch);
        const std::uint32_t down = hidKeysDown();
        const std::uint32_t held = hidKeysHeld();

        handleInput(down, held, touch, now);
        update(delta, now);
        render(now);
    }

    if (screen_ == Screen::Game && game_.result() == chess::GameResult::Ongoing && !puzzleMode_)
        saveCurrentGame();
    storage_.saveSettings(settings_);
    storage_.saveStatistics(statistics_);
    engine_.shutdown();
    sound_.shutdown();
    renderer_.shutdown();
    return 0;
}

std::vector<App::MainAction> App::mainActions() const {
    std::vector<MainAction> actions;
    if (storage_.hasSavedGame()) actions.push_back(MainAction::Continue);
    actions.insert(actions.end(), {MainAction::Cpu, MainAction::Local, MainAction::Puzzles,
                                   MainAction::Settings, MainAction::Statistics,
                                   MainAction::About, MainAction::Exit});
    return actions;
}

void App::handleInput(std::uint32_t down, std::uint32_t held, const touchPosition& touch,
                      std::uint64_t) {
    switch (screen_) {
        case Screen::MainMenu: handleMainMenu(down, touch); break;
        case Screen::Setup: handleSetup(down, touch); break;
        case Screen::Game: handleGame(down, held, touch); break;
        case Screen::Pause: handlePause(down, touch); break;
        case Screen::Settings: handleSettings(down, touch); break;
        case Screen::Puzzles: handlePuzzles(down, touch); break;
        case Screen::Statistics:
        case Screen::About:
            if (down & (KEY_A | KEY_B | KEY_START)) returnToMainMenu();
            break;
    }
}

void App::handleMainMenu(std::uint32_t down, const touchPosition& touch) {
    const auto actions = mainActions();
    if (actions.empty()) return;
    menuIndex_ = std::clamp(menuIndex_, 0, static_cast<int>(actions.size()) - 1);

    if (down & KEY_LEFT) menuIndex_ = std::max(0, menuIndex_ - 1);
    if (down & KEY_RIGHT) menuIndex_ = std::min(static_cast<int>(actions.size()) - 1, menuIndex_ + 1);
    if (down & KEY_UP) menuIndex_ = std::max(0, menuIndex_ - 2);
    if (down & KEY_DOWN) menuIndex_ = std::min(static_cast<int>(actions.size()) - 1, menuIndex_ + 2);

    bool activate = (down & KEY_A) != 0;
    if (down & KEY_TOUCH) {
        for (int i = 0; i < static_cast<int>(actions.size()); ++i) {
            if (gridButtonRect(i).contains(touch.px, touch.py)) {
                menuIndex_ = i;
                activate = true;
                break;
            }
        }
    }
    if (down & KEY_START) running_ = false;
    if (!activate) return;

    switch (actions[menuIndex_]) {
        case MainAction::Continue: resumeSavedGame(); break;
        case MainAction::Cpu:
            if (!ensureEngineAvailable()) {
                showToast(tr("Stockfish nie uruchomił się", "Stockfish is unavailable"));
                break;
            }
            setupMode_ = platform::GameMode::Cpu;
            setupIndex_ = 0;
            screen_ = Screen::Setup;
            break;
        case MainAction::Local:
            setupMode_ = platform::GameMode::Local;
            setupIndex_ = 0;
            screen_ = Screen::Setup;
            break;
        case MainAction::Puzzles:
            puzzleIndex_ = 0;
            screen_ = Screen::Puzzles;
            break;
        case MainAction::Settings:
            settingsIndex_ = 0;
            screen_ = Screen::Settings;
            break;
        case MainAction::Statistics: screen_ = Screen::Statistics; break;
        case MainAction::About: screen_ = Screen::About; break;
        case MainAction::Exit: running_ = false; break;
    }
}

void App::handleSetup(std::uint32_t down, const touchPosition& touch) {
    const int count = setupMode_ == platform::GameMode::Cpu ? 5 : 3;
    if (down & KEY_UP) setupIndex_ = (setupIndex_ + count - 1) % count;
    if (down & KEY_DOWN) setupIndex_ = (setupIndex_ + 1) % count;
    bool activate = (down & KEY_A) != 0;
    int direction = (down & (KEY_LEFT | KEY_L)) ? -1
        : ((down & (KEY_RIGHT | KEY_R)) ? 1 : 0);
    if (down & KEY_TOUCH) {
        for (int i = 0; i < count; ++i) {
            if (listButtonRect(i, 36.0f, 18.0f).contains(touch.px, touch.py)) {
                setupIndex_ = i;
                activate = true;
                break;
            }
        }
    }
    if (down & KEY_B) {
        screen_ = Screen::MainMenu;
        menuIndex_ = 0;
        return;
    }

    if (setupMode_ == platform::GameMode::Cpu) {
        if (setupIndex_ == 0 && (direction || activate)) setupSide_ = (setupSide_ + (direction < 0 ? 2 : 1)) % 3;
        if (setupIndex_ == 1 && (direction || activate))
            setupTimeControl_ = static_cast<platform::TimeControl>((static_cast<int>(setupTimeControl_) + (direction < 0 ? 3 : 1)) % 4);
        if (setupIndex_ == 2 && (direction || activate))
            settings_.engineLevel = std::clamp(settings_.engineLevel + (direction < 0 ? -1 : 1), 1, 8);
        if (setupIndex_ == 3 && activate) startNewGame();
        if (setupIndex_ == 4 && activate) screen_ = Screen::MainMenu;
    } else {
        if (setupIndex_ == 0 && (direction || activate))
            setupTimeControl_ = static_cast<platform::TimeControl>((static_cast<int>(setupTimeControl_) + (direction < 0 ? 3 : 1)) % 4);
        if (setupIndex_ == 1 && activate) startNewGame();
        if (setupIndex_ == 2 && activate) screen_ = Screen::MainMenu;
    }
    storage_.saveSettings(settings_);
}

void App::handleSettings(std::uint32_t down, const touchPosition& touch) {
    constexpr int Count = 8;
    if (down & KEY_UP) settingsIndex_ = (settingsIndex_ + Count - 1) % Count;
    if (down & KEY_DOWN) settingsIndex_ = (settingsIndex_ + 1) % Count;
    int direction = (down & (KEY_LEFT | KEY_L)) ? -1
        : ((down & (KEY_RIGHT | KEY_R)) ? 1 : 0);
    bool activate = (down & KEY_A) != 0;
    if (down & KEY_TOUCH) {
        for (int i = 0; i < Count; ++i) {
            if (listButtonRect(i, 24.0f, 4.0f).contains(touch.px, touch.py)) {
                settingsIndex_ = i;
                activate = true;
                break;
            }
        }
    }
    if (down & KEY_B) {
        storage_.saveSettings(settings_);
        returnToMainMenu();
        return;
    }
    if (!direction && !activate) return;
    const int step = direction < 0 ? -1 : 1;
    switch (settingsIndex_) {
        case 0: settings_.engineLevel = std::clamp(settings_.engineLevel + step, 1, 8); break;
        case 1: {
            const int count = static_cast<int>(platform::Theme::Count);
            settings_.theme = static_cast<platform::Theme>((static_cast<int>(settings_.theme) + (step < 0 ? count - 1 : 1)) % count);
            break;
        }
        case 2: settings_.language = settings_.language == platform::Language::Polish
                    ? platform::Language::English : platform::Language::Polish; break;
        case 3: settings_.sound = !settings_.sound; sound_.setEnabled(settings_.sound); break;
        case 4: settings_.showLegalMoves = !settings_.showLegalMoves; break;
        case 5: settings_.animations = !settings_.animations; break;
        case 6: settings_.autoFlipLocal = !settings_.autoFlipLocal; break;
        case 7: storage_.saveSettings(settings_); returnToMainMenu(); return;
    }
    storage_.saveSettings(settings_);
}

void App::handlePuzzles(std::uint32_t down, const touchPosition& touch) {
    const int count = static_cast<int>(puzzles().size()) + 1;
    if (down & KEY_UP) puzzleIndex_ = (puzzleIndex_ + count - 1) % count;
    if (down & KEY_DOWN) puzzleIndex_ = (puzzleIndex_ + 1) % count;
    bool activate = (down & KEY_A) != 0;
    if (down & KEY_TOUCH) {
        for (int i = 0; i < count; ++i) {
            if (listButtonRect(i, 28.0f, 8.0f).contains(touch.px, touch.py)) {
                puzzleIndex_ = i;
                activate = true;
                break;
            }
        }
    }
    if (down & KEY_B) returnToMainMenu();
    else if (activate) {
        if (puzzleIndex_ == static_cast<int>(puzzles().size())) returnToMainMenu();
        else startPuzzle(puzzleIndex_);
    }
}

void App::handleGame(std::uint32_t down, std::uint32_t, const touchPosition& touch) {
    if (puzzleSolved_ || game_.result() != chess::GameResult::Ongoing) {
        if (down & KEY_A) {
            if (puzzleMode_) startPuzzle((activePuzzle_ + 1) % static_cast<int>(puzzles().size()));
            else startNewGame();
        }
        if (down & (KEY_B | KEY_START)) returnToMainMenu();
        return;
    }

    if (!promotionChoices_.empty()) {
        if (down & KEY_LEFT) promotionIndex_ = (promotionIndex_ + 3) % 4;
        if (down & KEY_RIGHT) promotionIndex_ = (promotionIndex_ + 1) % 4;
        if (down & KEY_A) choosePromotion(promotionIndex_);
        if (down & KEY_B) promotionChoices_.clear();
        if (down & KEY_TOUCH) {
            for (int i = 0; i < static_cast<int>(promotionChoices_.size()); ++i) {
                const Rect rect{55.0f + i * 53.0f, 102.0f, 46.0f, 50.0f};
                if (rect.contains(touch.px, touch.py)) choosePromotion(i);
            }
        }
        return;
    }

    if (down & KEY_START) {
        engine_.stop();
        screen_ = Screen::Pause;
        menuIndex_ = 0;
        selectedSquare_ = chess::NoSquare;
        selectedMoves_.clear();
        return;
    }
    if (down & KEY_X) flipped_ = !flipped_;
    if (down & KEY_Y) undoMove();
    if (down & KEY_B) {
        if (selectedSquare_ != chess::NoSquare) {
            selectedSquare_ = chess::NoSquare;
            selectedMoves_.clear();
        } else {
            engine_.stop();
            screen_ = Screen::Pause;
            menuIndex_ = 0;
        }
        return;
    }

    int file = chess::fileOf(cursorSquare_);
    int rank = chess::rankOf(cursorSquare_);
    if (down & KEY_LEFT) file += flipped_ ? 1 : -1;
    if (down & KEY_RIGHT) file += flipped_ ? -1 : 1;
    if (down & KEY_UP) rank += flipped_ ? -1 : 1;
    if (down & KEY_DOWN) rank += flipped_ ? 1 : -1;
    file = std::clamp(file, 0, 7);
    rank = std::clamp(rank, 0, 7);
    cursorSquare_ = rank * 8 + file;

    if ((down & KEY_A) && isHumanTurn())
        selectedSquare_ == chess::NoSquare ? selectSquare(cursorSquare_) : attemptMoveTo(cursorSquare_);
    if ((down & KEY_TOUCH) && isHumanTurn()) {
        const int square = Renderer::squareFromPoint(touch.px, touch.py, flipped_);
        if (square != chess::NoSquare) {
            cursorSquare_ = square;
            selectedSquare_ == chess::NoSquare ? selectSquare(square) : attemptMoveTo(square);
        }
    }
}

void App::handlePause(std::uint32_t down, const touchPosition& touch) {
    constexpr int Count = 4;
    if (down & KEY_UP) menuIndex_ = (menuIndex_ + Count - 1) % Count;
    if (down & KEY_DOWN) menuIndex_ = (menuIndex_ + 1) % Count;
    bool activate = (down & KEY_A) != 0;
    if (down & KEY_TOUCH) {
        for (int i = 0; i < Count; ++i) {
            if (listButtonRect(i, 38.0f, 24.0f).contains(touch.px, touch.py)) {
                menuIndex_ = i;
                activate = true;
                break;
            }
        }
    }
    if (down & (KEY_B | KEY_START)) {
        screen_ = Screen::Game;
        startEngineIfNeeded();
        return;
    }
    if (!activate) return;
    switch (menuIndex_) {
        case 0: screen_ = Screen::Game; startEngineIfNeeded(); break;
        case 1: saveCurrentGame(); returnToMainMenu(); break;
        case 2:
            game_.resign(gameMode_ == platform::GameMode::Cpu ? playerColor_ : game_.position().sideToMove());
            screen_ = Screen::Game;
            finalizeResult();
            break;
        case 3: exportCurrentPgn(); break;
    }
}

void App::startNewGame() {
    engine_.stop();
    if (setupMode_ == platform::GameMode::Cpu && !ensureEngineAvailable()) {
        screen_ = Screen::MainMenu;
        showToast(tr("Stockfish nie uruchomił się", "Stockfish is unavailable"));
        return;
    }
    game_.reset();
    gameMode_ = setupMode_;
    timeControl_ = setupTimeControl_;
    gameEngineLevel_ = settings_.engineLevel;
    if (gameMode_ == platform::GameMode::Cpu) {
        if (setupSide_ == 2) playerColor_ = (osGetTime() & 1u) ? chess::Color::White : chess::Color::Black;
        else playerColor_ = setupSide_ == 0 ? chess::Color::White : chess::Color::Black;
    } else playerColor_ = chess::Color::White;

    const platform::ClockPreset clock = platform::clockPreset(timeControl_);
    whiteTimeMs_ = blackTimeMs_ = clock.initialMs;
    incrementMs_ = clock.incrementMs;
    flipped_ = gameMode_ == platform::GameMode::Cpu && playerColor_ == chess::Color::Black;
    cursorSquare_ = flipped_ ? chess::parseSquare("e7") : chess::parseSquare("e2");
    selectedSquare_ = chess::NoSquare;
    selectedMoves_.clear();
    lastMove_.reset();
    animation_ = {};
    pendingAutoFlip_ = false;
    pendingEngineResult_.reset();
    resultFinalized_ = false;
    puzzleMode_ = false;
    puzzleSolved_ = false;
    lastEvaluationCp_ = 0;
    lastEngineDepth_ = 0;
    screen_ = Screen::Game;
    saveCurrentGame();
    startEngineIfNeeded();
}

void App::resumeSavedGame() {
    platform::SavedGame saved;
    std::string error;
    if (!storage_.loadGame(saved) || !game_.restore(saved.initialFen, saved.moves, &error)) {
        storage_.clearSavedGame();
        showToast(tr("Nie można odczytać zapisu", "Could not load save"));
        return;
    }
    if (saved.mode == platform::GameMode::Cpu && !ensureEngineAvailable()) {
        showToast(tr("Stockfish nie uruchomił się", "Stockfish is unavailable"));
        return;
    }
    gameMode_ = saved.mode;
    timeControl_ = saved.timeControl;
    playerColor_ = saved.playerColor;
    gameEngineLevel_ = saved.engineLevel;
    whiteTimeMs_ = saved.whiteTimeMs;
    blackTimeMs_ = saved.blackTimeMs;
    incrementMs_ = saved.incrementMs;
    flipped_ = gameMode_ == platform::GameMode::Cpu
        ? playerColor_ == chess::Color::Black
        : (settings_.autoFlipLocal && game_.position().sideToMove() == chess::Color::Black);
    cursorSquare_ = flipped_ ? chess::parseSquare("e7") : chess::parseSquare("e2");
    selectedSquare_ = chess::NoSquare;
    selectedMoves_.clear();
    lastMove_ = game_.history().empty() ? std::optional<chess::Move>{}
                                        : std::optional<chess::Move>{game_.history().back().move};
    animation_ = {};
    pendingAutoFlip_ = false;
    pendingEngineResult_.reset();
    resultFinalized_ = false;
    puzzleMode_ = false;
    puzzleSolved_ = false;
    screen_ = Screen::Game;
    startEngineIfNeeded();
}

void App::startPuzzle(int index) {
    engine_.stop();
    activePuzzle_ = std::clamp(index, 0, static_cast<int>(puzzles().size()) - 1);
    chess::Position position;
    std::string error;
    if (!chess::Position::fromFen(puzzles()[activePuzzle_].fen, position, &error)) {
        showToast("Puzzle error");
        return;
    }
    game_.reset(position, puzzles()[activePuzzle_].fen);
    puzzleMode_ = true;
    puzzleSolved_ = false;
    puzzleStep_ = 0;
    gameMode_ = platform::GameMode::Local;
    timeControl_ = platform::TimeControl::Unlimited;
    whiteTimeMs_ = blackTimeMs_ = incrementMs_ = 0;
    playerColor_ = position.sideToMove();
    flipped_ = playerColor_ == chess::Color::Black;
    cursorSquare_ = position.sideToMove() == chess::Color::White ? chess::parseSquare("e4") : chess::parseSquare("e5");
    selectedSquare_ = chess::NoSquare;
    selectedMoves_.clear();
    lastMove_.reset();
    animation_ = {};
    pendingAutoFlip_ = false;
    promotionChoices_.clear();
    resultFinalized_ = false;
    screen_ = Screen::Game;
}

void App::returnToMainMenu() {
    engine_.stop();
    screen_ = Screen::MainMenu;
    menuIndex_ = 0;
    selectedSquare_ = chess::NoSquare;
    selectedMoves_.clear();
    promotionChoices_.clear();
    pendingAutoFlip_ = false;
    pendingEngineResult_.reset();
}

void App::selectSquare(int square) {
    const chess::Piece piece = game_.position().pieceAt(square);
    if (!piece || piece.color != game_.position().sideToMove()) {
        sound_.play(platform::SoundEffect::Error);
        return;
    }
    selectedSquare_ = square;
    selectedMoves_ = game_.position().legalMovesFrom(square);
    if (selectedMoves_.empty()) sound_.play(platform::SoundEffect::Error);
}

void App::attemptMoveTo(int square) {
    if (square == selectedSquare_) {
        selectedSquare_ = chess::NoSquare;
        selectedMoves_.clear();
        return;
    }
    std::vector<chess::Move> choices;
    for (const chess::Move& move : selectedMoves_)
        if (move.to == square) choices.push_back(move);

    if (choices.empty()) {
        const chess::Piece piece = game_.position().pieceAt(square);
        if (piece && piece.color == game_.position().sideToMove()) selectSquare(square);
        else sound_.play(platform::SoundEffect::Error);
        return;
    }
    if (choices.size() > 1) {
        promotionChoices_ = choices;
        promotionIndex_ = 0;
        return;
    }

    if (puzzleMode_) {
        const auto& solution = puzzles()[activePuzzle_].solution;
        if (puzzleStep_ >= solution.size() || chess::moveToUci(choices.front()) != solution[puzzleStep_]) {
            sound_.play(platform::SoundEffect::Error);
            showToast(tr("Spróbuj innego ruchu", "Try another move"));
            selectedSquare_ = chess::NoSquare;
            selectedMoves_.clear();
            return;
        }
        ++puzzleStep_;
    }
    playMove(choices.front());
    if (puzzleMode_ && puzzleStep_ >= puzzles()[activePuzzle_].solution.size()) {
        puzzleSolved_ = true;
        sound_.play(platform::SoundEffect::GameEnd);
    }
}

void App::choosePromotion(int index) {
    if (index < 0 || index >= static_cast<int>(promotionChoices_.size())) return;
    const chess::Move move = promotionChoices_[index];
    promotionChoices_.clear();
    if (puzzleMode_) {
        const auto& solution = puzzles()[activePuzzle_].solution;
        if (puzzleStep_ >= solution.size() || chess::moveToUci(move) != solution[puzzleStep_]) {
            sound_.play(platform::SoundEffect::Error);
            showToast(tr("To nie jest rozwiązanie", "That is not the solution"));
            return;
        }
        ++puzzleStep_;
    }
    playMove(move);
    if (puzzleMode_ && puzzleStep_ >= puzzles()[activePuzzle_].solution.size()) {
        puzzleSolved_ = true;
        sound_.play(platform::SoundEffect::GameEnd);
    }
}

void App::playMove(const chess::Move& move, bool) {
    const chess::Position before = game_.position();
    const chess::Piece moving = before.pieceAt(move.from);
    const chess::Color movingSide = before.sideToMove();
    if (!game_.play(move)) {
        sound_.play(platform::SoundEffect::Error);
        return;
    }
    const chess::Move actualMove = game_.history().back().move;
    if (clockEnabled()) {
        if (movingSide == chess::Color::White) whiteTimeMs_ += incrementMs_;
        else blackTimeMs_ += incrementMs_;
    }
    lastMove_ = actualMove;
    selectedSquare_ = chess::NoSquare;
    selectedMoves_.clear();

    if (settings_.animations) {
        animation_.active = true;
        animation_.move = actualMove;
        animation_.piece = moving;
        animation_.progress = 0.0f;
        animationElapsedMs_ = 0.0f;
    }
    if (actualMove.isCastle()) sound_.play(platform::SoundEffect::Castle);
    else if (actualMove.isCapture()) sound_.play(platform::SoundEffect::Capture);
    else sound_.play(platform::SoundEffect::Move);
    if (game_.position().inCheck(game_.position().sideToMove()))
        sound_.play(platform::SoundEffect::Check);

    if (gameMode_ == platform::GameMode::Local && settings_.autoFlipLocal && !puzzleMode_) {
        if (settings_.animations) pendingAutoFlip_ = true;
        else {
            flipped_ = game_.position().sideToMove() == chess::Color::Black;
            pendingAutoFlip_ = false;
        }
    }
    if (!puzzleMode_) saveCurrentGame();
    if (game_.result() != chess::GameResult::Ongoing) finalizeResult();
    else startEngineIfNeeded();
}

void App::undoMove() {
    if (puzzleMode_ || game_.result() != chess::GameResult::Ongoing || game_.history().empty()) {
        sound_.play(platform::SoundEffect::Error);
        return;
    }
    const bool humanTurnBeforeUndo = isHumanTurn();
    engine_.stop();
    pendingEngineResult_.reset();
    const int plies = gameMode_ == platform::GameMode::Cpu && humanTurnBeforeUndo
        && game_.history().size() >= 2 ? 2 : 1;
    if (!game_.undo(plies)) return;
    lastMove_ = game_.history().empty() ? std::optional<chess::Move>{}
                                        : std::optional<chess::Move>{game_.history().back().move};
    animation_ = {};
    pendingAutoFlip_ = false;
    selectedSquare_ = chess::NoSquare;
    selectedMoves_.clear();
    if (gameMode_ == platform::GameMode::Cpu) flipped_ = playerColor_ == chess::Color::Black;
    sound_.play(platform::SoundEffect::Move);
    saveCurrentGame();
    startEngineIfNeeded();
}

bool App::isHumanTurn() const {
    if (puzzleMode_) return !puzzleSolved_ && (puzzleStep_ % 2 == 0);
    return gameMode_ == platform::GameMode::Local || game_.position().sideToMove() == playerColor_;
}

void App::startEngineIfNeeded() {
    if (!engineAvailable_ || puzzleMode_ || screen_ != Screen::Game
        || gameMode_ != platform::GameMode::Cpu || game_.isOver() || isHumanTurn()
        || engine_.isThinking() || pendingEngineResult_) return;

    int thinkTime = engine::StockfishEngine::defaultThinkTimeMs(gameEngineLevel_);
    if (clockEnabled()) {
        const std::int64_t remaining = game_.position().sideToMove() == chess::Color::White
            ? whiteTimeMs_ : blackTimeMs_;
        thinkTime = std::min<int>(thinkTime,
            static_cast<int>(std::clamp<std::int64_t>(remaining / 25, 40, 1500)));
    }
    platform::writeRuntimeStage("stockfish_search");
    if (!engine_.start(game_.position().toFen(), gameEngineLevel_, thinkTime)) {
        platform::writeRuntimeStage("running");
        showToast(tr("Błąd uruchamiania silnika", "Engine start error"));
    }
}

bool App::ensureEngineAvailable() {
    if (engine_.isInitialized()) return true;
    if (!engineAvailable_) return false;
    platform::writeRuntimeStage("stockfish_init");
    engineAvailable_ = engine_.initialize();
    platform::writeRuntimeStage(engineAvailable_ ? "running" : "stockfish_failed");
    return engineAvailable_;
}

void App::applyEngineResult(const engine::EngineResult& result) {
    if (game_.result() != chess::GameResult::Ongoing || isHumanTurn()) return;
    chess::Move move;
    if (!game_.position().findLegalMoveUci(result.bestMoveUci, move)) {
        showToast(tr("Stockfish zwrócił błędny ruch", "Stockfish returned an invalid move"));
        return;
    }
    lastEvaluationCp_ = result.evaluationCp;
    lastEngineDepth_ = result.depth;
    playMove(move, true);
}

void App::update(float deltaMs, std::uint64_t nowMs) {
    if (!toast_.empty() && nowMs >= toastUntil_) toast_.clear();
    if (screen_ != Screen::Game) return;

    if (game_.result() == chess::GameResult::Ongoing && clockEnabled()) {
        std::int64_t& clock = activeClock();
        clock = std::max<std::int64_t>(0, clock - static_cast<std::int64_t>(deltaMs));
        if (clock == 0) {
            engine_.stop();
            game_.timeout(game_.position().sideToMove());
            finalizeResult();
        }
    }

    if (animation_.active) {
        animationElapsedMs_ += deltaMs;
        animation_.progress = std::min(1.0f, animationElapsedMs_ / 165.0f);
        if (animation_.progress >= 1.0f) {
            animation_.active = false;
            if (pendingAutoFlip_) {
                flipped_ = game_.position().sideToMove() == chess::Color::Black;
                pendingAutoFlip_ = false;
            }
        }
    }

    engine::EngineResult result;
    if (engine_.poll(result)) {
        platform::writeRuntimeStage("running");
        pendingEngineResult_ = result;
    }
    if (pendingEngineResult_ && !animation_.active) {
        const engine::EngineResult completed = *pendingEngineResult_;
        pendingEngineResult_.reset();
        applyEngineResult(completed);
    }

    if (puzzleMode_ && !puzzleSolved_ && puzzleStep_ % 2 == 1 && !animation_.active) {
        const auto& solution = puzzles()[activePuzzle_].solution;
        if (puzzleStep_ < solution.size()) {
            chess::Move reply;
            if (game_.position().findLegalMoveUci(solution[puzzleStep_], reply)) {
                ++puzzleStep_;
                playMove(reply);
                if (puzzleStep_ >= solution.size()) puzzleSolved_ = true;
            }
        }
    }
    startEngineIfNeeded();
}

bool App::clockEnabled() const { return timeControl_ != platform::TimeControl::Unlimited; }

std::int64_t& App::activeClock() {
    return game_.position().sideToMove() == chess::Color::White ? whiteTimeMs_ : blackTimeMs_;
}

void App::saveCurrentGame() {
    if (puzzleMode_ || game_.result() != chess::GameResult::Ongoing) return;
    platform::SavedGame saved;
    saved.mode = gameMode_;
    saved.timeControl = timeControl_;
    saved.playerColor = playerColor_;
    saved.engineLevel = gameEngineLevel_;
    saved.whiteTimeMs = whiteTimeMs_;
    saved.blackTimeMs = blackTimeMs_;
    saved.incrementMs = incrementMs_;
    saved.initialFen = game_.initialFen();
    saved.moves = game_.uciHistory();
    storage_.saveGame(saved);
}

void App::finalizeResult() {
    if (resultFinalized_ || game_.result() == chess::GameResult::Ongoing) return;
    resultFinalized_ = true;
    engine_.stop();
    if (puzzleMode_) return;

    const chess::GameResult result = game_.result();
    if (gameMode_ == platform::GameMode::Cpu) {
        if (isDraw(result)) ++statistics_.cpuDraws;
        else if (winner(result) == playerColor_) {
            ++statistics_.cpuWins;
            statistics_.highestDefeatedLevel = std::max(statistics_.highestDefeatedLevel, gameEngineLevel_);
        } else ++statistics_.cpuLosses;
    } else ++statistics_.localGames;
    storage_.clearSavedGame();
    exportCurrentPgn();
    storage_.saveStatistics(statistics_);
    sound_.play(platform::SoundEffect::GameEnd);
}

void App::exportCurrentPgn() {
    std::string white = "White";
    std::string black = "Black";
    if (gameMode_ == platform::GameMode::Cpu) {
        const std::string stockfish = "Stockfish 11 (level " + std::to_string(gameEngineLevel_) + ")";
        if (playerColor_ == chess::Color::White) white = "Player", black = stockfish;
        else white = stockfish, black = "Player";
    }
    if (storage_.exportPgn(game_.toPgn(white, black), statistics_, &lastPgnPath_)) {
        storage_.saveStatistics(statistics_);
        showToast(tr("Zapisano ", "Saved ") + basenameOf(lastPgnPath_), 2600);
    } else showToast(tr("Nie udało się zapisać PGN", "Could not save PGN"));
}

void App::showToast(const std::string& value, std::uint64_t durationMs) {
    toast_ = value;
    toastUntil_ = osGetTime() + durationMs;
}

std::string App::clockText(std::int64_t milliseconds) const {
    if (!clockEnabled()) return "--:--";
    milliseconds = std::max<std::int64_t>(0, milliseconds);
    const int totalSeconds = static_cast<int>((milliseconds + 999) / 1000);
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "%02d:%02d", totalSeconds / 60, totalSeconds % 60);
    return buffer;
}

std::string App::tr(const char* polish, const char* english) const {
    return settings_.language == platform::Language::Polish ? polish : english;
}

std::string App::themeName(platform::Theme theme) const {
    switch (theme) {
        case platform::Theme::Classic: return tr("Klasyczny", "Classic");
        case platform::Theme::Midnight: return tr("Nocny", "Midnight");
        case platform::Theme::Forest: return tr("Leśny", "Forest");
        case platform::Theme::Rosewood: return tr("Palisander", "Rosewood");
        case platform::Theme::Count: break;
    }
    return "-";
}

std::string App::timeControlName(platform::TimeControl control) const {
    if (control == platform::TimeControl::Unlimited) return tr("Bez zegara", "No clock");
    return platform::clockPreset(control).label;
}

std::string App::sideName(int side) const {
    if (side == 0) return tr("Białe", "White");
    if (side == 1) return tr("Czarne", "Black");
    return tr("Losowo", "Random");
}

std::string App::resultText(chess::GameResult result) const {
    switch (result) {
        case chess::GameResult::Ongoing:
            return tr("Partia trwa", "Game in progress");
        case chess::GameResult::WhiteCheckmate:
            return tr("Białe wygrywają przez mata", "White wins by checkmate");
        case chess::GameResult::BlackCheckmate:
            return tr("Czarne wygrywają przez mata", "Black wins by checkmate");
        case chess::GameResult::DrawStalemate:
            return tr("Remis przez pata", "Draw by stalemate");
        case chess::GameResult::DrawRepetition:
            return tr("Remis przez powtórzenie", "Draw by repetition");
        case chess::GameResult::DrawFiftyMove:
            return tr("Remis przez regułę 50 ruchów", "Draw by 50-move rule");
        case chess::GameResult::DrawInsufficientMaterial:
            return tr("Remis: brak materiału", "Draw: insufficient material");
        case chess::GameResult::WhiteResigned:
            return tr("Czarne wygrywają — poddanie", "Black wins by resignation");
        case chess::GameResult::BlackResigned:
            return tr("Białe wygrywają — poddanie", "White wins by resignation");
        case chess::GameResult::WhiteTimeout:
            return tr("Czarne wygrywają na czas", "Black wins on time");
        case chess::GameResult::BlackTimeout:
            return tr("Białe wygrywają na czas", "White wins on time");
    }
    return tr("Koniec partii", "Game over");
}

void App::render(std::uint64_t) {
    renderer_.beginFrame();
    switch (screen_) {
        case Screen::MainMenu: renderMainMenu(); break;
        case Screen::Setup: renderSetup(); break;
        case Screen::Game: renderGame(); break;
        case Screen::Pause: renderPause(); break;
        case Screen::Settings: renderSettings(); break;
        case Screen::Statistics: renderStatistics(); break;
        case Screen::Puzzles: renderPuzzles(); break;
        case Screen::About: renderAbout(); break;
    }
    renderer_.endFrame();
}

void App::renderMainMenu() {
    const Palette colors = Renderer::palette(settings_.theme);
    renderer_.beginTop(colors.background);
    renderer_.logo(78, 112, 116, colors);
    renderer_.text("Chess3DS", 150, 62, 1.05f, colors.text);
    renderer_.text(tr("Szybkie szachy stworzone dla 3DS", "Fast chess made for the 3DS"),
                   152, 105, 0.5f, colors.muted);
    renderer_.text("Stockfish 11  •  Offline  •  60 FPS UI", 152, 137, 0.42f, colors.accent);
    renderer_.text(tr("A: wybierz     START: wyjdź", "A: select     START: exit"),
                   152, 195, 0.42f, colors.muted);

    renderer_.beginBottom(colors.background);
    const auto actions = mainActions();
    for (int i = 0; i < static_cast<int>(actions.size()); ++i) {
        std::string label;
        bool disabled = false;
        switch (actions[i]) {
            case MainAction::Continue: label = tr("Kontynuuj", "Continue"); break;
            case MainAction::Cpu: label = tr("Graj ze Stockfishem", "Play Stockfish"); disabled = !engineAvailable_; break;
            case MainAction::Local: label = tr("Dwóch graczy", "Two players"); break;
            case MainAction::Puzzles: label = tr("Zadania", "Puzzles"); break;
            case MainAction::Settings: label = tr("Ustawienia", "Settings"); break;
            case MainAction::Statistics: label = tr("Statystyki", "Statistics"); break;
            case MainAction::About: label = tr("O grze", "About"); break;
            case MainAction::Exit: label = tr("Wyjdź", "Exit"); break;
        }
        renderer_.button(gridButtonRect(i), label, i == menuIndex_, disabled, colors);
    }
}

void App::renderSetup() {
    const Palette colors = Renderer::palette(settings_.theme);
    renderer_.beginTop(colors.background);
    renderer_.text(setupMode_ == platform::GameMode::Cpu
        ? tr("Partia ze Stockfishem", "Game vs Stockfish")
        : tr("Partia lokalna", "Local game"), 200, 28, 0.78f, colors.text, C2D_AlignCenter);
    renderer_.logo(200, 125, 88, colors);
    renderer_.text(tr("Wszystkie zasady FIDE • zapis partii • PGN", "Full chess rules • autosave • PGN"),
                   200, 190, 0.44f, colors.muted, C2D_AlignCenter);

    renderer_.beginBottom(colors.background);
    std::vector<std::pair<std::string, std::string>> rows;
    if (setupMode_ == platform::GameMode::Cpu) {
        rows.push_back({tr("Kolor", "Side"), sideName(setupSide_)});
        rows.push_back({tr("Tempo", "Clock"), timeControlName(setupTimeControl_)});
        rows.push_back({tr("Poziom Stockfisha", "Stockfish level"), std::to_string(settings_.engineLevel) + "/8"});
    } else rows.push_back({tr("Tempo", "Clock"), timeControlName(setupTimeControl_)});
    rows.push_back({tr("Rozpocznij", "Start game"), "A"});
    rows.push_back({tr("Wróć", "Back"), "B"});
    for (int i = 0; i < static_cast<int>(rows.size()); ++i)
        renderer_.button(listButtonRect(i, 36.0f, 18.0f), rows[i].first, i == setupIndex_, false,
                         colors, rows[i].second);
}

void App::renderSettings() {
    const Palette colors = Renderer::palette(settings_.theme);
    renderer_.beginTop(colors.background);
    renderer_.text(tr("Ustawienia", "Settings"), 24, 24, 0.85f, colors.text);
    renderer_.piece({chess::PieceType::King, chess::Color::White}, 264, 53, 90);
    renderer_.piece({chess::PieceType::Queen, chess::Color::Black}, 306, 99, 70);
    renderer_.text(themeName(settings_.theme), 24, 84, 0.58f, colors.accent);
    renderer_.text(tr("Zmiany zapisują się automatycznie.", "Changes are saved automatically."),
                   24, 126, 0.44f, colors.muted);
    renderer_.text(tr("Sterowanie: krzyżak / dotyk / A", "Controls: D-pad / touch / A"),
                   24, 160, 0.44f, colors.muted);

    renderer_.beginBottom(colors.background);
    const auto onOff = [&](bool value) { return value ? tr("Tak", "On") : tr("Nie", "Off"); };
    const std::array<std::pair<std::string, std::string>, 8> rows{{
        {tr("Domyślny poziom", "Default level"), std::to_string(settings_.engineLevel) + "/8"},
        {tr("Motyw", "Theme"), themeName(settings_.theme)},
        {tr("Język", "Language"), settings_.language == platform::Language::Polish ? "Polski" : "English"},
        {tr("Dźwięki", "Sound"), onOff(settings_.sound)},
        {tr("Pokaż legalne ruchy", "Show legal moves"), onOff(settings_.showLegalMoves)},
        {tr("Animacje", "Animations"), onOff(settings_.animations)},
        {tr("Obracaj planszę lokalnie", "Auto-flip local board"), onOff(settings_.autoFlipLocal)},
        {tr("Wróć", "Back"), "B"},
    }};
    for (int i = 0; i < 8; ++i)
        renderer_.button(listButtonRect(i, 24.0f, 4.0f), rows[i].first, i == settingsIndex_, false,
                         colors, rows[i].second);
}

void App::renderStatistics() {
    const Palette colors = Renderer::palette(settings_.theme);
    renderer_.beginTop(colors.background);
    renderer_.text(tr("Statystyki", "Statistics"), 200, 24, 0.86f, colors.text, C2D_AlignCenter);
    const std::uint32_t total = statistics_.cpuWins + statistics_.cpuLosses + statistics_.cpuDraws;
    const float ratio = total ? static_cast<float>(statistics_.cpuWins) / total : 0.0f;
    renderer_.progressBar({55, 88, 290, 14}, ratio, colors.panel, colors.accent);
    renderer_.text(tr("Wygrane przeciwko Stockfishowi", "Wins against Stockfish"),
                   200, 116, 0.46f, colors.muted, C2D_AlignCenter);
    renderer_.text(std::to_string(static_cast<int>(ratio * 100.0f)) + "%", 200, 150,
                   0.86f, colors.accent, C2D_AlignCenter);
    renderer_.text(tr("B: menu", "B: menu"), 200, 210, 0.42f, colors.muted, C2D_AlignCenter);

    renderer_.beginBottom(colors.background);
    const std::array<std::pair<std::string, std::string>, 6> rows{{
        {tr("Wygrane", "Wins"), std::to_string(statistics_.cpuWins)},
        {tr("Przegrane", "Losses"), std::to_string(statistics_.cpuLosses)},
        {tr("Remisy", "Draws"), std::to_string(statistics_.cpuDraws)},
        {tr("Partie lokalne", "Local games"), std::to_string(statistics_.localGames)},
        {tr("Najwyższy pokonany poziom", "Highest defeated level"), std::to_string(statistics_.highestDefeatedLevel) + "/8"},
        {tr("Wyeksportowane PGN", "Exported PGNs"), std::to_string(statistics_.exportedGames)},
    }};
    for (int i = 0; i < 6; ++i)
        renderer_.button(listButtonRect(i, 30.0f, 12.0f), rows[i].first, false, false, colors, rows[i].second);
}

void App::renderPuzzles() {
    const Palette colors = Renderer::palette(settings_.theme);
    renderer_.beginTop(colors.background);
    renderer_.text(tr("Zadania matowe", "Checkmate puzzles"), 200, 26, 0.84f, colors.text, C2D_AlignCenter);
    renderer_.piece({chess::PieceType::Knight, chess::Color::White}, 148, 70, 104);
    renderer_.text(tr("Znajdź najlepszy ruch. Brak podpowiedzi silnika.",
                      "Find the best move. No engine hints."),
                   200, 190, 0.44f, colors.muted, C2D_AlignCenter);
    renderer_.beginBottom(colors.background);
    for (int i = 0; i < static_cast<int>(puzzles().size()); ++i) {
        const Puzzle& puzzle = puzzles()[i];
        renderer_.button(listButtonRect(i, 28.0f, 8.0f),
            settings_.language == platform::Language::Polish ? puzzle.titlePl : puzzle.titleEn,
            i == puzzleIndex_, false, colors, tr("Mat", "Mate"));
    }
    const int back = static_cast<int>(puzzles().size());
    renderer_.button(listButtonRect(back, 28.0f, 8.0f), tr("Wróć", "Back"),
                     back == puzzleIndex_, false, colors, "B");
}

void App::renderAbout() {
    const Palette colors = Renderer::palette(settings_.theme);
    renderer_.beginTop(colors.background);
    renderer_.logo(75, 80, 92, colors);
    renderer_.text("Chess3DS", 145, 44, 0.9f, colors.text);
    renderer_.text("Version 1.0.1", 147, 86, 0.44f, colors.accent);
    renderer_.text("mexo4", 147, 115, 0.46f, colors.muted);
    renderer_.text(tr("Lekki interfejs 2D przeznaczony również dla Old 3DS.",
                      "A lightweight 2D interface made for the Old 3DS too."),
                   25, 170, 0.42f, colors.text, 0, 350);
    renderer_.text(tr("B: menu", "B: menu"), 200, 218, 0.4f, colors.muted, C2D_AlignCenter);

    renderer_.beginBottom(colors.background);
    renderer_.panel({12, 12, 296, 216}, colors.panel, 7);
    renderer_.text(tr("Silnik: Stockfish 11 (GPLv3)\n"
                      "Grafika: własne figury wektorowe\n\n"
                      "Obsługa:\n"
                      "A / dotyk — wybór i ruch\n"
                      "B / START — pauza\n"
                      "Y — cofnij ruch\n"
                      "X — obróć planszę\n\n"
                      "Kod źródłowy: github.com/mexo4/Chess3DS",
                      "Engine: Stockfish 11 (GPLv3)\n"
                      "Art: original vector pieces\n\n"
                      "Controls:\n"
                      "A / touch — select and move\n"
                      "B / START — pause\n"
                      "Y — undo\n"
                      "X — flip board\n\n"
                      "Source: github.com/mexo4/Chess3DS"),
                   24, 22, 0.42f, colors.text, 0, 272);
}

void App::renderGame() {
    const Palette colors = Renderer::palette(settings_.theme);
    renderer_.beginTop(colors.background);
    const bool whiteActive = game_.position().sideToMove() == chess::Color::White
        && game_.result() == chess::GameResult::Ongoing;
    const bool blackActive = game_.position().sideToMove() == chess::Color::Black
        && game_.result() == chess::GameResult::Ongoing;
    renderer_.panel({18, 16, 170, 55}, whiteActive ? colors.panelAlt : colors.panel, 7);
    renderer_.panel({212, 16, 170, 55}, blackActive ? colors.panelAlt : colors.panel, 7);
    renderer_.text(tr("BIAŁE", "WHITE"), 31, 24, 0.4f, whiteActive ? colors.accent : colors.muted);
    renderer_.text(tr("CZARNE", "BLACK"), 225, 24, 0.4f, blackActive ? colors.accent : colors.muted);
    renderer_.text(clockText(whiteTimeMs_), 103, 36, 0.73f, colors.text, C2D_AlignCenter);
    renderer_.text(clockText(blackTimeMs_), 297, 36, 0.73f, colors.text, C2D_AlignCenter);

    renderer_.panel({18, 84, 364, 94}, colors.panel, 7);
    std::string status;
    if (puzzleMode_) status = settings_.language == platform::Language::Polish
        ? puzzles()[activePuzzle_].titlePl : puzzles()[activePuzzle_].titleEn;
    else if (game_.result() != chess::GameResult::Ongoing) status = resultText(game_.result());
    else if (engine_.isThinking()) status = tr("Stockfish myśli…", "Stockfish is thinking…");
    else status = game_.position().sideToMove() == chess::Color::White
        ? tr("Ruch białych", "White to move") : tr("Ruch czarnych", "Black to move");
    renderer_.text(status, 32, 94, 0.5f,
                   game_.result() == chess::GameResult::Ongoing ? colors.text : colors.accent);
    const std::string history = game_.moveListText(12);
    renderer_.text(history.empty() ? tr("Rozpocznij partię", "Make the first move") : history,
                   32, 126, 0.39f, colors.muted, 0, 330);
    if (gameMode_ == platform::GameMode::Cpu && !puzzleMode_) {
        std::ostringstream engineInfo;
        engineInfo << "L" << gameEngineLevel_ << "  d" << lastEngineDepth_ << "  ";
        if (lastEvaluationCp_ >= 0) engineInfo << '+';
        engineInfo << (lastEvaluationCp_ / 100.0f);
        renderer_.text(engineInfo.str(), 370, 187, 0.39f, colors.accent, C2D_AlignRight);
    }
    renderer_.text(tr("A/dotyk ruch   Y cofnij   X obróć   START pauza",
                      "A/touch move   Y undo   X flip   START pause"),
                   200, 213, 0.37f, colors.muted, C2D_AlignCenter);

    renderer_.beginBottom(colors.background);
    renderer_.board(game_.position(), colors, flipped_, cursorSquare_, selectedSquare_,
                    selectedMoves_, lastMove_ ? &*lastMove_ : nullptr, animation_,
                    settings_.showLegalMoves);

    if (!promotionChoices_.empty()) {
        renderer_.panel({42, 76, 236, 86}, colors.background, 9);
        renderer_.text(tr("Wybierz promocję", "Choose promotion"), 160, 80, 0.43f,
                       colors.text, C2D_AlignCenter);
        for (int i = 0; i < static_cast<int>(promotionChoices_.size()); ++i) {
            const Rect rect{55.0f + i * 53.0f, 102.0f, 46.0f, 50.0f};
            renderer_.panel(rect, i == promotionIndex_ ? colors.accent : colors.panel, 5);
            renderer_.piece({promotionChoices_[i].promotion, game_.position().sideToMove()},
                            rect.x + 8, rect.y + 5, 36);
        }
    }

    if (puzzleSolved_ || game_.result() != chess::GameResult::Ongoing) {
        renderer_.panel({40, 62, 240, 116}, colors.background, 10);
        renderer_.text(puzzleSolved_ ? tr("Rozwiązane!", "Solved!")
                                     : resultText(game_.result()),
                       160, 79, 0.61f, colors.accent, C2D_AlignCenter);
        if (!lastPgnPath_.empty() && !puzzleMode_)
            renderer_.text(basenameOf(lastPgnPath_), 160, 118, 0.38f, colors.muted, C2D_AlignCenter);
        renderer_.text(puzzleMode_ ? tr("A następne   B menu", "A next   B menu")
                                   : tr("A rewanż   B menu", "A rematch   B menu"),
                       160, 148, 0.43f, colors.text, C2D_AlignCenter);
    }
    if (!toast_.empty()) {
        renderer_.panel({35, 204, 250, 28}, colors.background, 6);
        renderer_.text(toast_, 160, 210, 0.39f, colors.text, C2D_AlignCenter);
    }
}

void App::renderPause() {
    const Palette colors = Renderer::palette(settings_.theme);
    renderer_.beginTop(colors.background);
    renderer_.logo(200, 92, 96, colors);
    renderer_.text(tr("Pauza", "Paused"), 200, 160, 0.84f, colors.text, C2D_AlignCenter);
    renderer_.text(tr("Zegar jest zatrzymany", "The clock is stopped"),
                   200, 202, 0.42f, colors.muted, C2D_AlignCenter);
    renderer_.beginBottom(colors.background);
    const std::array<std::string, 4> rows{{
        tr("Wznów", "Resume"), tr("Zapisz i wróć do menu", "Save and return to menu"),
        tr("Poddaj partię", "Resign"), tr("Eksportuj PGN", "Export PGN")
    }};
    for (int i = 0; i < 4; ++i)
        renderer_.button(listButtonRect(i, 38.0f, 24.0f), rows[i], i == menuIndex_, false, colors);
    if (!toast_.empty())
        renderer_.text(toast_, 160, 206, 0.39f, colors.accent, C2D_AlignCenter);
}

const std::vector<App::Puzzle>& App::puzzles() {
    static const std::vector<Puzzle> values{
        {"Mat hetmanem", "Queen mate", "7k/5Q2/6K1/8/8/8/8/8 w - - 0 1", {"f7g7"}},
        {"Mat na ostatniej linii", "Back-rank mate", "6k1/5ppp/8/8/8/8/5PPP/3R2K1 w - - 0 1", {"d1d8"}},
        {"Zamknięty król", "Boxed-in king", "7k/6pp/5Q2/8/8/8/8/6K1 w - - 0 1", {"f6f8"}},
        {"Mat skoczkiem", "Knight mate", "6rk/6pp/7N/8/8/8/8/6K1 w - - 0 1", {"h6f7"}},
        {"Promocja z matem", "Promotion mate", "7k/5Ppp/4K3/8/8/8/8/8 w - - 0 1", {"f7f8q"}},
    };
    return values;
}

} // namespace chess3ds::ui
