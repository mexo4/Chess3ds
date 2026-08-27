# Changelog

## 1.0.1

- Hardened startup after an immediate crash was observed on physical Old 3DS hardware.
- Increased the libctru main stack from its small default to 1 MiB.
- Load Stockfish and NDSP audio on demand instead of before the first frame.
- Use a 2 MiB Stockfish worker stack and handle thread/allocation failures.
- Removed temporary helper threads used for hash clearing and result waiting.
- Added `/3ds/Chess3DS/last_stage.txt` crash-stage diagnostics.

## 1.0.0

- Initial full Chess3DS release.
- Complete legal move generator and game adjudication.
- Embedded Stockfish 11 with eight difficulty levels.
- CPU and local two-player modes with four clock presets.
- Touch and button controls, animations, sounds and four themes.
- Autosave, PGN export, statistics and five mate puzzles.
- Polish and English user interface.
