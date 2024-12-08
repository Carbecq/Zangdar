#include <cmath>
#include <cstring>
#include <iomanip>
#include "Search.h"
#include "ThreadPool.h"
#include "Move.h"
#include "TranspositionTable.h"


//=============================================
//! \brief  Constructeur
//---------------------------------------------
Search::Search()
{
#if defined DEBUG_LOG
    char message[100];
    sprintf(message, "Search constructeur");
    printlog(message);
#endif

    // Initializes the late move reduction array
    for (int depth = 1; depth < 32; ++depth)
        for (int moves = 1; moves < 32; ++moves)
            Reductions[0][depth][moves] = 0.00 + log(depth) * log(moves) / 3.25, // capture
                Reductions[1][depth][moves] = 1.75 + log(depth) * log(moves) / 2.25; // quiet
}

//=============================================
//! \brief  Destructeur
//---------------------------------------------
Search::~Search() {}

//=========================================================
//! \brief  Affichage UCI du résultat de la recherche
//!
//! \param[in] depth        profondeur de la recherche
//! \param[in] best_score   meilleur score
//! \param[in] elapsed      temps passé pour la recherche, en millisecondes
//---------------------------------------------------------
void Search::show_uci_result(const ThreadData* td, I64 elapsed, const PVariation& pv) const
{
    elapsed++; // évite une division par 0
    // commande envoyée à UCI
    // voir le document : uci_commands.txt

    std::stringstream stream;

// en mode UCI, il ne faut pas mettre les points de séparateur,
// sinon Arena n'affiche pas correctement le nombre de nodes
#if defined USE_PRETTY
    stream.imbue(std::locale(std::locale::classic(), new MyNumPunct));
    int l = 12;
#endif

    /*  score
     *      cp <x>
     *          the score from the engine's point of view in centipawns.
     *      mate <y>
     *          mate in y moves, not plies.
     *          If the engine is getting mated use negative values for y.
     */

    //collect info about nodes from all Threads
    U64 all_nodes  = threadPool.get_all_nodes();
    U64 all_tbhits = threadPool.get_all_tbhits();
    int hash_full  = transpositionTable.hash_full();

    stream << "info "

#if defined USE_PRETTY
           << " depth "    << std::setw(2) << td->best_depth                     // depth <x> search depth in plies
           << " seldepth " << std::setw(2) << td->seldepth                       // seldepth <x> selective search depth in plies
#else
           << " depth "    << td->best_depth
           << " seldepth " << td->seldepth
#endif

// time     : the time searched in ms
// nodes    : noeuds calculés
// nps      : nodes per second searched

#if defined USE_PRETTY
              << " time "       << std::setw(6) << elapsed                          // time <x> the time searched in ms
              << " nodes "      << std::setw(l) << all_nodes
              << " nps "        << std::setw(7) << all_nodes * 1000 / elapsed       // nps <x> x nodes per second searched,
              << " tbhits "     << all_tbhits                                       // tbhits <x> x positions where found in the endgame table bases
           << " hashfull "   << std::setw(5) << hash_full;                          // hashfull <x> the hash is x permill full,
#else
              << " time "       << elapsed
              << " nodes "      << all_nodes
              << " nps "        << all_nodes * 1000 / elapsed
              << " tbhits "     << all_tbhits
           << " hashfull "   << hash_full;
#endif

    if (td->best_score >= MATE_IN_X)
    {
        stream << " score mate " << (MATE - td->best_score) / 2 + 1;             // score mate <y> mate in y moves, not plies.
    }
    else if (td->best_score <= -MATE_IN_X)
    {
        stream << " score mate " << (-MATE - td->best_score) / 2;
    }
    else
    {

#if defined USE_PRETTY
        stream << " score cp " << std::right << std::setw(5) << td->best_score;  // score cp <x> the score from the engine's point of view in centipawns.
#else
        stream << " score cp " << td->best_score;
#endif
    }

    stream << " pv";
    for (int i=0; i<pv.length; i++)
        stream << " " << Move::name(pv.line[i]);

    std::cout << stream.str() << std::endl;
}

//=========================================================
//! \brief  Affichage UCI du meilleur coup trouvé
//!
//! \param[in]  name   coup en notation UCI
//---------------------------------------------------------
void Search::show_uci_best(const ThreadData* td) const
{
    // ATTENTION AU FORMAT D'AFFICHAGE
    std::cout << "bestmove " << Move::name(td->best_move) << std::endl;
}

//=========================================================
//! \brief  Affichage UCI du coup en cours d'étude
//!
//---------------------------------------------------------
void Search::show_uci_current(MOVE move, int currmove, int depth) const
{
    std::cout << "info depth "      << depth
              << " currmove "       << Move::show(move, 4)
              << " currmovenumber " << currmove
              << std::endl;
}

