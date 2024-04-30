#include <cassert>
#include "TranspositionTable.h"
#include "defines.h"
#include <iostream>
#include <cstring>
#include "Move.h"


// Code inspiré de Sungorus
// Idées provenant de Bruce Moreland


#include <cassert>
#include "TranspositionTable.h"
#include "defines.h"


//========================================================
//! \brief  Constructeur avec argument
//--------------------------------------------------------
TranspositionTable::TranspositionTable(int MB) :
    pawn_size(PAWN_HASH_SIZE*1024),
    pawn_mask(PAWN_HASH_SIZE*1024 - 1)
{
#if defined DEBUG_LOG
    char message[100];
    sprintf(message, "TranspositionTable::constructeur MB : %d ", MB);
    printlog(message);
#endif

    tt_entries = nullptr;
    tt_size    = 0;
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

    int bytes    = mbsize * 1024 * 1024;
    int nbr_elem = bytes / sizeof(HashEntry);

    if (tt_entries != nullptr)
    {
        delete [] tt_entries;
        tt_entries = nullptr;
        tt_size = 0;
    }

    // size must be a power of 2!
    tt_size = 1;

    while (tt_size <= nbr_elem)
        tt_size *= 2;
    tt_size /= 2;

    assert(tt_size!=0 && (tt_size&(tt_size-1))==0); // power of 2

    tt_buckets = 4;
    tt_mask    = tt_size - tt_buckets;
    tt_entries = new HashEntry[tt_size];

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
    tt_size = 0;
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

    std::memset(tt_entries, 0, sizeof(HashEntry) * tt_size);
    std::memset(pawn_entries, 0, sizeof(PawnHashEntry) * pawn_size);
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
void TranspositionTable::store(U64 hash, MOVE move, Score score, Score eval, int bound, int depth, int ply)
{
    assert(abs(score) <= MATE);
    assert(abs(eval) <= MATE || eval == NOSCORE);
    assert(0 <= depth && depth < MAX_PLY);
    assert(bound == BOUND_LOWER || bound == BOUND_UPPER || bound == BOUND_EXACT);


    // extract the 32-bit key from the 64-bit zobrist hash
    U32 hash32 = hash >> 32;

    HashEntry *entry   = tt_entries + (hash & tt_mask);
    HashEntry *replace = nullptr;
    int oldest = -1;
    int age;

    for (int i = 0; i < tt_buckets; i++)
    {
        assert (tt_date >= entry->date);

        // Found a matching hash or an unused entry
        if (entry->hash32 == hash32 || entry->bound == 0)
        {
            replace = entry;
            break;
        }

        // Take the first entry as a starting point
        if (i == 0)
        {
            replace = entry;
            continue;
        }

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
        age = (tt_date - entry->date) * KMULT + 255 - entry->depth;             // version simplifiée

        // if (   replace->depth - (tt_date - replace->date) * KMULT            // version comme Ethereal, Berserk
        //     >= entry->depth   - (tt_date - entry->date) * KMULT )
        if (age > oldest)
        {
            oldest  = age;
            replace = entry;
        }
        entry++;
    }

    // Don't overwrite an entry from the same position, unless we have
    // an exact bound or depth that is nearly as good as the old one
    if (    bound != BOUND_EXACT
        &&  hash32 == replace->hash32
        &&  depth < replace->depth - 3)
        return;

    replace->hash32 = hash32;
    replace->move   = (MOVE)move;
    replace->score  = (I16)ScoreToTT(score, ply);
    replace->eval   = (I16)eval;
    replace->depth  = (U08)depth;
    replace->date   = (U08)tt_date;
    replace->bound  = (U08)bound;

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
bool TranspositionTable::probe(U64 hash, int ply, MOVE& move, Score& score, Score& eval, int &bound, int& depth)
{
    move  = Move::MOVE_NONE;
    score = NOSCORE;
    eval  = NOSCORE;
    bound = BOUND_NONE;

    // extract the 32-bit key from the 64-bit zobrist hash
    U32 hash32 = hash >> 32;

    HashEntry* entry = tt_entries + (hash & tt_mask);

    for (int i = 0; i < tt_buckets; i++)
    {
        assert (tt_date >= entry->date);
        if (entry->hash32 == hash32)
        {
            entry->date = tt_date;

            move  = entry->move;
            bound = entry->bound;
            depth = entry->depth;
            score = ScoreFromTT(entry->score, ply);
            eval  = entry->eval;

            return true;
        }
        entry++;
    }

    return false;
}

//===============================================================
//! \brief  Recherche d'une donnée dans la table des pions
//! \param[in]  hash    code hash des pions
//! \param{out] score   score de cette position
//! \param[out] passed  bitboard des pions passés
//---------------------------------------------------------------
bool TranspositionTable::probe_pawn_table(U64 hash, Score &eval, Bitboard& passed)
{
    PawnHashEntry* entry = pawn_entries + (hash & pawn_mask);

    if (entry->hash == hash)
    {
        eval   = entry->eval;
        passed = entry->passed;
        return true;
    }

    return false;
}

//=============================================================
//! \brief Stocke une évaluation dans la table des pions
//!
//! \param[in]  hash    hash des pions
//! \param[in]  score   évaluation
//! \param[in]  passed  bitboard des pions passés
//-------------------------------------------------------------
void TranspositionTable::store_pawn_table(U64 hash, Score eval, Bitboard passed)
{
    PawnHashEntry* entry = pawn_entries + (hash & pawn_mask);

    entry->hash   = hash;
    entry->eval   = eval;
    entry->passed = passed;
}


void TranspositionTable::stats()
{
    std::cout << "TT size = " << tt_size << std::endl;
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
int TranspositionTable::hash_full()
{
    int used = 0;

    for (int i = 0; i < tt_buckets*1000; i++)
        if (   tt_entries[i].move != Move::MOVE_NONE
            && tt_entries[i].date == tt_date)
            used++;

    return used/tt_buckets;
}

