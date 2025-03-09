#include "ThreadData.h"
#include "Move.h"

ThreadData::ThreadData()
{

}

//*********************************************************************
void ThreadData::reset()
{
    std::memset(_info,                 0, sizeof(SearchInfo)*STACK_SIZE);
    std::memset(killer1,               0, sizeof(KillerTable));
    std::memset(killer2,               0, sizeof(KillerTable));
    std::memset(history,               0, sizeof(Historytable));
    std::memset(counter_move,          0, sizeof(CounterMoveTable));
    std::memset(counter_move_history,  0, sizeof(CounterMoveHistoryTable));
    std::memset(followup_move_history, 0, sizeof(FollowupMoveHistoryTable));
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
void ThreadData::update_killers(int ply, MOVE move)
{
    if (killer1[ply] != move)
    {
        killer2[ply] = killer1[ply];
        killer1[ply] = move;
    }
}

//*********************************************************************
int ThreadData::get_history(Color color, const MOVE move) const
{
    return(history[color][Move::from(move)][Move::dest(move)]);
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

//==================================================================
//! \brief  Récupère le counter_move
//! \param[in] oppcolor     couleur du camp opposé
//! \param[in] ply          ply de recherche
//------------------------------------------------------------------
MOVE ThreadData::get_counter_move(Color oppcolor, int ply) const
{
    MOVE previous_move = info[ply-1].move;

    return( (previous_move==Move::MOVE_NONE || previous_move==Move::MOVE_NULL)
                ? Move::MOVE_NONE
                : counter_move[oppcolor][Move::piece(previous_move)][Move::dest(previous_move)] );
}

//=================================================================
//! \brief  Update counter_move.
//! \param[in] oppcolor     couleur du camp opposé
//! \param[in] prev_move    coup recherché au ply précédant
//! \param[in] move         coup de réfutation
//-----------------------------------------------------------------
void ThreadData::update_counter_move(Color oppcolor, int ply, MOVE move)
{
    MOVE previous_move = info[ply-1].move;

    if (previous_move != Move::MOVE_NONE && previous_move != Move::MOVE_NULL)
        counter_move[oppcolor][Move::piece(previous_move)][Move::dest(previous_move)] = move;
}

//==================================================================
//! \brief  Récupère le counter_move history
//! \param[in] ply    ply cherché
//------------------------------------------------------------------
int ThreadData::get_counter_move_history(int ply, MOVE move) const
{
    MOVE previous_move = info[ply-1].move;

    return( (previous_move==Move::MOVE_NONE || previous_move==Move::MOVE_NULL)
                ? 0
                : counter_move_history[Move::piece(previous_move)][Move::dest(previous_move)][Move::piece(move)][Move::dest(move)] );
}

//=================================================================
//! \brief  Update Counter Move History.
//! \param[in] prev_move    coup recherché au ply précédant
//! \param[in] move         coup de réfutation
//-----------------------------------------------------------------
void ThreadData::update_counter_move_history(int ply, MOVE move, int bonus)
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

    cm_histo = counter_move_history[prev_piece][prev_dest][piece][dest];
    cm_histo += 32 * bonus - cm_histo * abs(bonus) / 512;
    counter_move_history[prev_piece][prev_dest][piece][dest] = cm_histo;
}

int ThreadData::get_followup_move_history(int ply, MOVE move) const
{
    MOVE folowup_move = info[ply-2].move;

    return( (folowup_move==Move::MOVE_NONE || folowup_move==Move::MOVE_NULL)
                ? 0
                : followup_move_history[Move::piece(folowup_move)][Move::dest(folowup_move)][Move::piece(move)][Move::dest(move)] );
}

//=================================================================
//! \brief  Update Counter Move History.
//! \param[in] prev_move    coup recherché au ply précédant
//! \param[in] move         coup de réfutation
//-----------------------------------------------------------------
void ThreadData::update_followup_move_history(int ply, MOVE move, int bonus)
{
    MOVE followup_move = info[ply-2].move;

    // Check for root position or null moves
    if (followup_move == Move::MOVE_NONE || followup_move == Move::MOVE_NULL)
        return;

    int fm_histo;

    bonus = std::max(-400, std::min(400, bonus));

    PieceType followup_piece = Move::piece(followup_move);
    int       followup_dest  = Move::dest(followup_move);

    PieceType piece = Move::piece(move);
    int       dest  = Move::dest(move);

    fm_histo = followup_move_history[followup_piece][followup_dest][piece][dest];
    fm_histo += 32 * bonus - fm_histo * abs(bonus) / 512;
    followup_move_history[followup_piece][followup_dest][piece][dest] = fm_histo;
}




