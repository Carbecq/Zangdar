#include <cassert>
#include <cmath>
#include <climits>
#include "defines.h"
#include <iostream>
#include <cstring>
#include "Move.h"
#include "TranspositionTable.h"


// Code inspiré de Sungorus, puis Stormphrax
// Idées provenant de Bruce Moreland


#include <cassert>
#include "TranspositionTable.h"
#include "defines.h"


//========================================================
//! \brief  Constructeur avec argument
//--------------------------------------------------------
TranspositionTable::TranspositionTable(int MB)
{
#if defined DEBUG_LOG
    char message[100];
    sprintf(message, "TranspositionTable::constructeur MB : %d ", MB);
    printlog(message);
#endif

    tt_entries = nullptr;
    tt_age     = 0;

    init_size(MB);
}

//========================================================
//! \brief  Destructeur
//--------------------------------------------------------
TranspositionTable::~TranspositionTable()
{
    delete [] tt_entries;
}

//========================================================
//! \brief  Initialisation de la table
//--------------------------------------------------------
void TranspositionTable::init_size(int mbsize)
{
#if defined DEBUG_LOG
    char message[1000];
    sprintf(message, "TranspositionTable::init_size : %d ", mbsize);
    printlog(message);
#endif

    Usize size  = (mbsize * 1024 * 1024) / sizeof(HashCluster);

    // L'index est calculé par : key & tt_mask
    // if faut que le nombre de clusters soit un multiple de 2
    int keySize = static_cast<int>(std::log2(size));    // puissance de 2
    tt_mask = (ONE << keySize) - ONE;                   // tt_mask = nbr_clusters - 1

    if (tt_entries != nullptr)
    {
        delete [] tt_entries;
        tt_entries = nullptr;
    }

    tt_entries = new HashCluster[(ONE << keySize)];

    clear();


#if defined DEBUG_LOG
    sprintf(message, "TranspositionTable init complete with %d entries of %lu bytes for a total of %lu bytes (%lu MB) \n",
            tt_size, sizeof(HashEntry), tt_size*sizeof(HashEntry), tt_size*sizeof(HashEntry)/1024/1024);
    printlog(message);
#endif
}

//========================================================
//! \brief  Allocation uniquement de la Hash Table
//--------------------------------------------------------
void TranspositionTable::set_hash_size(int mbsize)
{
#if defined DEBUG_LOG
    char message[100];
    sprintf(message, "TranspositionTable::set_hash_size : %d ", mbsize);
    printlog(message);
#endif

    delete [] tt_entries;
    tt_entries = nullptr;
    init_size(mbsize);
}

//========================================================
//! \brief  Remise à zéro de la table de transposition
//--------------------------------------------------------
void TranspositionTable::clear(void)
{
#if defined DEBUG_LOG
    char message[100];
    sprintf(message, "TranspositionTable::clear");
    printlog(message);
#endif

    tt_age = 0;

    std::memset(tt_entries, 0, sizeof(HashCluster) * (tt_mask+1));
}

//========================================================
//! \brief  Ecriture dans la hashtable d'une nouvelle donnée
//--------------------------------------------------------
void TranspositionTable::store(U64 key, MOVE move, int score, int eval, int bound, int depth, int ply, bool pv)
{
    assert(0 <= depth && depth <= MAX_PLY);

    // extract the 32-bit key from the 64-bit zobrist hash
    // U32 key32 = (key >> 32);
    U32 key32 = static_cast<U32>(key);
    //  static_cast<U16>(key);
    // key & 0xFFFF;

    /*
    hash64  = 8020241708cd0710 ;
    shift32 = 80202417 ;
    cast32  =          8cd0710 ;
    autre   = 88ed2307
    */

    const auto entryValue = [this](const auto &entry)
    {
        const I32 relativeAge = (HashEntry::AgeCycle + tt_age - entry.age()) & HashEntry::AgeMask;
        return entry.depth - relativeAge * 2;
    };

    HashCluster& cluster = tt_entries[index(key)];
    HashEntry *replace = nullptr;
    auto minValue = std::numeric_limits<I32>::max();

    for (auto & elem : cluster.entries)
    {
        assert (tt_age >= elem.age());

        // always take an empty entry, or one from the same position
        // Question : But there are some entries with TtFlag=None , when it stores rawStaticEval.
        // >>>"empty" just means no search score in this case (I think), in which one with a search score is better
        // >>> in other words caching a static eval score and not a search score, to avoid calling eval
        if (elem.key32 == key32 || elem.bound() == BOUND_NONE)
        {
            replace = &elem;
            break;
        }

        // otherwise, take the lowest-weighted entry by depth and age
        const auto value = entryValue(elem);

        if (value < minValue)
        {
            replace  = &elem;
            minValue = value;
        }
    }

    assert(replace != nullptr);

    // Roughly the SF replacement scheme
    // Don't overwrite an entry from the same position, unless we have
    // an exact bound or depth that is nearly as good as the old one
    if (!(bound == BOUND_EXACT
          || key32 != replace->key32
          || replace->age() != tt_age
          || depth + 4 + pv * 2 > replace->depth))
        return;

    // only overwrite the move if new move is not a null move or the entry is from a different position
    // idea from Sirius, Stockfish and Ethereal
    // Preserve any existing move for the same position
    if (move || replace->key32 != key32)
        replace->move = move;

    replace->key32  = key32;
    replace->score  = static_cast<I16>(ScoreToTT(score, ply));
    replace->eval   = static_cast<I16>(eval);
    replace->depth  = static_cast<I08>(depth);
    replace->setAgePvBound(tt_age, pv, bound);
}