//=========================================================
//! \brief  Mise à jour de la Principal variation
//!
//! \param[in]  name   coup en notation UCI
//---------------------------------------------------------
void Search::update_pv(SearchInfo* si, const MOVE move) const
{
    si->pv.length = 1 + (si+1)->pv.length;
    si->pv.line[0] = move;
    memcpy(si->pv.line+1, (si+1)->pv.line, sizeof(MOVE) * (si+1)->pv.length);
}

//=========================================================
//! \brief  Met à jour les heuristiques "Killer"
//---------------------------------------------------------
void Search::update_killers(ThreadData* td, int ply, MOVE move)
{
    if (td->killer1[ply] != move)
    {
        td->killer2[ply] = td->killer1[ply];
        td->killer1[ply] = move;
    }
}


//=========================================================
//! \brief  Incrémente l'heuristique "History"
//---------------------------------------------------------
void Search::update_history(ThreadData* td, Color color, MOVE move, int bonus)
{
    // valeur actuelle de l'history
    int histo = td->history[color][Move::from(move)][Move::dest(move)];

    // le bonus est contenu dans la zone [-400 , 400]
    bonus  = std::max(-400, std::min(400, bonus));

    // la nouvelle valeur de l'history est contenue dans la zone [-16384, 16384]
    histo += 32 * bonus - histo * abs(bonus) / 512;

    // Sauve la nouvelle valeur de l'history
    td->history[color][Move::from(move)][Move::dest(move)] = histo;
}

//=================================================================
//! \brief  Update counter_move.
//! \param[in] oppcolor     couleur du camp opposé
//! \param[in] prev_move    coup recherché au ply précédant
//! \param[in] move         coup de réfutation
//-----------------------------------------------------------------
void Search::update_counter_move(ThreadData* td, Color oppcolor, int ply, MOVE move)
{
    MOVE previous_move = td->info[ply-1].move;

    if (previous_move != Move::MOVE_NONE && previous_move != Move::MOVE_NULL)
        td->counter_move[oppcolor][Move::piece(previous_move)][Move::dest(previous_move)] = move;
}

//==================================================================
//! \brief  Récupère le counter_move
//! \param[in] oppcolor     couleur du camp opposé
//! \param[in] ply          ply de recherche
//------------------------------------------------------------------
MOVE Search::get_counter_move(ThreadData* td, Color oppcolor, int ply) const
{
    MOVE previous_move = td->info[ply-1].move;

    return( (previous_move==Move::MOVE_NONE || previous_move==Move::MOVE_NULL)
                ? Move::MOVE_NONE
                : td->counter_move[oppcolor][Move::piece(previous_move)][Move::dest(previous_move)] );
}

//=================================================================
//! \brief  Update Counter Move History.
//! \param[in] prev_move    coup recherché au ply précédant
//! \param[in] move         coup de réfutation
//-----------------------------------------------------------------
void Search::update_counter_move_history(ThreadData* td, int ply, MOVE move, int bonus)
{
    MOVE previous_move = td->info[ply-1].move;

    // Check for root position or null moves
    if (previous_move == Move::MOVE_NONE || previous_move == Move::MOVE_NULL)
        return;

    int cm_histo;

    bonus = std::max(-400, std::min(400, bonus));

    PieceType prev_piece = Move::piece(previous_move);
    int       prev_dest  = Move::dest(previous_move);

    PieceType piece = Move::piece(move);
    int       dest  = Move::dest(move);

    cm_histo = td->counter_move_history[prev_piece][prev_dest][piece][dest];
    cm_histo += 32 * bonus - cm_histo * abs(bonus) / 512;
    td->counter_move_history[prev_piece][prev_dest][piece][dest] = cm_histo;
}

//=================================================================
//! \brief  Update Counter Move History.
//! \param[in] prev_move    coup recherché au ply précédant
//! \param[in] move         coup de réfutation
//-----------------------------------------------------------------
void Search::update_followup_move_history(ThreadData* td, int ply, MOVE move, int bonus)
{
    MOVE followup_move = td->info[ply-2].move;

    // Check for root position or null moves
    if (followup_move == Move::MOVE_NONE || followup_move == Move::MOVE_NULL)
        return;

    int fm_histo;

    bonus = std::max(-400, std::min(400, bonus));

    PieceType followup_piece = Move::piece(followup_move);
    int       followup_dest  = Move::dest(followup_move);

    PieceType piece = Move::piece(move);
    int       dest  = Move::dest(move);

    fm_histo = td->followup_move_history[followup_piece][followup_dest][piece][dest];
    fm_histo += 32 * bonus - fm_histo * abs(bonus) / 512;
    td->followup_move_history[followup_piece][followup_dest][piece][dest] = fm_histo;
}



