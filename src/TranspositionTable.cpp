#include <algorithm>
#include <cassert>
#include <cmath>
#include <climits>
#include "defines.h"
#include <iostream>
#include <cstring>
#include "HugePages.h"
#include "Move.h"
#include "Board.h"
#include "TranspositionTable.h"


// Code inspiré de Sungorus, puis Stormphrax
// Idées provenant de Bruce Moreland


#include <cassert>
#include "TranspositionTable.h"
#include "defines.h"


//========================================================
//  Coup compacté sur 16 bits
//
//  bits  0-5  : case de départ
//  bits  6-11 : case d'arrivée
//  bits 12-14 : 0 : 000    : normal,
//               1 : 001    : double poussée
//               2 : 010    : prise en passant
//               3 : 011    : roque
//               4 : 100    : promotion en cavalier
//               5 : 101    :              fou
//               6 : 110    :              tour
//               7 : 111    :              dame
//
//  La pièce jouée et la pièce prise se relisent sur l'échiquier.
//  0 représente MOVE_NONE (a1a1 n'est pas un coup).
//--------------------------------------------------------
static U16 move_to_move16(MOVE move)
{
    // Une promotion ne porte pas de drapeau, et les drapeaux sont exclusifs :
    //   promotion (type 2..5) -> 4..7 ; drapeau 1, 2, 4 -> 1, 2, 3 ; MOVE_NONE -> 0
    const U32 pt   = Move::promoted_type(move);
    const U32 f    = Move::flags(move) >> Move::SHIFT_FLAGS;

    assert(f == 0 || f == 1 || f == 2 || f == 4);   // un seul drapeau
    assert(pt == 0 || f == 0);                      // une promotion n'en porte pas

    const U32 code = pt ? pt + 2 : f - (f >> 2);

    return static_cast<U16>((move & Move::MOVE_FROMDEST_MASK) | (code << 12));
}

//========================================================
//! \brief  Reconstruit un coup complet à partir de sa forme 16 bits
//--------------------------------------------------------
MOVE TranspositionTable::move16_to_move(const Board& board, U16 move16) const
{
    static constexpr U32 Flags[8] = { Move::FLAG_NONE, Move::FLAG_DOUBLE_MASK, Move::FLAG_ENPASSANT_MASK,
                                      Move::FLAG_CASTLE_MASK, 0, 0, 0, 0 };
    if (move16 == 0)
        return Move::MOVE_NONE;

    const Color us    = board.turn();
    const Piece piece = board.piece_at(move16 & 63);

    if (piece == Piece::PIECE_NONE || Move::color(piece) != us)
        return Move::MOVE_NONE;

    const U32   code     = move16 >> 12;
    const Piece captured = (code == 2) ? Move::make_piece(~us, PieceType::PAWN)
                                       : board.piece_at((move16 >> 6) & 63);
    const Piece promo    = (code >= 4) ? Move::make_piece(us, static_cast<PieceType>(code - 2))
                                       : Piece::PIECE_NONE;

    return (move16 & Move::MOVE_FROMDEST_MASK)
         | (static_cast<U32>(piece)    << Move::SHIFT_PIECE)
         | (static_cast<U32>(captured) << Move::SHIFT_CAPT)
         | (static_cast<U32>(promo)    << Move::SHIFT_PROMO)
         | Flags[code];
}

//========================================================
//! \brief  Constructeur avec argument
//! \param[in]  MB  taille de la table de transposition, en mégaoctets
//--------------------------------------------------------
TranspositionTable::TranspositionTable(int MB) :
    nbr_cluster(0),
    tt_age(0)
{
#if defined DEBUG_LOG
    char message[100];
    sprintf(message, "TranspositionTable::constructeur MB : %d ", MB);
    printlog(message);
#endif

    init_size(MB);
}

//========================================================
//! \brief  Initialisation de la table
//! \param[in]  mbsize  taille de la table de transposition, en mégaoctets
//--------------------------------------------------------
void TranspositionTable::init_size(int mbsize)
{
#if defined DEBUG_LOG
    char message[1000];
    sprintf(message, "TranspositionTable::init_size : %d ", mbsize);
    printlog(message);
#endif

    // Le cast est indispensable : en int, 2048 Mo déborde et donne un nombre de
    // clusters absurde, 4096 Mo en donne zéro (table vide, index hors bornes).
    size_t size  = (static_cast<size_t>(mbsize) * 1024 * 1024) / sizeof(HashCluster);

    // L'index est calculé par : key & tt_mask
    // if faut que le nombre de clusters soit un multiple de 2
    // int keySize = static_cast<int>(std::log2(size));    // puissance de 2

    if (size != nbr_cluster)
    {
        // On alloue AVANT de libérer : si le système refuse, la table en place
        // reste utilisable.
        HugeArray<HashCluster> fresh = make_huge_array<HashCluster>(size);

        if (!fresh)
        {
            std::cout << "info string hash " << mbsize
                      << " Mo refuse par le systeme, taille inchangee" << std::endl;

            if (!tt_entries)
            {
                std::cout << "info string pas de table de transposition, arret" << std::endl;
                std::abort();
            }
            return;     // pas de clear() : la table courante est intacte
        }

        tt_entries  = std::move(fresh);     // libère l'ancienne au passage
        nbr_cluster = size;
    }

    clear();


#if defined DEBUG_LOG
    sprintf(message, "TranspositionTable::init complete : \n %s", info().c_str() );
    printlog(message);
#endif
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

    // Remet chaque HashCluster à zéro. C'est aussi ce premier parcours qui
    // matérialise les huge pages : sous THP=madvise, madvise() ne fait que marquer
    // la zone, la promotion n'a lieu qu'au premier accès (même raison que le
    // tt_clear() d'Ethereal juste après son madvise).
    if (tt_entries)
        // Le cast en void* est volontaire : HashCluster a un initialisateur par
        // défaut, et on le court-circuite sciemment. Sans lui, -Wclass-memaccess.
        std::memset(static_cast<void*>(tt_entries.get()), 0,
                    nbr_cluster * sizeof(HashCluster));
}

