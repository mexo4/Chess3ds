# Third-party software

## Stockfish 11

Chess3DS contains Stockfish 11, taken from the official Stockfish repository:

- upstream: https://github.com/official-stockfish/Stockfish
- tag: `sf_11`
- commit: `c3483fa9a7d7c0ffa9fcc32b467ca844cfb63790`
- license: GNU General Public License v3 or later

The complete upstream source is stored in `vendor/stockfish/`.

Chess3DS changes are intentionally small and visible in the same repository:

- the default transposition table is reduced to 4 MiB for Old 3DS;
- the search thread receives a 512 KiB stack on 3DS;
- UCI console output is suppressed in the embedded build;
- one `clamp` call is qualified for modern C++17 compilers;
- Syzygy probing is replaced by no-op stubs because large memory-mapped
  tablebase files are not suitable for this target;
- `StockfishEngine.cpp` supplies the in-process asynchronous application bridge.

No neural-network weights are included: Stockfish 11 is the final pre-NNUE
release and uses its classical evaluation.
