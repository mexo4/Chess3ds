#include "platform/Storage.hpp"

#include <cstdlib>
#include <iostream>

using namespace chess3ds;

namespace {
int failures = 0;
void check(bool value, const char* label) {
    if (!value) std::cerr << "FAIL: " << label << '\n', ++failures;
}
}

int main(int argc, char** argv) {
    if (argc != 2) return EXIT_FAILURE;
    platform::Storage storage(argv[1]);
    check(storage.initialize(), "initialize storage");

    platform::Settings settings;
    settings.engineLevel = 8;
    settings.theme = platform::Theme::Forest;
    settings.language = platform::Language::English;
    settings.sound = false;
    check(storage.saveSettings(settings), "save settings");
    platform::Settings loadedSettings;
    check(storage.loadSettings(loadedSettings), "load settings");
    check(loadedSettings.engineLevel == 8 && loadedSettings.theme == platform::Theme::Forest
          && loadedSettings.language == platform::Language::English && !loadedSettings.sound,
          "settings round-trip");

    platform::SavedGame saved;
    saved.mode = platform::GameMode::Cpu;
    saved.timeControl = platform::TimeControl::Blitz3Increment2;
    saved.playerColor = chess::Color::Black;
    saved.engineLevel = 7;
    saved.whiteTimeMs = 123456;
    saved.blackTimeMs = 98765;
    saved.incrementMs = 2000;
    saved.initialFen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    saved.moves = {"e2e4", "e7e5", "g1f3"};
    check(storage.saveGame(saved), "save game");
    check(storage.hasSavedGame(), "saved game exists");
    platform::SavedGame loaded;
    check(storage.loadGame(loaded), "load game");
    check(loaded.mode == saved.mode && loaded.timeControl == saved.timeControl
          && loaded.playerColor == saved.playerColor && loaded.engineLevel == saved.engineLevel
          && loaded.whiteTimeMs == saved.whiteTimeMs && loaded.blackTimeMs == saved.blackTimeMs
          && loaded.initialFen == saved.initialFen && loaded.moves == saved.moves,
          "saved game round-trip");

    platform::Statistics stats;
    std::string pgnPath;
    check(storage.exportPgn("[Result \"*\"]\n\n*\n", stats, &pgnPath), "export PGN");
    check(stats.exportedGames == 1 && !pgnPath.empty(), "PGN number updated");
    check(storage.clearSavedGame() && !storage.hasSavedGame(), "clear saved game");

    if (failures) return EXIT_FAILURE;
    std::cout << "All Chess3DS storage tests passed.\n";
    return EXIT_SUCCESS;
}