//========================================================
//! \brief  Ecriture dans la hashtable d'une nouvelle donnée
//! \param[in]  key    code hash (Zobrist) de la position
//! \param[in]  move   meilleur coup trouvé pour cette position
//! \param[in]  score  score de la position (converti avant stockage)
//! \param[in]  eval   évaluation statique de la position
//! \param[in]  bound  type de borne (BOUND_NONE/UPPER/LOWER/EXACT)
//! \param[in]  depth  profondeur de recherche réelle : DEPTH_EVAL, ou DEPTH_QS..MAX_PLY-1
//! \param[in]  ply    profondeur (distance à la racine) de la position
//! \param[in]  pv     "true" si le nœud provient d'une ligne principale
//--------------------------------------------------------
void TranspositionTable::store(U64 key, MOVE move, int score, int eval, int bound, int depth, int ply, bool pv)
{
    assert(depth == DEPTH_EVAL || (DEPTH_QS <= depth && depth < MAX_PLY));

    // DEPTH_EVAL et BOUND_NONE vont ensemble
    assert((depth == DEPTH_EVAL) == (bound == BOUND_NONE));

    assert(move != Move::MOVE_NULL);

    // bits bas de la clé : l'index du cluster utilise les bits hauts
    const U16 key16 = static_cast<U16>(key);

    HashCluster& cluster   = tt_entries[index(key)];
    HashEntry*   replace   = nullptr;
    bool         gratuit   = false;   // vrai si l'entrée prise ne coûte aucun résultat de recherche
    auto         minValue  = std::numeric_limits<I32>::max();

    for (auto & entry : cluster.entries)
    {
        //  Si l'entrée contient la même position, ou est vierge, ou contient une évaluation statique :
        //    on la prend tout de suite.
        //  Réutiliser cette entrée existante, plutôt que de consommer une entrée vierge du cluster,
        //    est ce qui empêche la table de se saturer d'entrées sans coup.
        if (entry.key16 == key16 || entry.empty() || entry.depth() == DEPTH_EVAL)
        {
            replace = &entry;
            gratuit = true;
            break;
        }

        // sinon, on prend l'entrée de plus faible poids (profondeur et âge).
        I32 value = entry.depth() - 2 * entry.relative_age(tt_age);
        if (value < minValue)
        {
            replace  = &entry;
            minValue = value;
        }
    }

    assert(replace != nullptr);

    // Une évaluation statique ne vaut pas qu'on évince un résultat de recherche :
    // Si le cluster ne contient que des résultats de recherche, on renonce.
    if (depth == DEPTH_EVAL && gratuit == false)
        return;

    // Approximativement le schéma de remplacement de SF
    // On n'écrase pas une entrée de la même position, sauf si on a
    // une borne exacte ou une profondeur presque aussi bonne que l'ancienne
    if ((bound == BOUND_EXACT
         || key16 != replace->key16
         || replace->relative_age(tt_age)           // replace->age() != tt_age
         || depth + 3 + 2*pv > replace->depth()))
    {
        // idée de Sirius, Stockfish et Ethereal
        // Préserve le coup existant pour la même position
        // Ne pas écraser le coup s'il n'y a pas de nouveau meilleur coup
        if (move != Move::MOVE_NONE || replace->key16 != key16)
            replace->move16 = move_to_move16(move);

        replace->key16  = key16;
        replace->score  = static_cast<I16>(ScoreToTT(score, ply));
        replace->eval   = static_cast<I16>(eval);
        replace->set_depth(depth);
        replace->setAgePvBound(tt_age, pv, bound);
    }
}

