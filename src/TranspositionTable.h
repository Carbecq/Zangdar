#ifndef TRANSPOSITIONTABLE_H
#define TRANSPOSITIONTABLE_H

//  Classe définissant la table de transposition
//  Voir le site de Moreland
//  Code provenant du code Sungorus (merci à lui)

// https://www.chessprogramming.org/Node_Types

static constexpr int BOUND_NONE  = 0;   //
static constexpr int BOUND_UPPER = 1;   // All-nodes (Knuth's Type 3) : score <= alpha        ; appelé aussi FAIL-LOW
static constexpr int BOUND_LOWER = 2;   // Cut-nodes (Knuth's Type 2) : score >= beta         ; appelé aussi FAIL-HIGH
static constexpr int BOUND_EXACT = 3;   // PV-node   (Knuth's Type 1) : alpha < score < beta  ; the value returned is EXACT (not a BOUND)

class TranspositionTable;

#include "defines.h"

struct HashEntry {
    U32   hash32;   // 32 bits
    MOVE  move;     // 32 bits  (seuls 24 bits sont utilisés)
    I16   score;    // 16 bits
    I16   eval;     // 16 bits
    U08   depth;    //  8 bits
    U08   date;     //  8 bits
    U08   bound;    //  8 bits  (2 bits nécessaires seulement)
};

//----------------------------------------------------------
// Code provenant de Ethereal

struct PawnHashEntry {
    U64      hash;      // 64 bits
    Bitboard passed;    // 64 bits
    Score    eval;      // 32 bits
};

//----------------------------------------------------------

/* Remarques
 *  1) le "hash_code" est mis dans "move" comme score
 *  2) dans Koivisto, la date (ou age) est mise dans move (8 bits) comme score
 */

// 134 217 728 = 2 ^ 27 = 128 Mo
// 0000 1000 0000 0000 0000 0000 0000 0000
//
//  8388608 entries of 16 for a total of 134217728
//
// tt_size = 8388608 = 100000000000000000000000 = 2 ^ 23
//
// tt_mask = tt_size - 4 = 0111 1111 1111 1111 1111 1100    : index multiple de 4
// tt_mask = tt_size - 2 = 0111 1111 1111 1111 1111 1110
// tt_mask = tt_size - 1 = 0111 1111 1111 1111 1111 1111

class TranspositionTable
{
private:
    int         tt_size;
    int         tt_mask;
    U08         tt_date;
    int         tt_buckets;
    HashEntry*  tt_entries = nullptr;

    // Table de transposition pour les pions
    PawnHashEntry   pawn_entries[PAWN_HASH_SIZE*1024];
    int             pawn_size;
    int             pawn_mask;

    static constexpr int   KMULT = 256;

public:
    TranspositionTable(int MB);
    ~TranspositionTable();

    void init_size(int mbsize);
    void set_hash_size(int mbsize);
    int  get_hash_size(void) const { return tt_size; }

    void clear(void);
    void update_age(void);
    void store(U64 hash, MOVE move, Score score, Score eval, int bound, int depth, int ply);
    bool probe(U64 hash, int ply, MOVE &code, Score &score, Score &eval, int &bound, int &depth);
    void stats();
    int  hash_full();

    //! \brief Store terminal scores as distance from the current position to mate/TB
    int ScoreToTT (const int score, const int ply)
    {
        return score >=  TBWIN_IN_X ? score + ply
               : score <= -TBWIN_IN_X ? score - ply
                                        : score;
    }

    //! \brief Add the distance from root to terminal scores get the total distance to mate/TB
    int ScoreFromTT (const int score, const int ply)
    {
        return score >=  TBWIN_IN_X ? score - ply
               : score <= -TBWIN_IN_X ? score + ply
                                        : score;
    }

    //------------------------------------------------

    bool probe_pawn_table(U64 hash, Score &eval, Bitboard &passed);
    void store_pawn_table(U64 hash, Score eval, Bitboard passed);

    void prefetch(const U64 hash);
};

extern TranspositionTable transpositionTable;



#endif // TRANSPOSITIONTABLE_H
