#include "chess/Position.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <sstream>

namespace chess3ds::chess {
namespace {

constexpr Piece Empty{};

char pieceToFen(Piece piece) {
    char value = ' ';
    switch (piece.type) {
        case PieceType::Pawn:   value = 'p'; break;
        case PieceType::Knight: value = 'n'; break;
        case PieceType::Bishop: value = 'b'; break;
        case PieceType::Rook:   value = 'r'; break;
        case PieceType::Queen:  value = 'q'; break;
        case PieceType::King:   value = 'k'; break;
        case PieceType::None:   return ' ';
    }
    return piece.color == Color::White ? static_cast<char>(std::toupper(value)) : value;
}

Piece fenToPiece(char value) {
    Piece piece;
    piece.color = std::isupper(static_cast<unsigned char>(value)) ? Color::White : Color::Black;
    switch (static_cast<char>(std::tolower(static_cast<unsigned char>(value)))) {
        case 'p': piece.type = PieceType::Pawn; break;
        case 'n': piece.type = PieceType::Knight; break;
        case 'b': piece.type = PieceType::Bishop; break;
        case 'r': piece.type = PieceType::Rook; break;
        case 'q': piece.type = PieceType::Queen; break;
        case 'k': piece.type = PieceType::King; break;
        default:  piece.type = PieceType::None; break;
    }
    return piece;
}

void pushPromotionMoves(std::vector<Move>& moves, int from, int to, std::uint8_t baseFlags) {
    for (PieceType promotion : {PieceType::Queen, PieceType::Rook, PieceType::Bishop, PieceType::Knight}) {
        moves.push_back(Move{static_cast<std::uint8_t>(from), static_cast<std::uint8_t>(to),
                             promotion, static_cast<std::uint8_t>(baseFlags | MovePromotion)});
    }
}

} // namespace

std::string squareName(int square) {
    if (!validSquare(square)) return "-";
    std::string result(2, ' ');
    result[0] = static_cast<char>('a' + fileOf(square));
    result[1] = static_cast<char>('1' + rankOf(square));
    return result;
}

int parseSquare(const std::string& text) {
    if (text.size() != 2) return NoSquare;
    const char file = static_cast<char>(std::tolower(static_cast<unsigned char>(text[0])));
    const char rank = text[1];
    if (file < 'a' || file > 'h' || rank < '1' || rank > '8') return NoSquare;
    return (rank - '1') * 8 + (file - 'a');
}

std::string moveToUci(const Move& move) {
    std::string result = squareName(move.from) + squareName(move.to);
    if (move.promotion != PieceType::None) {
        switch (move.promotion) {
            case PieceType::Knight: result += 'n'; break;
            case PieceType::Bishop: result += 'b'; break;
            case PieceType::Rook:   result += 'r'; break;
            case PieceType::Queen:  result += 'q'; break;
            default: break;
        }
    }
    return result;
}

Position::Position() = default;

Position Position::starting() {
    Position position;
    std::string ignored;
    fromFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", position, &ignored);
    return position;
}

bool Position::fromFen(const std::string& fen, Position& out, std::string* error) {
    auto fail = [&](const char* message) {
        if (error) *error = message;
        return false;
    };

    std::istringstream stream(fen);
    std::string boardPart, sidePart, castlingPart, epPart;
    int halfmove = 0;
    int fullmove = 1;
    if (!(stream >> boardPart >> sidePart >> castlingPart >> epPart >> halfmove >> fullmove))
        return fail("FEN must contain six fields");

    Position parsed;
    parsed.board_.fill(Empty);
    int rank = 7;
    int file = 0;
    int whiteKings = 0;
    int blackKings = 0;

    for (char value : boardPart) {
        if (value == '/') {
            if (file != 8 || rank == 0) return fail("Invalid board rows in FEN");
            --rank;
            file = 0;
            continue;
        }
        if (value >= '1' && value <= '8') {
            file += value - '0';
            if (file > 8) return fail("Too many squares in FEN row");
            continue;
        }
        Piece piece = fenToPiece(value);
        if (!piece || file >= 8 || rank < 0) return fail("Invalid piece in FEN");
        parsed.board_[rank * 8 + file] = piece;
        ++file;
        if (piece.type == PieceType::King)
            piece.color == Color::White ? ++whiteKings : ++blackKings;
    }
    if (rank != 0 || file != 8) return fail("FEN board does not contain 64 squares");
    if (whiteKings != 1 || blackKings != 1) return fail("FEN must contain exactly one king per side");

    if (sidePart == "w") parsed.sideToMove_ = Color::White;
    else if (sidePart == "b") parsed.sideToMove_ = Color::Black;
    else return fail("Invalid side to move in FEN");

    parsed.castlingRights_ = 0;
    if (castlingPart != "-") {
        for (char value : castlingPart) {
            std::uint8_t right = 0;
            switch (value) {
                case 'K': right = WhiteKingSide; break;
                case 'Q': right = WhiteQueenSide; break;
                case 'k': right = BlackKingSide; break;
                case 'q': right = BlackQueenSide; break;
                default: return fail("Invalid castling rights in FEN");
            }
            if (parsed.castlingRights_ & right) return fail("Duplicate castling right in FEN");
            parsed.castlingRights_ |= right;
        }
    }

    parsed.enPassantSquare_ = epPart == "-" ? NoSquare : parseSquare(epPart);
    if (epPart != "-" && parsed.enPassantSquare_ == NoSquare) return fail("Invalid en-passant square in FEN");
    if (parsed.enPassantSquare_ != NoSquare) {
        const int epRank = rankOf(parsed.enPassantSquare_);
        if (epRank != 2 && epRank != 5) return fail("Invalid en-passant rank in FEN");
    }
    if (halfmove < 0 || fullmove < 1) return fail("Invalid move counters in FEN");
    parsed.halfmoveClock_ = halfmove;
    parsed.fullmoveNumber_ = fullmove;

    out = parsed;
    if (error) error->clear();
    return true;
}

std::string Position::toFen() const {
    std::ostringstream stream;
    for (int rank = 7; rank >= 0; --rank) {
        int empty = 0;
        for (int file = 0; file < 8; ++file) {
            const Piece piece = board_[rank * 8 + file];
            if (!piece) {
                ++empty;
                continue;
            }
            if (empty) stream << empty, empty = 0;
            stream << pieceToFen(piece);
        }
        if (empty) stream << empty;
        if (rank) stream << '/';
    }
    stream << (sideToMove_ == Color::White ? " w " : " b ");
    if (!castlingRights_) stream << '-';
    else {
        if (castlingRights_ & WhiteKingSide) stream << 'K';
        if (castlingRights_ & WhiteQueenSide) stream << 'Q';
        if (castlingRights_ & BlackKingSide) stream << 'k';
        if (castlingRights_ & BlackQueenSide) stream << 'q';
    }
    stream << ' ' << (enPassantSquare_ == NoSquare ? "-" : squareName(enPassantSquare_));
    stream << ' ' << halfmoveClock_ << ' ' << fullmoveNumber_;
    return stream.str();
}

void Position::addPawnMoves(std::vector<Move>& moves, int from, Piece piece) const {
    const int direction = piece.color == Color::White ? 8 : -8;
    const int startRank = piece.color == Color::White ? 1 : 6;
    const int promotionRank = piece.color == Color::White ? 6 : 1;
    const int fromRank = rankOf(from);
    const int fromFile = fileOf(from);
    const int one = from + direction;

    if (validSquare(one) && !board_[one]) {
        if (fromRank == promotionRank) pushPromotionMoves(moves, from, one, MoveQuiet);
        else {
            moves.push_back(Move{static_cast<std::uint8_t>(from), static_cast<std::uint8_t>(one)});
            const int two = from + direction * 2;
            if (fromRank == startRank && !board_[two])
                moves.push_back(Move{static_cast<std::uint8_t>(from), static_cast<std::uint8_t>(two),
                                     PieceType::None, MoveDoublePawn});
        }
    }

    for (int fileDelta : {-1, 1}) {
        const int targetFile = fromFile + fileDelta;
        if (targetFile < 0 || targetFile > 7) continue;
        const int to = one + fileDelta;
        if (!validSquare(to)) continue;

        if (board_[to] && board_[to].color != piece.color) {
            if (fromRank == promotionRank) pushPromotionMoves(moves, from, to, MoveCapture);
            else moves.push_back(Move{static_cast<std::uint8_t>(from), static_cast<std::uint8_t>(to),
                                      PieceType::None, MoveCapture});
        } else if (to == enPassantSquare_) {
            const int capturedSquare = to - direction;
            const Piece captured = board_[capturedSquare];
            if (captured.type == PieceType::Pawn && captured.color != piece.color)
                moves.push_back(Move{static_cast<std::uint8_t>(from), static_cast<std::uint8_t>(to),
                                     PieceType::None,
                                     static_cast<std::uint8_t>(MoveCapture | MoveEnPassant)});
        }
    }
}

void Position::addKnightMoves(std::vector<Move>& moves, int from, Piece piece) const {
    constexpr int deltas[8][2] = {{1, 2}, {2, 1}, {2, -1}, {1, -2},
                                   {-1, -2}, {-2, -1}, {-2, 1}, {-1, 2}};
    const int fromFile = fileOf(from);
    const int fromRank = rankOf(from);
    for (const auto& delta : deltas) {
        const int file = fromFile + delta[0];
        const int rank = fromRank + delta[1];
        if (file < 0 || file > 7 || rank < 0 || rank > 7) continue;
        const int to = rank * 8 + file;
        if (!board_[to] || board_[to].color != piece.color)
            moves.push_back(Move{static_cast<std::uint8_t>(from), static_cast<std::uint8_t>(to),
                                 PieceType::None, board_[to] ? MoveCapture : MoveQuiet});
    }
}

void Position::addSlidingMoves(std::vector<Move>& moves, int from, Piece piece) const {
    constexpr int directions[8][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1},
                                       {1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
    const int begin = piece.type == PieceType::Bishop ? 4 : 0;
    const int end = piece.type == PieceType::Rook ? 4 : 8;
    const int fromFile = fileOf(from);
    const int fromRank = rankOf(from);
    for (int i = begin; i < end; ++i) {
        int file = fromFile + directions[i][0];
        int rank = fromRank + directions[i][1];
        while (file >= 0 && file < 8 && rank >= 0 && rank < 8) {
            const int to = rank * 8 + file;
            if (!board_[to]) {
                moves.push_back(Move{static_cast<std::uint8_t>(from), static_cast<std::uint8_t>(to)});
            } else {
                if (board_[to].color != piece.color)
                    moves.push_back(Move{static_cast<std::uint8_t>(from), static_cast<std::uint8_t>(to),
                                         PieceType::None, MoveCapture});
                break;
            }
            file += directions[i][0];
            rank += directions[i][1];
        }
    }
}

void Position::addKingMoves(std::vector<Move>& moves, int from, Piece piece) const {
    const int fromFile = fileOf(from);
    const int fromRank = rankOf(from);
    for (int rankDelta = -1; rankDelta <= 1; ++rankDelta) {
        for (int fileDelta = -1; fileDelta <= 1; ++fileDelta) {
            if (!fileDelta && !rankDelta) continue;
            const int file = fromFile + fileDelta;
            const int rank = fromRank + rankDelta;
            if (file < 0 || file > 7 || rank < 0 || rank > 7) continue;
            const int to = rank * 8 + file;
            if (!board_[to] || board_[to].color != piece.color)
                moves.push_back(Move{static_cast<std::uint8_t>(from), static_cast<std::uint8_t>(to),
                                     PieceType::None, board_[to] ? MoveCapture : MoveQuiet});
        }
    }

    const Color enemy = opposite(piece.color);
    if (piece.color == Color::White && from == 4 && !inCheck(Color::White)) {
        if ((castlingRights_ & WhiteKingSide) && board_[7] == Piece{PieceType::Rook, Color::White}
            && !board_[5] && !board_[6]
            && !isSquareAttacked(5, enemy) && !isSquareAttacked(6, enemy))
            moves.push_back(Move{4, 6, PieceType::None, MoveCastle});
        if ((castlingRights_ & WhiteQueenSide) && board_[0] == Piece{PieceType::Rook, Color::White}
            && !board_[1] && !board_[2] && !board_[3]
            && !isSquareAttacked(3, enemy) && !isSquareAttacked(2, enemy))
            moves.push_back(Move{4, 2, PieceType::None, MoveCastle});
    } else if (piece.color == Color::Black && from == 60 && !inCheck(Color::Black)) {
        if ((castlingRights_ & BlackKingSide) && board_[63] == Piece{PieceType::Rook, Color::Black}
            && !board_[61] && !board_[62]
            && !isSquareAttacked(61, enemy) && !isSquareAttacked(62, enemy))
            moves.push_back(Move{60, 62, PieceType::None, MoveCastle});
        if ((castlingRights_ & BlackQueenSide) && board_[56] == Piece{PieceType::Rook, Color::Black}
            && !board_[57] && !board_[58] && !board_[59]
            && !isSquareAttacked(59, enemy) && !isSquareAttacked(58, enemy))
            moves.push_back(Move{60, 58, PieceType::None, MoveCastle});
    }
}

std::vector<Move> Position::pseudoLegalMoves() const {
    std::vector<Move> moves;
    moves.reserve(64);
    for (int from = 0; from < 64; ++from) {
        const Piece piece = board_[from];
        if (!piece || piece.color != sideToMove_) continue;
        switch (piece.type) {
            case PieceType::Pawn: addPawnMoves(moves, from, piece); break;
            case PieceType::Knight: addKnightMoves(moves, from, piece); break;
            case PieceType::Bishop:
            case PieceType::Rook:
            case PieceType::Queen: addSlidingMoves(moves, from, piece); break;
            case PieceType::King: addKingMoves(moves, from, piece); break;
            case PieceType::None: break;
        }
    }
    return moves;
}

std::vector<Move> Position::legalMoves() const {
    std::vector<Move> legal;
    const Color movingSide = sideToMove_;
    for (const Move& move : pseudoLegalMoves()) {
        Position next = *this;
        next.applyUnchecked(move);
        if (!next.inCheck(movingSide)) legal.push_back(move);
    }
    return legal;
}

std::vector<Move> Position::legalMovesFrom(int square) const {
    std::vector<Move> result;
    if (!validSquare(square)) return result;
    for (const Move& move : legalMoves())
        if (move.from == square) result.push_back(move);
    return result;
}

bool Position::findLegalMoveUci(const std::string& uci, Move& out) const {
    if (uci.size() != 4 && uci.size() != 5) return false;
    const int from = parseSquare(uci.substr(0, 2));
    const int to = parseSquare(uci.substr(2, 2));
    if (from == NoSquare || to == NoSquare) return false;
    PieceType promotion = PieceType::None;
    if (uci.size() == 5) {
        switch (static_cast<char>(std::tolower(static_cast<unsigned char>(uci[4])))) {
            case 'q': promotion = PieceType::Queen; break;
            case 'r': promotion = PieceType::Rook; break;
            case 'b': promotion = PieceType::Bishop; break;
            case 'n': promotion = PieceType::Knight; break;
            default: return false;
        }
    }
    for (const Move& move : legalMoves()) {
        if (move.from == from && move.to == to && move.promotion == promotion) {
            out = move;
            return true;
        }
    }
    return false;
}

bool Position::isLegal(const Move& move) const {
    const auto moves = legalMoves();
    return std::any_of(moves.begin(), moves.end(), [&](const Move& legal) {
        return legal.sameCoordinates(move);
    });
}

bool Position::makeMove(const Move& move) {
    for (const Move& legal : legalMoves()) {
        if (legal.sameCoordinates(move)) {
            applyUnchecked(legal);
            return true;
        }
    }
    return false;
}

void Position::applyUnchecked(const Move& move) {
    const Piece moving = board_[move.from];
    const Color movingSide = sideToMove_;
    Piece captured = board_[move.to];
    const int capturedSquare = move.isEnPassant()
        ? static_cast<int>(move.to) + (movingSide == Color::White ? -8 : 8)
        : static_cast<int>(move.to);
    if (move.isEnPassant()) {
        captured = board_[capturedSquare];
        board_[capturedSquare] = Empty;
    }

    board_[move.from] = Empty;
    board_[move.to] = moving;
    if (move.isPromotion()) board_[move.to].type = move.promotion;

    if (move.isCastle()) {
        if (move.to == 6) board_[5] = board_[7], board_[7] = Empty;
        else if (move.to == 2) board_[3] = board_[0], board_[0] = Empty;
        else if (move.to == 62) board_[61] = board_[63], board_[63] = Empty;
        else if (move.to == 58) board_[59] = board_[56], board_[56] = Empty;
    }

    if (moving.type == PieceType::King) {
        if (movingSide == Color::White)
            castlingRights_ &= static_cast<std::uint8_t>(~(WhiteKingSide | WhiteQueenSide));
        else
            castlingRights_ &= static_cast<std::uint8_t>(~(BlackKingSide | BlackQueenSide));
    }
    if (moving.type == PieceType::Rook) {
        if (move.from == 0) castlingRights_ &= static_cast<std::uint8_t>(~WhiteQueenSide);
        if (move.from == 7) castlingRights_ &= static_cast<std::uint8_t>(~WhiteKingSide);
        if (move.from == 56) castlingRights_ &= static_cast<std::uint8_t>(~BlackQueenSide);
        if (move.from == 63) castlingRights_ &= static_cast<std::uint8_t>(~BlackKingSide);
    }
    if (captured.type == PieceType::Rook) {
        if (capturedSquare == 0) castlingRights_ &= static_cast<std::uint8_t>(~WhiteQueenSide);
        if (capturedSquare == 7) castlingRights_ &= static_cast<std::uint8_t>(~WhiteKingSide);
        if (capturedSquare == 56) castlingRights_ &= static_cast<std::uint8_t>(~BlackQueenSide);
        if (capturedSquare == 63) castlingRights_ &= static_cast<std::uint8_t>(~BlackKingSide);
    }

    enPassantSquare_ = NoSquare;
    if (moving.type == PieceType::Pawn && std::abs(static_cast<int>(move.to) - static_cast<int>(move.from)) == 16)
        enPassantSquare_ = (move.from + move.to) / 2;

    if (moving.type == PieceType::Pawn || captured) halfmoveClock_ = 0;
    else ++halfmoveClock_;
    if (movingSide == Color::Black) ++fullmoveNumber_;
    sideToMove_ = opposite(sideToMove_);
}

int Position::kingSquare(Color color) const {
    for (int square = 0; square < 64; ++square)
        if (board_[square] == Piece{PieceType::King, color}) return square;
    return NoSquare;
}

bool Position::inCheck(Color color) const {
    const int king = kingSquare(color);
    return king == NoSquare || isSquareAttacked(king, opposite(color));
}

bool Position::isSquareAttacked(int square, Color byColor) const {
    const int file = fileOf(square);
    const int rank = rankOf(square);

    const int pawnRank = rank + (byColor == Color::White ? -1 : 1);
    if (pawnRank >= 0 && pawnRank < 8) {
        for (int delta : {-1, 1}) {
            const int pawnFile = file + delta;
            if (pawnFile >= 0 && pawnFile < 8
                && board_[pawnRank * 8 + pawnFile] == Piece{PieceType::Pawn, byColor})
                return true;
        }
    }

    constexpr int knightDeltas[8][2] = {{1, 2}, {2, 1}, {2, -1}, {1, -2},
                                         {-1, -2}, {-2, -1}, {-2, 1}, {-1, 2}};
    for (const auto& delta : knightDeltas) {
        const int attackerFile = file + delta[0];
        const int attackerRank = rank + delta[1];
        if (attackerFile >= 0 && attackerFile < 8 && attackerRank >= 0 && attackerRank < 8
            && board_[attackerRank * 8 + attackerFile] == Piece{PieceType::Knight, byColor})
            return true;
    }

    for (int rankDelta = -1; rankDelta <= 1; ++rankDelta)
        for (int fileDelta = -1; fileDelta <= 1; ++fileDelta) {
            if (!fileDelta && !rankDelta) continue;
            const int attackerFile = file + fileDelta;
            const int attackerRank = rank + rankDelta;
            if (attackerFile >= 0 && attackerFile < 8 && attackerRank >= 0 && attackerRank < 8
                && board_[attackerRank * 8 + attackerFile] == Piece{PieceType::King, byColor})
                return true;
        }

    constexpr int directions[8][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1},
                                       {1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
    for (int i = 0; i < 8; ++i) {
        int attackerFile = file + directions[i][0];
        int attackerRank = rank + directions[i][1];
        while (attackerFile >= 0 && attackerFile < 8 && attackerRank >= 0 && attackerRank < 8) {
            const Piece piece = board_[attackerRank * 8 + attackerFile];
            if (piece) {
                if (piece.color == byColor
                    && (piece.type == PieceType::Queen
                        || (i < 4 && piece.type == PieceType::Rook)
                        || (i >= 4 && piece.type == PieceType::Bishop)))
                    return true;
                break;
            }
            attackerFile += directions[i][0];
            attackerRank += directions[i][1];
        }
    }
    return false;
}

bool Position::hasInsufficientMaterial() const {
    int knights = 0;
    int bishops = 0;
    int firstBishopColor = -1;
    bool bishopsSameColor = true;
    for (int square = 0; square < 64; ++square) {
        const Piece piece = board_[square];
        if (!piece || piece.type == PieceType::King) continue;
        if (piece.type == PieceType::Pawn || piece.type == PieceType::Rook || piece.type == PieceType::Queen)
            return false;
        if (piece.type == PieceType::Knight) ++knights;
        if (piece.type == PieceType::Bishop) {
            ++bishops;
            const int squareColor = (fileOf(square) + rankOf(square)) & 1;
            if (firstBishopColor < 0) firstBishopColor = squareColor;
            else if (squareColor != firstBishopColor) bishopsSameColor = false;
        }
    }
    if (knights == 0 && bishops == 0) return true;
    if (knights + bishops == 1) return true;
    return knights == 0 && bishops > 0 && bishopsSameColor;
}

std::uint64_t Position::repetitionKey() const {
    constexpr std::uint64_t offset = 1469598103934665603ULL;
    constexpr std::uint64_t prime = 1099511628211ULL;
    std::uint64_t hash = offset;
    auto mix = [&](std::uint8_t value) { hash ^= value; hash *= prime; };
    for (const Piece piece : board_) {
        const std::uint8_t encoded = piece
            ? static_cast<std::uint8_t>(static_cast<int>(piece.type) + (piece.color == Color::Black ? 8 : 0))
            : 0;
        mix(encoded);
    }
    mix(sideToMove_ == Color::White ? 1 : 2);
    mix(castlingRights_);

    int normalizedEp = NoSquare;
    if (enPassantSquare_ != NoSquare) {
        const int pawnRank = rankOf(enPassantSquare_) + (sideToMove_ == Color::White ? -1 : 1);
        const int epFile = fileOf(enPassantSquare_);
        for (int delta : {-1, 1}) {
            const int pawnFile = epFile + delta;
            if (pawnFile >= 0 && pawnFile < 8 && pawnRank >= 0 && pawnRank < 8
                && board_[pawnRank * 8 + pawnFile] == Piece{PieceType::Pawn, sideToMove_}) {
                normalizedEp = enPassantSquare_;
                break;
            }
        }
    }
    mix(normalizedEp == NoSquare ? 0 : static_cast<std::uint8_t>(normalizedEp + 1));
    return hash;
}

std::uint64_t Position::perft(int depth) const {
    if (depth <= 0) return 1;
    std::uint64_t nodes = 0;
    for (const Move& move : legalMoves()) {
        Position next = *this;
        next.applyUnchecked(move);
        nodes += next.perft(depth - 1);
    }
    return nodes;
}

} // namespace chess3ds::chess
