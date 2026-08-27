#pragma once

#include <cstdint>
#include <string>

namespace chess3ds::chess {

enum class Color : std::uint8_t { White = 0, Black = 1 };

constexpr Color opposite(Color color) {
    return color == Color::White ? Color::Black : Color::White;
}

enum class PieceType : std::uint8_t {
    None = 0,
    Pawn,
    Knight,
    Bishop,
    Rook,
    Queen,
    King,
};

struct Piece {
    PieceType type{PieceType::None};
    Color color{Color::White};

    constexpr explicit operator bool() const { return type != PieceType::None; }
    constexpr bool operator==(const Piece& other) const {
        return type == other.type && (!*this || color == other.color);
    }
    constexpr bool operator!=(const Piece& other) const { return !(*this == other); }
};

enum MoveFlag : std::uint8_t {
    MoveQuiet       = 0,
    MoveCapture     = 1u << 0,
    MoveDoublePawn  = 1u << 1,
    MoveEnPassant   = 1u << 2,
    MoveCastle      = 1u << 3,
    MovePromotion   = 1u << 4,
};

struct Move {
    std::uint8_t from{0};
    std::uint8_t to{0};
    PieceType promotion{PieceType::None};
    std::uint8_t flags{MoveQuiet};

    constexpr bool isCapture() const { return (flags & MoveCapture) != 0; }
    constexpr bool isCastle() const { return (flags & MoveCastle) != 0; }
    constexpr bool isEnPassant() const { return (flags & MoveEnPassant) != 0; }
    constexpr bool isPromotion() const { return promotion != PieceType::None; }

    constexpr bool sameCoordinates(const Move& other) const {
        return from == other.from && to == other.to && promotion == other.promotion;
    }
    constexpr bool operator==(const Move& other) const {
        return sameCoordinates(other) && flags == other.flags;
    }
    constexpr bool operator!=(const Move& other) const { return !(*this == other); }
};

constexpr int NoSquare = -1;

inline int fileOf(int square) { return square & 7; }
inline int rankOf(int square) { return square >> 3; }
inline bool validSquare(int square) { return square >= 0 && square < 64; }

std::string squareName(int square);
int parseSquare(const std::string& text);
std::string moveToUci(const Move& move);

} // namespace chess3ds::chess
