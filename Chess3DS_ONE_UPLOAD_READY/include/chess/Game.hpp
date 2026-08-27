#pragma once

#include "chess/Position.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace chess3ds::chess {

enum class GameResult : std::uint8_t {
    Ongoing = 0,
    WhiteCheckmate,
    BlackCheckmate,
    DrawStalemate,
    DrawRepetition,
    DrawFiftyMove,
    DrawInsufficientMaterial,
    WhiteResigned,
    BlackResigned,
    WhiteTimeout,
    BlackTimeout,
};

struct PlyRecord {
    Position before;
    Move move;
    std::string san;
};

class Game {
public:
    Game();

    void reset();
    void reset(const Position& position, const std::string& initialFen = {});
    bool restore(const std::string& initialFen, const std::vector<std::string>& uciMoves,
                 std::string* error = nullptr);

    const Position& position() const { return position_; }
    const std::vector<PlyRecord>& history() const { return history_; }
    const std::string& initialFen() const { return initialFen_; }

    bool play(const Move& move);
    bool playUci(const std::string& uci);
    bool undo(int plies = 1);

    GameResult result() const;
    bool isOver() const { return result() != GameResult::Ongoing; }
    void resign(Color color);
    void timeout(Color color);

    std::vector<std::string> uciHistory() const;
    std::string moveListText(std::size_t maxPlies = 0) const;
    std::string toPgn(const std::string& whiteName, const std::string& blackName,
                      const std::string& eventName = "Chess3DS Game") const;

    static std::string sanForMove(const Position& position, const Move& move);
    static const char* resultLabel(GameResult result);
    static const char* pgnResult(GameResult result);

private:
    Position position_;
    std::string initialFen_;
    std::vector<PlyRecord> history_;
    std::unordered_map<std::uint64_t, int> repetitions_;
    GameResult forcedResult_{GameResult::Ongoing};

    void rebuildRepetitions();
};

} // namespace chess3ds::chess
