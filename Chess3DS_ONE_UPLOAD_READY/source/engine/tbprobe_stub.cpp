// Chess3DS deliberately disables Syzygy tablebases. Their memory-mapped files
// are unsuitable for the Old 3DS, while Stockfish's normal search remains more
// than strong enough. These stubs satisfy Stockfish 11's tablebase interface.

#include "syzygy/tbprobe.h"

namespace Tablebases {

int MaxCardinality = 0;

void init(const std::string&) { MaxCardinality = 0; }

WDLScore probe_wdl(Position&, ProbeState* result) {
    if (result) *result = FAIL;
    return WDLScoreNone;
}

int probe_dtz(Position&, ProbeState* result) {
    if (result) *result = FAIL;
    return 0;
}

bool root_probe(Position&, Search::RootMoves&) { return false; }
bool root_probe_wdl(Position&, Search::RootMoves&) { return false; }

} // namespace Tablebases
