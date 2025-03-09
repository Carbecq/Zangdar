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

#include <cassert>
#include "defines.h"

struct HashEntry {
    static constexpr U32 AgeBits = 5;
    static constexpr U32 AgeCycle = 1 << AgeBits;       // 32
    static constexpr U32 AgeMask = AgeCycle - 1;        // 31

    U32   key32;    // 32 bits
    MOVE  move;     // 32 bits  (seuls 24 bits sont utilisés)
    I16   score;    // 16 bits
    I16   eval;     // 16 bits
    U08   depth;    //  8 bits
    U08   agePvBound;    //  8 bits  (5 bits age : 1 bit pv ; 2 bits bound)
                         //           0-31

    [[nodiscard]] inline auto age() const
    {
        return static_cast<U32>(agePvBound >> 3);
    }

    [[nodiscard]] inline auto pv() const
    {
        return (static_cast<U32>(agePvBound >> 2) & 1) != 0;
    }

    [[nodiscard]] inline auto bound() const
    {
        return static_cast<U08>(agePvBound & 0x3);
    }

    inline auto setAgePvBound(U32 age, bool pv, U08 bound)
    {
        assert(age < (1 << AgeBits));
        agePvBound = (age << 3) | (static_cast<U32>(pv) << 2) | static_cast<U32>(bound);
    }

    /* hash32 :
     *
     * total = 112 bits = 14 octets
     * le compilateur ajoute un padding de 16 bits pour avoir un alignement mémoire de 32 bits : 128 = 32*4
     *  donc sizeof(HashEntry) = 16
     *
     *

Nombre de clusters  : 2097152
Taille d'un cluster : 64 octets
Entrées par cluster : 4
Taille d'une entrée : 16 octets
Total entrées       : 8388608

 */


/* key16 :
 *
 * 88 bits = 11 octets --> alignement sur 12 octets (12*8 = 96 = 3*32)
 * le compilateur ajoute 8 octets
 *
Nombre de clusters  : 2796202
Taille d'un cluster : 48 octets
Entrées par cluster : 4
Taille d'une entrée : 12 octets
Total entrées       : 11.184.808
Taille totale       : 134217696  134217696 (128) Mo
 */

};

static constexpr Usize CLUSTER_SIZE = 4;

struct HashCluster {
    std::array<HashEntry, CLUSTER_SIZE> entries{};
};



//----------------------------------------------------------


class TranspositionTable
{
private:
    static constexpr U64 ONE = 1ULL;

    Usize          nbr_cluster{};
    U32            tt_age{};
    HashCluster*   tt_entries{};


    // https://lemire.me/blog/2016/06/27/a-fast-alternative-to-the-modulo-reduction/
    // inline U64 mul_hi(const U64 a, const U64 b) {
    //      return (static_cast<U128>(a) * static_cast<U128>(b)) >> 64;
    // }

    // inline HashEntry* GetEntry(U64 key) {
    //      return &tt_entries[ mul_hi(key, tt_size) ];
    // }

    inline U64 index(U64 key) {
        // this emits a single mul on both x64 and arm64
        return static_cast<U64>((static_cast<U128>(key) * static_cast<U128>(nbr_cluster)) >> 64);
        // return key & tt_mask;
    }

public:
    TranspositionTable() : TranspositionTable(HASH_SIZE) {}
    TranspositionTable(int MB);
    ~TranspositionTable();

    void init_size(int mbsize);
    void set_hash_size(int mbsize);
    int  get_hash_size(void) const { return nbr_cluster; }
    void info();

    void clear(void);
    inline auto update_age()
    {
        tt_age = (tt_age + 1) % (1 << HashEntry::AgeBits);
    }

    void store(U64 hash, MOVE move, int score, int eval, int bound, int depth, int ply, bool pv);
    bool probe(U64 hash, int ply, MOVE &code, int &score, int &eval, int &bound, int &depth, bool &pv);
    void stats();
    int  hash_full() const;

    //! \brief Store terminal scores as distance from the current position to mate/TB
    int ScoreToTT (const int score, const int ply)
    {
        return   score == VALUE_NONE ? VALUE_NONE
               : score >=  TBWIN_IN_X ? score + ply
               : score <= -TBWIN_IN_X ? score - ply
               : score;
    }

    //! \brief Add the distance from root to terminal scores get the total distance to mate/TB
    int ScoreFromTT (const int score, const int ply)
    {
        return   score == VALUE_NONE ? VALUE_NONE
               : score >=  TBWIN_IN_X ? score - ply
               : score <= -TBWIN_IN_X ? score + ply
               : score;
    }

    //------------------------------------------------
    /// prefetch() preloads the given address in L1/L2 cache. This is a non-blocking
    /// function that doesn't stall the CPU waiting for data to be loaded from memory,
    /// which can be quite slow.
    inline void prefetch(const U64 key)
    {
        __builtin_prefetch(&tt_entries[index(key)]);
    }

};

extern TranspositionTable transpositionTable;


#endif // TRANSPOSITIONTABLE_H
