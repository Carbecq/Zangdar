#include <cassert>
#include <cmath>
#include "defines.h"
#include <iostream>
#include <cstring>
#include "Move.h"
#include "TranspositionTable.h"


// Code inspiré de Sungorus
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
    tt_date    = 0;

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

    // if (nbr_elem == clusterCount)
    //     return;

    // size must be a power of 2!
    // clusterCount = 1;

    // while (clusterCount <= nbr_elem)
    //     clusterCount *= 2;
    // clusterCount /= 2;

    // printf("cc = %d \n", clusterCount);

    // assert(clusterCount!=0 && (clusterCount&(clusterCount-1))==0); // power of 2

    // tt_buckets = 4;


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

    tt_date = 0;

    std::memset(tt_entries, 0, sizeof(HashCluster) * (tt_mask+1));
}

//========================================================
//! \brief  Mise à jour de l'age
//! tt_date = [0...255]
//--------------------------------------------------------
void TranspositionTable::update_age(void)
{
    tt_date++;
}

//========================================================
//! \brief  Ecriture dans la hashtable d'une nouvelle donnée
//--------------------------------------------------------
void TranspositionTable::store(U64 key, MOVE move, int score, int bound, int depth, int ply)
{
    assert(abs(score) <= MATE);
    assert(0 <= depth && depth < MAX_PLY);
    assert(bound == BOUND_LOWER || bound == BOUND_UPPER || bound == BOUND_EXACT);

    // extract the 32-bit key from the 64-bit zobrist hash
    U32 key32 = (key >> 32);
        //  static_cast<U16>(key);
        // key & 0xFFFF;
        // static_cast<U16>(key);  //TODO autre moyen ??

    HashCluster& cluster = tt_entries[index(key)];

    // tt_entries + (key & tt_mask);

    //     tt_mask    = tt_size - tt_buckets;

    //  U64 i1 = key & (tt_size-1);
    //  U64 i2 = key % tt_size;
    //  U64 i3 = mul_hi(key, tt_size);
    // if (i2 != i3 )
    //  {
    //      std::cout << "ereur index " << i1 << "  " << "  " << i2 << "  " << i3 << std::endl;
    //  }

    // HashEntry *replace = nullptr;
    HashEntry *entry = nullptr;
    int oldest = -1;
    int age;

    entry = &(cluster.entries[0]);

    for (auto & elem : cluster.entries)
    {
        // entry = &c;

        assert (tt_date >= elem.date);

        // Found a matching hash or an unused entry
        if (elem.key32 == key32 || elem.bound == Move::FLAG_NONE)
        {
            entry = &elem;
            break;
        }

        // Take the first entry as a starting point
        // if (i == 0)
        // {
        //     replace = entry;
        //     continue;
        // }

        /* 1) tt_date >= entry->date
         * 2) le "+255" sert à compenser "-entry->depth",
         *    lorsque tt_date=entry->date, "(tt_date - entry->date) * 256" vaut 0
         * 3) KMULT définit l'équivalence entre date et depth.
         *    Pour une différence de 1 pour la date, il faut KMULT différence
         *    pour la profondeur.
         *    Dans Sungorus, KMULT=256, donc la profondeur ne peut pas entrer
         *    en concurence avec la date.
         * 4) age va de 255 --> 255*(KMULT+1)
         */

        // age = ((tt_date - entry->date) & 255) * 256 + 255 - entry->depth;    // version Sungorus
        age = (tt_date - elem.date) * KMULT + 255 - elem.depth;             // version simplifiée

        // if (   replace->depth - (tt_date - replace->date) * KMULT            // version comme Ethereal, Berserk
        //     >= entry->depth   - (tt_date - entry->date) * KMULT )
        if (age > oldest)
        {
            oldest  = age;
            entry = &elem;
        }
        // entry++;
    }

    assert(entry != nullptr);

    // Don't overwrite an entry from the same position, unless we have
    // an exact bound or depth that is nearly as good as the old one
    if (    bound != BOUND_EXACT
        &&  key32 == entry->key32
        &&  depth < entry->depth - 3)
        return;

    entry->key32  = key32;
    entry->move   = (MOVE)move;
    entry->score  = static_cast<I16>(ScoreToTT(score, ply));
    entry->depth  = static_cast<I08>(depth);
    entry->date   = static_cast<I08>(tt_date);
    entry->bound  = static_cast<I08>(bound);
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
bool TranspositionTable::probe(U64 key, int ply, MOVE& move, int &score, int &bound, int& depth)
{
    move  = Move::MOVE_NONE;
    score = NOSCORE;
    bound = BOUND_NONE;

    // extract the 32-bit key from the 64-bit zobrist hash
    // U16 key16 = static_cast<U16>(key);
    U32 key32 = (key >> 32);
        // key & 0xFFFF;
    // static_cast<U16>(key);  //TODO autre moyen ??

    HashCluster& cluster = tt_entries[index(key)];

    for (HashEntry& entry : cluster.entries)
    {
        assert (tt_date >= entry.date);

        if (entry.key32 == key32)
        {
            entry.date = tt_date;

            move  = entry.move;
            bound = entry.bound;
            depth = entry.depth;
            score = ScoreFromTT(entry.score, ply);

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
                && tt_entries[i].entries[j].date == tt_date
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
