#include "platform/Storage.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
#include <sys/stat.h>
#include <utility>

namespace chess3ds::platform {
namespace {

std::map<std::string, std::string> readValues(const std::string& filename, const char* header) {
    std::ifstream file(filename);
    std::map<std::string, std::string> values;
    std::string line;
    if (!file || !std::getline(file, line) || line != header) return values;
    while (std::getline(file, line)) {
        const std::size_t separator = line.find('=');
        if (separator == std::string::npos) continue;
        values[line.substr(0, separator)] = line.substr(separator + 1);
    }
    return values;
}

int readInt(const std::map<std::string, std::string>& values, const char* key, int fallback) {
    const auto found = values.find(key);
    if (found == values.end()) return fallback;
    char* end = nullptr;
    errno = 0;
    const long value = std::strtol(found->second.c_str(), &end, 10);
    return !errno && end && *end == '\0' ? static_cast<int>(value) : fallback;
}

std::int64_t readInt64(const std::map<std::string, std::string>& values,
                       const char* key, std::int64_t fallback) {
    const auto found = values.find(key);
    if (found == values.end()) return fallback;
    char* end = nullptr;
    errno = 0;
    const long long value = std::strtoll(found->second.c_str(), &end, 10);
    return !errno && end && *end == '\0' ? static_cast<std::int64_t>(value) : fallback;
}

std::vector<std::string> splitMoves(const std::string& text) {
    std::istringstream stream(text);
    std::vector<std::string> moves;
    std::string move;
    while (stream >> move) moves.push_back(move);
    return moves;
}

bool fileExists(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    return static_cast<bool>(file);
}

} // namespace

ClockPreset clockPreset(TimeControl control) {
    switch (control) {
        case TimeControl::Rapid10: return {10 * 60 * 1000, 0, "10 min"};
        case TimeControl::Blitz5: return {5 * 60 * 1000, 0, "5 min"};
        case TimeControl::Blitz3Increment2: return {3 * 60 * 1000, 2 * 1000, "3 + 2"};
        case TimeControl::Unlimited: return {0, 0, "No clock"};
    }
    return {0, 0, "No clock"};
}

Storage::Storage(std::string basePath)
    : basePath_(basePath.empty() ? "sdmc:/3ds/Chess3DS" : std::move(basePath)) {}

bool Storage::initialize() {
    if (basePath_.rfind("sdmc:/", 0) == 0) {
        ::mkdir("sdmc:/3ds", 0777);
    }
    if (::mkdir(basePath_.c_str(), 0777) == 0) return true;
    struct stat info {};
    return ::stat(basePath_.c_str(), &info) == 0 && S_ISDIR(info.st_mode);
}

std::string Storage::path(const char* filename) const {
    return basePath_ + "/" + filename;
}

bool Storage::atomicWrite(const std::string& filename, const std::string& data) {
    const std::string temporary = filename + ".tmp";
    {
        std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
        if (!file) return false;
        file.write(data.data(), static_cast<std::streamsize>(data.size()));
        file.flush();
        if (!file) return false;
    }
    std::remove(filename.c_str());
    if (std::rename(temporary.c_str(), filename.c_str()) == 0) return true;
    std::remove(temporary.c_str());
    return false;
}

bool Storage::loadSettings(Settings& settings) const {
    const auto values = readValues(path("settings.cfg"), "CHESS3DS_SETTINGS_V1");
    if (values.empty()) return false;
    settings.engineLevel = std::clamp(readInt(values, "engineLevel", settings.engineLevel), 1, 8);
    settings.theme = static_cast<Theme>(std::clamp(readInt(values, "theme", 0), 0,
                                                   static_cast<int>(Theme::Count) - 1));
    settings.language = readInt(values, "language", 0) == 1 ? Language::English : Language::Polish;
    settings.sound = readInt(values, "sound", 1) != 0;
    settings.showLegalMoves = readInt(values, "showLegalMoves", 1) != 0;
    settings.animations = readInt(values, "animations", 1) != 0;
    settings.autoFlipLocal = readInt(values, "autoFlipLocal", 1) != 0;
    return true;
}

bool Storage::saveSettings(const Settings& settings) const {
    std::ostringstream data;
    data << "CHESS3DS_SETTINGS_V1\n"
         << "engineLevel=" << std::clamp(settings.engineLevel, 1, 8) << '\n'
         << "theme=" << static_cast<int>(settings.theme) << '\n'
         << "language=" << static_cast<int>(settings.language) << '\n'
         << "sound=" << settings.sound << '\n'
         << "showLegalMoves=" << settings.showLegalMoves << '\n'
         << "animations=" << settings.animations << '\n'
         << "autoFlipLocal=" << settings.autoFlipLocal << '\n';
    return atomicWrite(path("settings.cfg"), data.str());
}

bool Storage::loadStatistics(Statistics& statistics) const {
    const auto values = readValues(path("statistics.cfg"), "CHESS3DS_STATISTICS_V1");
    if (values.empty()) return false;
    statistics.cpuWins = static_cast<std::uint32_t>(std::max(0, readInt(values, "cpuWins", 0)));
    statistics.cpuLosses = static_cast<std::uint32_t>(std::max(0, readInt(values, "cpuLosses", 0)));
    statistics.cpuDraws = static_cast<std::uint32_t>(std::max(0, readInt(values, "cpuDraws", 0)));
    statistics.localGames = static_cast<std::uint32_t>(std::max(0, readInt(values, "localGames", 0)));
    statistics.exportedGames = static_cast<std::uint32_t>(std::max(0, readInt(values, "exportedGames", 0)));
    statistics.highestDefeatedLevel = std::clamp(readInt(values, "highestDefeatedLevel", 0), 0, 8);
    return true;
}

bool Storage::saveStatistics(const Statistics& statistics) const {
    std::ostringstream data;
    data << "CHESS3DS_STATISTICS_V1\n"
         << "cpuWins=" << statistics.cpuWins << '\n'
         << "cpuLosses=" << statistics.cpuLosses << '\n'
         << "cpuDraws=" << statistics.cpuDraws << '\n'
         << "localGames=" << statistics.localGames << '\n'
         << "exportedGames=" << statistics.exportedGames << '\n'
         << "highestDefeatedLevel=" << statistics.highestDefeatedLevel << '\n';
    return atomicWrite(path("statistics.cfg"), data.str());
}

bool Storage::loadGame(SavedGame& game) const {
    const auto values = readValues(path("saved_game.dat"), "CHESS3DS_SAVE_V1");
    if (values.empty() || !values.count("fen")) return false;
    game.mode = readInt(values, "mode", 0) == 1 ? GameMode::Local : GameMode::Cpu;
    game.timeControl = static_cast<TimeControl>(std::clamp(readInt(values, "timeControl", 0), 0, 3));
    game.playerColor = readInt(values, "playerColor", 0) == 1 ? chess::Color::Black : chess::Color::White;
    game.engineLevel = std::clamp(readInt(values, "engineLevel", 4), 1, 8);
    game.whiteTimeMs = std::max<std::int64_t>(0, readInt64(values, "whiteTimeMs", 0));
    game.blackTimeMs = std::max<std::int64_t>(0, readInt64(values, "blackTimeMs", 0));
    game.incrementMs = std::max<std::int64_t>(0, readInt64(values, "incrementMs", 0));
    game.initialFen = values.at("fen");
    const auto moves = values.find("moves");
    game.moves = moves == values.end() ? std::vector<std::string>{} : splitMoves(moves->second);
    return true;
}

bool Storage::saveGame(const SavedGame& game) const {
    std::ostringstream data;
    data << "CHESS3DS_SAVE_V1\n"
         << "mode=" << static_cast<int>(game.mode) << '\n'
         << "timeControl=" << static_cast<int>(game.timeControl) << '\n'
         << "playerColor=" << static_cast<int>(game.playerColor) << '\n'
         << "engineLevel=" << std::clamp(game.engineLevel, 1, 8) << '\n'
         << "whiteTimeMs=" << std::max<std::int64_t>(0, game.whiteTimeMs) << '\n'
         << "blackTimeMs=" << std::max<std::int64_t>(0, game.blackTimeMs) << '\n'
         << "incrementMs=" << std::max<std::int64_t>(0, game.incrementMs) << '\n'
         << "fen=" << game.initialFen << '\n'
         << "moves=";
    for (std::size_t i = 0; i < game.moves.size(); ++i) {
        if (i) data << ' ';
        data << game.moves[i];
    }
    data << '\n';
    return atomicWrite(path("saved_game.dat"), data.str());
}

bool Storage::hasSavedGame() const {
    return fileExists(path("saved_game.dat"));
}

bool Storage::clearSavedGame() const {
    const std::string filename = path("saved_game.dat");
    return !fileExists(filename) || std::remove(filename.c_str()) == 0;
}

bool Storage::exportPgn(const std::string& pgn, Statistics& statistics, std::string* writtenPath) const {
    for (std::uint32_t attempt = 1; attempt < 100000; ++attempt) {
        const std::uint32_t number = statistics.exportedGames + attempt;
        std::ostringstream name;
        name << "game_" << std::setfill('0') << std::setw(4) << number << ".pgn";
        const std::string filename = path(name.str().c_str());
        if (fileExists(filename)) continue;
        if (!atomicWrite(filename, pgn)) return false;
        statistics.exportedGames = number;
        if (writtenPath) *writtenPath = filename;
        return true;
    }
    return false;
}

} // namespace chess3ds::platform
