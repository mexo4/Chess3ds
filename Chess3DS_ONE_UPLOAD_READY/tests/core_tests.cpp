#include "chess/Game.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

using namespace chess3ds::chess;

namespace {

int failures = 0;

void check(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void checkCount(std::uint64_t actual, std::uint64_t expected, const std::string& message) {
    if (actual != expected) {
        std::cerr << "FAIL: " << message << " (expected " << expected << ", got " << actual << ")\n";
        ++failures;
    }
}

Position fen(const std::string& value) {
    Position result;
    std::string error;
    check(Position::fromFen(value, result, &error), "FEN parses: " + value + " (" + error + ")");
    return result;
}

void testStartingPosition() {
    const Position start = Position::starting();
    check(start.toFen() == "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
          "starting FEN round-trip");
    check(start.legalMoves().size() == 20, "starting position has 20 legal moves");
    check(start.perft(1) == 20, "start perft 1");
    check(start.perft(2) == 400, "start perft 2");
    check(start.perft(3) == 8902, "start perft 3");
    check(start.perft(4) == 197281, "start perft 4");
}

void testReferencePerft() {
    const Position kiwipete = fen("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");
    checkCount(kiwipete.perft(1), 48, "Kiwipete perft 1");
    checkCount(kiwipete.perft(2), 2039, "Kiwipete perft 2");
    checkCount(kiwipete.perft(3), 97862, "Kiwipete perft 3");

    const Position endgame = fen("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1");
    check(endgame.perft(1) == 14, "endgame perft 1");
    check(endgame.perft(2) == 191, "endgame perft 2");
    check(endgame.perft(3) == 2812, "endgame perft 3");
}

void testSpecialMoves() {
    Position castle = fen("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
    Move move;
    check(castle.findLegalMoveUci("e1g1", move) && move.isCastle(), "white king-side castling generated");
    check(castle.makeMove(move), "white castling applies");
    check(castle.pieceAt(parseSquare("g1")) == Piece{PieceType::King, Color::White}, "king moved during castle");
    check(castle.pieceAt(parseSquare("f1")) == Piece{PieceType::Rook, Color::White}, "rook moved during castle");

    Position ep = fen("4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 2");
    check(ep.findLegalMoveUci("e5d6", move) && move.isEnPassant(), "en-passant generated");
    check(ep.makeMove(move), "en-passant applies");
    check(!ep.pieceAt(parseSquare("d5")), "en-passant removes captured pawn");

    Position promotion = fen("4k3/P7/8/8/8/8/8/4K3 w - - 0 1");
    check(promotion.findLegalMoveUci("a7a8q", move) && move.isPromotion(), "promotion generated");
    check(promotion.makeMove(move), "promotion applies");
    check(promotion.pieceAt(parseSquare("a8")) == Piece{PieceType::Queen, Color::White}, "pawn becomes queen");
}

void testSanAndMate() {
    Game game;
    Move e4;
    check(game.position().findLegalMoveUci("e2e4", e4), "e4 legal");
    check(Game::sanForMove(game.position(), e4) == "e4", "SAN e4");
    check(game.play(e4), "play e4");
    check(game.playUci("e7e5"), "play e5");
    check(game.playUci("d1h5"), "play Qh5");
    check(game.playUci("b8c6"), "play Nc6");
    check(game.playUci("f1c4"), "play Bc4");
    check(game.playUci("g8f6"), "play Nf6 blunder");
    Move mate;
    check(game.position().findLegalMoveUci("h5f7", mate), "Qxf7 legal");
    check(Game::sanForMove(game.position(), mate) == "Qxf7#", "checkmate SAN suffix");
    check(game.play(mate), "play checkmate");
    check(game.result() == GameResult::WhiteCheckmate, "checkmate result");
    check(std::string(Game::pgnResult(game.result())) == "1-0", "PGN result for mate");
}

void testDrawsAndHistory() {
    Game repetition;
    for (int i = 0; i < 2; ++i) {
        check(repetition.playUci("g1f3"), "repetition Nf3");
        check(repetition.playUci("g8f6"), "repetition ...Nf6");
        check(repetition.playUci("f3g1"), "repetition Ng1");
        check(repetition.playUci("f6g8"), "repetition ...Ng8");
    }
    check(repetition.result() == GameResult::DrawRepetition, "threefold repetition draw");
    check(repetition.undo(2), "undo two plies");
    check(repetition.result() == GameResult::Ongoing, "undo reopens game");

    Game fifty;
    fifty.reset(fen("8/8/8/8/8/2k5/8/2K4R w - - 99 1"));
    check(fifty.playUci("h1h2"), "quiet move reaches 100 halfmoves");
    check(fifty.result() == GameResult::DrawFiftyMove, "50-move draw");

    Game material;
    material.reset(fen("8/8/8/8/8/2k5/8/2KB4 w - - 0 1"));
    check(material.result() == GameResult::DrawInsufficientMaterial, "king and bishop is insufficient");

    Game stalemate;
    stalemate.reset(fen("7k/5Q2/6K1/8/8/8/8/8 b - - 0 1"));
    check(stalemate.result() == GameResult::DrawStalemate, "stalemate detected");
}

void testRestoreAndPgn() {
    Game original;
    check(original.playUci("e2e4"), "saved e4");
    check(original.playUci("c7c5"), "saved c5");
    check(original.playUci("g1f3"), "saved Nf3");

    Game restored;
    std::string error;
    check(restored.restore(original.initialFen(), original.uciHistory(), &error), "restore game: " + error);
    check(restored.position().toFen() == original.position().toFen(), "restored position matches");
    check(restored.history().size() == 3, "restored history matches");
    const std::string pgn = restored.toPgn("Player", "Stockfish");
    check(pgn.find("1. e4 c5 2. Nf3") != std::string::npos, "PGN contains SAN moves");
}

void testBuiltInPuzzleSolutions() {
    const std::pair<const char*, const char*> puzzles[] = {
        {"7k/5Q2/6K1/8/8/8/8/8 w - - 0 1", "f7g7"},
        {"6k1/5ppp/8/8/8/8/5PPP/3R2K1 w - - 0 1", "d1d8"},
        {"7k/6pp/5Q2/8/8/8/8/6K1 w - - 0 1", "f6f8"},
        {"6rk/6pp/7N/8/8/8/8/6K1 w - - 0 1", "h6f7"},
        {"7k/5Ppp/4K3/8/8/8/8/8 w - - 0 1", "f7f8q"},
    };
    for (const auto& puzzle : puzzles) {
        Game game;
        game.reset(fen(puzzle.first), puzzle.first);
        check(game.playUci(puzzle.second), std::string("puzzle move legal: ") + puzzle.second);
        check(game.result() == GameResult::WhiteCheckmate,
              std::string("puzzle ends in mate: ") + puzzle.second);
    }
}

} // namespace

int main() {
    testStartingPosition();
    testReferencePerft();
    testSpecialMoves();
    testSanAndMate();
    testDrawsAndHistory();
    testRestoreAndPgn();
    testBuiltInPuzzleSolutions();

    if (failures) {
        std::cerr << failures << " test(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "All Chess3DS core tests passed.\n";
    return EXIT_SUCCESS;
}