//========================================================
//! \brief  Recherche d'une donnée
//! \param[in]  hash    code hash de la position recherchée
//! \param{out] code    coup trouvé
//! \param{out] score   score de ce coup
//! \param{in]  alpha
//! \param{in]  beta
//! \param{in]  depth
//! \param{in]  ply
//--------------------------------------------------------
bool TranspositionTable::probe(U64 key, int ply, MOVE& move, int &score, int& eval, int &bound, int& depth, bool& pv)
{
    // extract the 32-bit key from the 64-bit zobrist hash
    const U32 key32 = static_cast<U32>(key);

    const HashCluster& cluster = tt_entries[index(key)];
    for (const HashEntry& entry : cluster.entries)
    {
        assert (tt_age >= entry.age());

        if (entry.key32 == key32)
        {
            move  = entry.move;
            bound = entry.bound();
            depth = entry.depth;
            score = ScoreFromTT(entry.score, ply);
            eval  = entry.eval;
            pv    = entry.pv();

            return true;
        }
    }

    return false;
}

void TranspositionTable::stats()
{
    // std::cout << "TT size = " << clusterCount << std::endl;
    //    for (int i=0; i<tt_buckets; i++)
    //    {
    //        std::cout << "store[" << i+1 << "] = " << nbr_store[i] << std::endl;
    //    }
    //    for (int i=0; i<tt_buckets; i++)
    //    {
    //        std::cout << "probe[" << i+1 << "] = " << nbr_probe[i] << std::endl;
    //    }

    //    std::cout << "PawnCacheSize = " << PAWN_CACHE_SIZE << std::endl;
    //    std::cout << "store  = " << pcachestore << std::endl;
    //    std::cout << "hit    = " << pcachehit << std::endl;
    //    std::cout << "no hit = " << pcachenohit << std::endl;
    //    std::cout << "usage  = " << (double)pcachenohit / (double)pcachehit << std::endl;
}

//=======================================================================
//! \brief Estimation de l'utilisation de la table de transposition
//! On regarde combien d'entrées contiennent une valeur récente.
//! La valeur retournée va de 0 à 1000 (1 = 0.1 %)
//-----------------------------------------------------------------------
int TranspositionTable::hash_full() const
{
    int used = 0;

    for (int i = 0; i < 1000; i++)
    {
        for (Usize j=0; j<CLUSTER_SIZE; j++)
        {
            if (   tt_entries[i].entries[j].move != Move::MOVE_NONE
                   && tt_entries[i].entries[j].age() == tt_age
                   )
                used++;
        }
    }

    return used/CLUSTER_SIZE;
}

/// prefetch() preloads the given address in L1/L2 cache. This is a non-blocking
/// function that doesn't stall the CPU waiting for data to be loaded from memory,
/// which can be quite slow.
void TranspositionTable::prefetch(const U64 key)
{
    // __builtin_prefetch(&tt_entries[hash & tt_mask]);
    __builtin_prefetch(&tt_entries[index(key)]);

}

void TranspositionTable::info()
{
    std::cout << "Nombre de clusters  : " << (tt_mask+1) << std::endl
              << "Taille d'un cluster : " << sizeof(HashCluster) << " octets" << std::endl
              << "Entrées par cluster : " << CLUSTER_SIZE << std::endl
              << "Taille d'une entrée : " << sizeof(HashEntry) << " octets" << std::endl
              << "Total entrées       : " << (tt_mask+1) * CLUSTER_SIZE << std::endl
              << "Taille totale       : " << (tt_mask+1)*sizeof(HashCluster) << "  " << (tt_mask+1) * CLUSTER_SIZE*sizeof(HashEntry)
              << " (" << (tt_mask+1)*sizeof(HashCluster)/1024.0/1024.0 << ") Mo"
              << std::endl;

}
