#include "ThreadData.h"
#include "Move.h"
#include <cstring>

ThreadData::ThreadData()
{
}

//=================================================
//! \brief  Initialisation des valeurs des threads
//! lors de la création
//-------------------------------------------------
void ThreadData::create(int i)
{
    index      = i;
    depth      = 0;
    score      = -INFINITE;
    seldepth = 0;
    nodes    = 0;
    stopped  = false;

    // Grace au décalage, la position root peut regarder en arrière
    info     = &(_info[STACK_OFFSET]);
}

//=================================================
//! \brief  Initialisation des valeurs des threads
//! Utilisé lors de "start_thinking"
//-------------------------------------------------
void ThreadData::init()
{
    nodes      = 0;
    seldepth   = 0;

    // on prend TOUT le tableau
    std::memset(_info, 0, sizeof(SearchInfo)*STACK_SIZE);
}

//=================================================
//! \brief  Initialisation des valeurs des threads
//! Utilisé lors de "newgame"
//-------------------------------------------------
void ThreadData::reset()
{
    nodes      = 0;
    seldepth   = 0;

    std::memset(_info,      0, sizeof(SearchInfo)*STACK_SIZE);
    std::memset(history,    0, sizeof(history));
    std::memset(cm_table,   0, sizeof(cm_table));
    std::memset(cm_history, 0, sizeof(cm_history));
}

//=========================================================
//! \brief  Mise à jour de la Principal variation
//!
//! \param[in]  name   coup en notation UCI
//---------------------------------------------------------
void ThreadData::update_pv(SearchInfo* si, const MOVE move) const
{
    si->pv.length = 1 + (si+1)->pv.length;
    si->pv.line[0] = move;
    memcpy(si->pv.line+1, (si+1)->pv.line, sizeof(MOVE) * (si+1)->pv.length);
}

//=========================================================
//! \brief  Met à jour les heuristiques "Killer"
//---------------------------------------------------------
void ThreadData::update_killers(SearchInfo* si, MOVE move)
{
    if (si->killer1 != move)
    {
        si->killer2 = si->killer1;
        si->killer1 = move;
    }
}


//=========================================================
//! \brief  Incrémente l'heuristique "History"
//---------------------------------------------------------
void ThreadData::update_history(Color color, MOVE move, int bonus)
{
    // valeur actuelle de l'history
    int histo = history[color][Move::from(move)][Move::dest(move)];

    // le bonus est contenu dans la zone [-400 , 400]
    bonus  = std::max(-400, std::min(400, bonus));

    // la nouvelle valeur de l'history est contenue dans la zone [-16384, 16384]
    histo += 32 * bonus - histo * abs(bonus) / 512;

    // Sauve la nouvelle valeur de l'history
    history[color][Move::from(move)][Move::dest(move)] = histo;
}

//=========================================================
//! \brief  Récupère l'heuristique "History"
//---------------------------------------------------------
int ThreadData::get_history(Color color, const MOVE move) const
{
    return(history[color][Move::from(move)][Move::dest(move)]);
}

//=================================================================
//! \brief  Update countermove.
//! \param[in] oppcolor     couleur du camp opposé
//! \param[in] prev_move    coup recherché au ply précédant
//! \param[in] move         coup de réfutation
//-----------------------------------------------------------------
void ThreadData::update_counter_move(Color oppcolor, int ply, MOVE move)
{
    MOVE previous_move = info[ply-1].move;

    if (previous_move != Move::MOVE_NONE && previous_move != Move::MOVE_NULL)
        cm_table[oppcolor][Move::piece(previous_move)][Move::dest(previous_move)] = move;
}

//==================================================================
//! \brief  Récupère le countermove
//! \param[in] oppcolor     couleur du camp opposé
//! \param[in] ply          ply de recherche
//------------------------------------------------------------------
MOVE ThreadData::get_counter_move(Color oppcolor, int ply) const
{
    MOVE previous_move = info[ply-1].move;

    return( (previous_move==Move::MOVE_NONE || previous_move==Move::MOVE_NULL)
                ? Move::MOVE_NONE
                : cm_table[oppcolor][Move::piece(previous_move)][Move::dest(previous_move)] );
}

//=================================================================
//! \brief  Update counter_move history.
//! \param[in] prev_move    coup recherché au ply précédant
//! \param[in] move         coup de réfutation
//-----------------------------------------------------------------
void ThreadData::update_counter_history(int ply, MOVE move, int bonus)
{
    MOVE previous_move = info[ply-1].move;

    // Check for root position or null moves
    if (previous_move == Move::MOVE_NONE || previous_move == Move::MOVE_NULL)
        return;

    int cm_histo;

    bonus = std::max(-400, std::min(400, bonus));

    PieceType prev_piece = Move::piece(previous_move);
    int       prev_dest  = Move::dest(previous_move);

    PieceType piece = Move::piece(move);
    int       dest  = Move::dest(move);

    cm_histo = cm_history[prev_piece][prev_dest][piece][dest];
    cm_histo += 32 * bonus - cm_histo * abs(bonus) / 512;
    cm_history[prev_piece][prev_dest][piece][dest] = cm_histo;
}

//==================================================================
//! \brief  Récupère le counter_move history
//! \param[in] ply    ply cherché
//------------------------------------------------------------------
MOVE ThreadData::get_counter_history(int ply, MOVE move) const
{
    MOVE previous_move = info[ply-1].move;

    return( (previous_move==Move::MOVE_NONE || previous_move==Move::MOVE_NULL)
                ? Move::MOVE_NONE
                : cm_history[Move::piece(previous_move)][Move::dest(previous_move)][Move::piece(move)][Move::dest(move)] );
}
