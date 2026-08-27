#include "chess/Game.hpp"

#include <algorithm>
#include <sstream>

namespace chess3ds::chess {
namespace {

char sanPieceLetter(PieceType type) {
    switch (type) {
        case PieceType::Knight: return 'N';
        case PieceType::Bishop: return 'B';
        case PieceType::Rook: return 'R';
        case PieceType::Queen: return 'Q';
        case PieceType::King: return 'K';
        default: return '\0';
    }
}

} // namespace

Game::Game() { reset(); }

void Game::reset() {
    reset(Position::starting());
}

void Game::reset(const Position& position, const std::string& initialFen) {
    position_ = position;
    initialFen_ = initialFen.empty() ? position.toFen() : initialFen;
    history_.clear();
    repetitions_.clear();
    repetitions_[position_.repetitionKey()] = 1;
    forcedResult_ = GameResult::Ongoing;
}

bool Game::restore(const std::string& initialFen, const std::vector<std::string>& uciMoves,
                   std::string* error) {
    Position initial;
    if (!Position::fromFen(initialFen, initial, error)) return false;
    reset(initial, initialFen);
    for (std::size_t i = 0; i < uciMoves.size(); ++i) {
        if (!playUci(uciMoves[i])) {
            if (error) {
                std::ostringstream message;
                message << "Illegal saved move " << (i + 1) << ": " << uciMoves[i];
                *error = message.str();
            }
            reset();
            return false;
        }
    }
    if (error) error->clear();
    return true;
}

bool Game::play(const Move& requestedMove) {
    if (isOver()) return false;
    Move move;
    bool found = false;
    for (const Move& legal : position_.legalMoves()) {
        if (legal.sameCoordinates(requestedMove)) {
            move = legal;
            found = true;
            break;
        }
    }
    if (!found) return false;

    PlyRecord record{position_, move, sanForMove(position_, move)};
    position_.makeMove(move);
    history_.push_back(std::move(record));
    ++repetitions_[position_.repetitionKey()];
    return true;
}

bool Game::playUci(const std::string& uci) {
    Move move;
    return position_.findLegalMoveUci(uci, move) && play(move);
}

bool Game::undo(int plies) {
    if (plies <= 0 || history_.empty()) return false;
    plies = std::min<int>(plies, static_cast<int>(history_.size()));
    while (plies-- > 0) {
        position_ = history_.back().before;
        history_.pop_back();
    }
    forcedResult_ = GameResult::Ongoing;
    rebuildRepetitions();
    return true;
}

void Game::rebuildRepetitions() {
    repetitions_.clear();
    if (history_.empty()) {
        repetitions_[position_.repetitionKey()] = 1;
        return;
    }
    ++repetitions_[history_.front().before.repetitionKey()];
    for (const PlyRecord& record : history_) {
        Position after = record.before;
        after.makeMove(record.move);
        ++repetitions_[after.repetitionKey()];
    }
}

GameResult Game::result() const {
    if (forcedResult_ != GameResult::Ongoing) return forcedResult_;

    const auto legal = position_.legalMoves();
    if (legal.empty()) {
        if (position_.inCheck(position_.sideToMove()))
            return position_.sideToMove() == Color::White
                ? GameResult::BlackCheckmate : GameResult::WhiteCheckmate;
        return GameResult::DrawStalemate;
    }
    if (position_.halfmoveClock() >= 100) return GameResult::DrawFiftyMove;
    const auto repetition = repetitions_.find(position_.repetitionKey());
    if (repetition != repetitions_.end() && repetition->second >= 3)
        return GameResult::DrawRepetition;
    if (position_.hasInsufficientMaterial()) return GameResult::DrawInsufficientMaterial;
    return GameResult::Ongoing;
}

void Game::resign(Color color) {
    if (result() != GameResult::Ongoing) return;
    forcedResult_ = color == Color::White ? GameResult::WhiteResigned : GameResult::BlackResigned;
}

void Game::timeout(Color color) {
    if (result() != GameResult::Ongoing) return;
    forcedResult_ = color == Color::White ? GameResult::WhiteTimeout : GameResult::BlackTimeout;
}

std::vector<std::string> Game::uciHistory() const {
    std::vector<std::string> moves;
    moves.reserve(history_.size());
    for (const PlyRecord& record : history_) moves.push_back(moveToUci(record.move));
    return moves;
}

std::string Game::moveListText(std::size_t maxPlies) const {
    const std::size_t begin = maxPlies && history_.size() > maxPlies ? history_.size() - maxPlies : 0;
    std::ostringstream stream;
    for (std::size_t i = begin; i < history_.size(); ++i) {
        if (i && i != begin) stream << ' ';
        if ((i & 1u) == 0) stream << (i / 2 + 1) << ". ";
        stream << history_[i].san;
    }
    return stream.str();
}

std::string Game::toPgn(const std::string& whiteName, const std::string& blackName,
                        const std::string& eventName) const {
    const GameResult currentResult = result();
    std::ostringstream stream;
    stream << "[Event \"" << eventName << "\"]\n"
           << "[Site \"Nintendo 3DS\"]\n"
           << "[White \"" << whiteName << "\"]\n"
           << "[Black \"" << blackName << "\"]\n"
           << "[Result \"" << pgnResult(currentResult) << "\"]\n";
    if (initialFen_ != Position::starting().toFen())
        stream << "[SetUp \"1\"]\n[FEN \"" << initialFen_ << "\"]\n";
    stream << '\n';

    std::size_t lineLength = 0;
    for (std::size_t i = 0; i < history_.size(); ++i) {
        std::ostringstream token;
        if ((i & 1u) == 0) token << (i / 2 + 1) << ". ";
        token << history_[i].san << ' ';
        const std::string value = token.str();
        if (lineLength && lineLength + value.size() > 78) {
            stream << '\n';
            lineLength = 0;
        }
        stream << value;
        lineLength += value.size();
    }
    stream << pgnResult(currentResult) << '\n';
    return stream.str();
}

std::string Game::sanForMove(const Position& position, const Move& requestedMove) {
    Move move;
    bool found = false;
    const auto legalMoves = position.legalMoves();
    for (const Move& legal : legalMoves) {
        if (legal.sameCoordinates(requestedMove)) {
            move = legal;
            found = true;
            break;
        }
    }
    if (!found) return "?";

    const Piece moving = position.pieceAt(move.from);
    std::string san;
    if (move.isCastle()) {
        san = fileOf(move.to) == 6 ? "O-O" : "O-O-O";
    } else {
        const char letter = sanPieceLetter(moving.type);
        if (letter) san += letter;

        if (moving.type != PieceType::Pawn) {
            bool conflict = false;
            bool sameFile = false;
            bool sameRank = false;
            for (const Move& other : legalMoves) {
                if (other.from == move.from || other.to != move.to) continue;
                const Piece otherPiece = position.pieceAt(other.from);
                if (otherPiece.type != moving.type || otherPiece.color != moving.color) continue;
                conflict = true;
                if (fileOf(other.from) == fileOf(move.from)) sameFile = true;
                if (rankOf(other.from) == rankOf(move.from)) sameRank = true;
            }
            if (conflict) {
                if (!sameFile) san += static_cast<char>('a' + fileOf(move.from));
                else if (!sameRank) san += static_cast<char>('1' + rankOf(move.from));
                else san += squareName(move.from);
            }
        } else if (move.isCapture()) {
            san += static_cast<char>('a' + fileOf(move.from));
        }

        if (move.isCapture()) san += 'x';
        san += squareName(move.to);
        if (move.isPromotion()) {
            san += '=';
            san += sanPieceLetter(move.promotion);
        }
    }

    Position after = position;
    after.makeMove(move);
    if (after.inCheck(after.sideToMove()))
        san += after.legalMoves().empty() ? '#' : '+';
    return san;
}

const char* Game::resultLabel(GameResult value) {
    switch (value) {
        case GameResult::Ongoing: return "Game in progress";
        case GameResult::WhiteCheckmate: return "White wins by checkmate";
        case GameResult::BlackCheckmate: return "Black wins by checkmate";
        case GameResult::DrawStalemate: return "Draw by stalemate";
        case GameResult::DrawRepetition: return "Draw by repetition";
        case GameResult::DrawFiftyMove: return "Draw by 50-move rule";
        case GameResult::DrawInsufficientMaterial: return "Draw: insufficient material";
        case GameResult::WhiteResigned: return "Black wins by resignation";
        case GameResult::BlackResigned: return "White wins by resignation";
        case GameResult::WhiteTimeout: return "Black wins on time";
        case GameResult::BlackTimeout: return "White wins on time";
    }
    return "Game over";
}

const char* Game::pgnResult(GameResult value) {
    switch (value) {
        case GameResult::WhiteCheckmate:
        case GameResult::BlackResigned:
        case GameResult::BlackTimeout: return "1-0";
        case GameResult::BlackCheckmate:
        case GameResult::WhiteResigned:
        case GameResult::WhiteTimeout: return "0-1";
        case GameResult::DrawStalemate:
        case GameResult::DrawRepetition:
        case GameResult::DrawFiftyMove:
        case GameResult::DrawInsufficientMaterial: return "1/2-1/2";
        case GameResult::Ongoing: return "*";
    }
    return "*";
}

} // namespace chess3ds::chess
