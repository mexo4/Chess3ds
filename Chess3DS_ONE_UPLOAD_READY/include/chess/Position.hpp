#pragma once

#include "chess/Types.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace chess3ds::chess {

class Position {
public:
    enum CastlingRight : std::uint8_t {
        WhiteKingSide  = 1u << 0,
        WhiteQueenSide = 1u << 1,
        BlackKingSide  = 1u << 2,
        BlackQueenSide = 1u << 3,
    };

    Position();

    static Position starting();
    static bool fromFen(const std::string& fen, Position& out, std::string* error = nullptr);

    std::string toFen() const;
    const Piece& pieceAt(int square) const { return board_[square]; }
    Color sideToMove() const { return sideToMove_; }
    std::uint8_t castlingRights() const { return castlingRights_; }
    int enPassantSquare() const { return enPassantSquare_; }
    int halfmoveClock() const { return halfmoveClock_; }
    int fullmoveNumber() const { return fullmoveNumber_; }

    std::vector<Move> legalMoves() const;
    std::vector<Move> legalMovesFrom(int square) const;
    bool findLegalMoveUci(const std::string& uci, Move& out) const;
    bool isLegal(const Move& move) const;
    bool makeMove(const Move& move);

    bool inCheck(Color color) const;
    bool isSquareAttacked(int square, Color byColor) const;
    int kingSquare(Color color) const;
    bool hasInsufficientMaterial() const;

    std::uint64_t repetitionKey() const;
    std::uint64_t perft(int depth) const;

private:
    std::array<Piece, 64> board_{};
    Color sideToMove_{Color::White};
    std::uint8_t castlingRights_{WhiteKingSide | WhiteQueenSide | BlackKingSide | BlackQueenSide};
    int enPassantSquare_{NoSquare};
    int halfmoveClock_{0};
    int fullmoveNumber_{1};

    std::vector<Move> pseudoLegalMoves() const;
    void applyUnchecked(const Move& move);
    void addPawnMoves(std::vector<Move>& moves, int from, Piece piece) const;
    void addKnightMoves(std::vector<Move>& moves, int from, Piece piece) const;
    void addSlidingMoves(std::vector<Move>& moves, int from, Piece piece) const;
    void addKingMoves(std::vector<Move>& moves, int from, Piece piece) const;
};

} // namespace chess3ds::chess