//========================================================
//! \brief  Recherche d'une donnée dans la table de transposition
//! \param[in]  key     code hash (Zobrist) de la position recherchée
//! \param[in]  ply     profondeur (distance à la racine) de la position
//! \param[out] move16  coup trouvé, compacté : voir move16_to_move()
//! \param[out] score   score de la position (converti depuis le format TT)
//! \param[out] eval    évaluation statique stockée
//! \param[out] bound   type de borne (BOUND_NONE/UPPER/LOWER/EXACT)
//! \param[out] depth   profondeur réelle de l'entrée
//! \param[out] pv      "true" si l'entrée provient d'une ligne principale
//! \return Retourne "true" si une entrée ÉCRITE correspond à "key".
//--------------------------------------------------------
bool TranspositionTable::probe(U64 key, int ply, U16& move16, int &score, int& eval, int &bound, int& depth, bool& pv)
{
    const U16 key16 = static_cast<U16>(key);
    const HashCluster& cluster = tt_entries[index(key)];
    for (const HashEntry& entry : cluster.entries)
    {
        // Il faut s'assurer que l'entrée n'est pas vierge.
        // Sinon une entrée dont les 16 bits bas seraient nuls pourrait être acceptée.
        // L'ordre compte : empty() en premier coûte une lecture et une branche sur
        // chaque entrée du cluster, au lieu d'une seule sur correspondance de clé.
        if (entry.key16 == key16 && entry.empty() == false)
        {
            move16 = entry.move16;
            bound = entry.bound();
            depth = entry.depth();
            score = ScoreFromTT(entry.score, ply);
            eval  = entry.eval;
            pv    = entry.pv();

            return true;
        }
    }

    return false;
}

//=======================================================================
//! \brief Estimation de l'utilisation de la table de transposition
//! Compte, sur les 1000 premiers clusters, les entrées écrites ayant l'âge
//! courant. C'est l'empreinte de la recherche en cours et
//! non le remplissage de la table : pour celui-ci, voir occupancy().
//! \return Utilisation en pour mille, de 0 à 1000 (1 = 0,1 %)
//-----------------------------------------------------------------------
int TranspositionTable::hash_full() const
{
    int used = 0;

    for (int i = 0; i < 1000; i++)
    {
        for (size_t j=0; j<CLUSTER_SIZE; j++)
        {
            // Toute entrée écrite par la recherche en cours, évaluation statique comprise.
            // Le test « sans coup » d'avant ratait aussi les fail low stockés
            // sans meilleur coup. Convention de Stockfish, Obsidian et Ethereal.
            if (   !tt_entries[i].entries[j].empty()
                   && tt_entries[i].entries[j].age() == tt_age
                   )
                used++;
        }
    }

    return used/CLUSTER_SIZE;
}
//=================================================================
//! \brief  Occupation réelle de la table, sur les 1000 premiers clusters.
//!
//! Complète hash_full(), qui ne compte que l'âge courant : celui-ci mesure le
//! remplissage réel de la table, pas l'empreinte de la recherche en cours.
//!
//! \param[out] physique       toute case écrite ; une case vierge porte la
//!                            profondeur réservée 0, d'où le test sur empty()
//! \param[out] age_courant    entrées de la recherche en cours, ou de celle qui
//!                            vient de s'achever : ThreadPool::start_thinking
//!                            avance l'âge AVANT de chercher, donc une
//!                            interrogation post-recherche lit bien ce champ
//! \param[out] age_precedent  entrées de la recherche d'avant
//! \param[out] eval_seule     entrées de l'évaluation statique, sans résultat de recherche
//-----------------------------------------------------------------------
void TranspositionTable::occupancy(int& physique, int& age_courant,
                                  int& age_precedent, int& eval_seule) const
{
    const U32 age_prec = (tt_age - 1) & HashEntry::AgeMask;

    physique = age_courant = age_precedent = eval_seule = 0;

    for (int i = 0; i < 1000; i++)
    {
        for (size_t j = 0; j < CLUSTER_SIZE; j++)
        {
            const HashEntry& e = tt_entries[i].entries[j];
            if (e.empty())
                continue;

            physique++;
            if (e.age()   == tt_age)         age_courant++;
            if (e.age()   == age_prec)       age_precedent++;
            if (e.depth() == DEPTH_EVAL)     eval_seule++;
        }
    }

    physique      /= CLUSTER_SIZE;
    age_courant   /= CLUSTER_SIZE;
    age_precedent /= CLUSTER_SIZE;
    eval_seule    /= CLUSTER_SIZE;
}


//=======================================================================
//! \brief  Construit une chaîne décrivant la configuration de la table
//!         de transposition (taille, nombre de clusters/entrées)
//! \return Chaîne de description, prête à être affichée ou loguée
//-----------------------------------------------------------------------
std::string TranspositionTable::info()
{
    std::stringstream sstr;

    sstr <<      "Nombre de clusters  : " << std::to_string(nbr_cluster) << std::endl
              << "Taille d'un cluster : " << sizeof(HashCluster) << " octets" << std::endl
              << "Entrées par cluster : " << CLUSTER_SIZE << std::endl
              << "Taille d'une entrée : " << sizeof(HashEntry) << " octets" << std::endl
              << "Total entrées       : " << nbr_cluster * CLUSTER_SIZE << std::endl
              << "Taille totale       : " << nbr_cluster*sizeof(HashCluster) << "  " << nbr_cluster*CLUSTER_SIZE*sizeof(HashEntry)
              << " (" << nbr_cluster*sizeof(HashCluster)/1024.0/1024.0 << ") Mo"
              << std::endl;

    return sstr.str();
}
