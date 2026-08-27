#pragma once

#include "chess/Types.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace chess3ds::platform {

enum class Theme : std::uint8_t { Classic = 0, Midnight, Forest, Rosewood, Count };
enum class Language : std::uint8_t { Polish = 0, English };
enum class GameMode : std::uint8_t { Cpu = 0, Local };
enum class TimeControl : std::uint8_t { Unlimited = 0, Rapid10, Blitz5, Blitz3Increment2 };

struct Settings {
    int engineLevel{4};
    Theme theme{Theme::Classic};
    Language language{Language::Polish};
    bool sound{true};
    bool showLegalMoves{true};
    bool animations{true};
    bool autoFlipLocal{true};
};

struct Statistics {
    std::uint32_t cpuWins{0};
    std::uint32_t cpuLosses{0};
    std::uint32_t cpuDraws{0};
    std::uint32_t localGames{0};
    std::uint32_t exportedGames{0};
    int highestDefeatedLevel{0};
};

struct SavedGame {
    GameMode mode{GameMode::Cpu};
    TimeControl timeControl{TimeControl::Unlimited};
    chess::Color playerColor{chess::Color::White};
    int engineLevel{4};
    std::int64_t whiteTimeMs{0};
    std::int64_t blackTimeMs{0};
    std::int64_t incrementMs{0};
    std::string initialFen;
    std::vector<std::string> moves;
};

struct ClockPreset {
    std::int64_t initialMs;
    std::int64_t incrementMs;
    const char* label;
};

ClockPreset clockPreset(TimeControl control);

class Storage {
public:
    explicit Storage(std::string basePath = {});

    bool initialize();
    bool loadSettings(Settings& settings) const;
    bool saveSettings(const Settings& settings) const;
    bool loadStatistics(Statistics& statistics) const;
    bool saveStatistics(const Statistics& statistics) const;
    bool loadGame(SavedGame& game) const;
    bool saveGame(const SavedGame& game) const;
    bool hasSavedGame() const;
    bool clearSavedGame() const;
    bool exportPgn(const std::string& pgn, Statistics& statistics, std::string* writtenPath = nullptr) const;

    const std::string& basePath() const { return basePath_; }

private:
    std::string basePath_;

    std::string path(const char* filename) const;
    static bool atomicWrite(const std::string& filename, const std::string& data);
};

} // namespace chess3ds::platform
