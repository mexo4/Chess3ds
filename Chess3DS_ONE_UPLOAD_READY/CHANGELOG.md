# Changelog

## v1.1.1
- Old 3DS safe-boot: smaller main stack and Citro2D object pool.
- App state moved to heap with allocation failure handling.
- Stockfish implementation allocated only when CPU mode is selected.
- Stockfish worker stack reduced from 2 MiB to 1 MiB for short searches.
- Old 3DS Stockfish hash reduced to 1 MiB and 3DS release assertions disabled.
- Saved-game availability cached instead of hitting SD every menu frame.
- Smaller Citro2D text buffer.

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
