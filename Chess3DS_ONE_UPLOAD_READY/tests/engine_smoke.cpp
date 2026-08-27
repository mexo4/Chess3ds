#include "chess/Position.hpp"
#include "engine/StockfishEngine.hpp"

#include "movegen.h"
#include "position.h"
#include "thread.h"
#include "uci.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <random>
#include <string>
#include <thread>
#include <vector>

namespace {

bool compareLegalMoves(const chess3ds::chess::Position& position) {
    std::vector<std::string> chess3dsMoves;
    for (const chess3ds::chess::Move& move : position.legalMoves())
        chess3dsMoves.push_back(chess3ds::chess::moveToUci(move));

    StateInfo state;
    ::Position stockfishPosition;
    stockfishPosition.set(position.toFen(), false, &state, Threads.main());
    std::vector<std::string> stockfishMoves;
    for (const ExtMove& move : MoveList<LEGAL>(stockfishPosition))
        stockfishMoves.push_back(UCI::move(move, false));

    std::sort(chess3dsMoves.begin(), chess3dsMoves.end());
    std::sort(stockfishMoves.begin(), stockfishMoves.end());
    if (chess3dsMoves == stockfishMoves) return true;

    std::cerr << "Legal move mismatch at " << position.toFen() << "\nChess3DS:";
    for (const std::string& move : chess3dsMoves) std::cerr << ' ' << move;
    std::cerr << "\nStockfish:";
    for (const std::string& move : stockfishMoves) std::cerr << ' ' << move;
    std::cerr << '\n';
    return false;
}

bool runDifferentialMoveTest() {
    std::mt19937 random(0xC3E553u);
    std::size_t positionsChecked = 0;
    for (int game = 0; game < 40; ++game) {
        chess3ds::chess::Position position = chess3ds::chess::Position::starting();
        for (int ply = 0; ply < 90; ++ply) {
            if (!compareLegalMoves(position)) return false;
            ++positionsChecked;
            const std::vector<chess3ds::chess::Move> moves = position.legalMoves();
            if (moves.empty()) break;
            const chess3ds::chess::Move move = moves[random() % moves.size()];
            if (!position.makeMove(move)) {
                std::cerr << "Random legal move could not be applied.\n";
                return false;
            }
        }
    }
    std::cout << "Differential move test passed: " << positionsChecked
              << " positions matched Stockfish.\n";
    return true;
}

} // namespace

int main() {
    chess3ds::engine::StockfishEngine engine;
    if (!engine.initialize()) {
        std::cerr << "Stockfish initialization failed.\n";
        return EXIT_FAILURE;
    }

    if (!runDifferentialMoveTest()) return EXIT_FAILURE;

    const auto position = chess3ds::chess::Position::starting();
    if (!engine.start(position.toFen(), 4, 100)) {
        std::cerr << "Stockfish search did not start.\n";
        return EXIT_FAILURE;
    }

    chess3ds::engine::EngineResult result;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!engine.poll(result) && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(5));

    chess3ds::chess::Move legal;
    if (result.bestMoveUci.empty() || !position.findLegalMoveUci(result.bestMoveUci, legal)) {
        std::cerr << "Stockfish returned an invalid move: " << result.bestMoveUci << '\n';
        return EXIT_FAILURE;
    }
    if (result.nodes == 0 || result.depth == 0) {
        std::cerr << "Stockfish returned no search statistics.\n";
        return EXIT_FAILURE;
    }

    // The 3DS adapter polls the native Stockfish worker directly instead of
    // creating a second waiter thread. Verify that stop/restart stays sound.
    if (!engine.start(position.toFen(), 8, 5000)) {
        std::cerr << "Stockfish restart did not start.\n";
        return EXIT_FAILURE;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    engine.stop();
    if (engine.isThinking()) {
        std::cerr << "Stockfish did not stop cleanly.\n";
        return EXIT_FAILURE;
    }
    if (!engine.start(position.toFen(), 1, 40)) {
        std::cerr << "Stockfish search after stop did not start.\n";
        return EXIT_FAILURE;
    }
    result = {};
    const auto restartDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!engine.poll(result) && std::chrono::steady_clock::now() < restartDeadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    if (result.bestMoveUci.empty() || !position.findLegalMoveUci(result.bestMoveUci, legal)) {
        std::cerr << "Stockfish restart returned an invalid move: " << result.bestMoveUci << '\n';
        return EXIT_FAILURE;
    }

    std::cout << "Stockfish smoke test passed: " << result.bestMoveUci
              << ", depth " << result.depth << ", nodes " << result.nodes << "\n";
    return EXIT_SUCCESS;
}
