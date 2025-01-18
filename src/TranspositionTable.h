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
    U32   key32;    // 32 bits
    MOVE  move;     // 32 bits  (seuls 24 bits sont utilisés)
    I16   score;    // 16 bits
    U08   depth;    //  8 bits
    U08   date;     //  8 bits
    U08   bound;    //  8 bits  (2 bits nécessaires seulement)

    /* hash32 :
     *
     * total = 104 bits = 13 octets
     * le compilateur ajoute un padding de 24 bits pour avoir un alignement mémoire de 32 bits : 128 = 32*4
     *  donc sizeof(HashEntry) = 16

avec la date :
Nombre de clusters  : 2097152
Taille d'un cluster : 64 octets
Entrées par cluster : 4
Taille d'une entrée : 16 octets
Total entrées       : 8.388.608
Taille totale       : 134217728  134217728 (128) Mo

sans la date : 96 bits
Nombre de clusters  : 2097152
Taille d'un cluster : 48 octets
Entrées par cluster : 4
Taille d'une entrée : 12 octets
Total entrées       : 8388608
Taille totale       : 100663296  100663296 (96) Mo

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
    std::array<HashEntry, CLUSTER_SIZE> entries;
};

/*

hash64  = 8020241708cd0710 ;
shift32 = 80202417 ;
cast32  =          8cd0710 ;
autre   = 88ed2307


*/


//----------------------------------------------------------

/* Remarques
 *  > dans Koivisto, la date (ou age) est mise dans move (8 bits) comme score
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
    static constexpr int KMULT = 256;
    static constexpr U64 ONE = 1ULL;

    // int         tt_size;
    U64            tt_mask;
    U08            tt_date;
    // int         tt_buckets;
    // HashEntry*  tt_entries = nullptr;
    // Usize clusterCount = 0;
    HashCluster* tt_entries = nullptr;




    // https://lemire.me/blog/2016/06/27/a-fast-alternative-to-the-modulo-reduction/
    // inline U64 mul_hi(const U64 a, const U64 b) {
    //      return (static_cast<U128>(a) * static_cast<U128>(b)) >> 64;
    // }

    // inline HashEntry* GetEntry(U64 key) {
    //      return &tt_entries[ mul_hi(key, tt_size) ];
    // }

    inline U64 index(U64 key) {
        // return static_cast<U64>((static_cast<U128>(key) * static_cast<U128>(clusterCount)) >> 64);
        return key & tt_mask;
    }


public:
    TranspositionTable() : TranspositionTable(HASH_SIZE) {}
    TranspositionTable(int MB);
    ~TranspositionTable();

    void init_size(int mbsize);
    void set_hash_size(int mbsize);
    int  get_hash_size(void) const { return tt_mask+1; }
    void info();

    void clear(void);
    void update_age(void);
    void store(U64 hash, MOVE move, int score, int bound, int depth, int ply);
    bool probe(U64 hash, int ply, MOVE &code, int &score, int &bound, int &depth);
    void stats();
    int  hash_full() const;

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

    void prefetch(const U64 hash);
};

extern TranspositionTable transpositionTable;


#endif // TRANSPOSITIONTABLE_H
