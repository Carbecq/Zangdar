#ifndef CUCKOO_H
#define CUCKOO_H

#include "defines.h"

// Marcel van Kervinck's cuckoo algorithm for fast detection of "upcoming repetition"
// situations. Description of the algorithm in the following paper:
// http://web.archive.org/web/20201107002606/https://marcelk.net/2013-04-06/paper/upcoming-rep-v2.pdf

/*
    cuckoo tables are mainly used to return a draw score if you're currently worse.
    if alpha > draw score is the best you have, then you should still continue the search
    even if a repetition is possible to claim. meaning, just because a possible repetition
    is upcoming, doesnt mean you can say the position is a draw.
    whereas with threefold repetition, that is an actual rule of chess that adjudicates the game.
    but, the addition of cuckoo tables is somewhere between 5-10 elo in most engines.
    if that's what you meant to ask

*/

namespace Cuckoo {

// First and second hash functions for indexing the cuckoo tables
inline U64 h1(U64 key) { return key & 0x1FFF; }
inline U64 h2(U64 key) { return (key >> 16) & 0x1FFF; }

// Cuckoo tables with Zobrist hashes of valid reversible moves, and the moves themselves
extern std::array<U64, 8192>  keys;
extern std::array<MOVE, 8192> moves;


void init();
}

#endif // CUCKOO_H
