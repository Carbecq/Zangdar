#include "OrderInfo.h"
#include "Move.h"
#include <cstring>

OrderInfo::OrderInfo()
{
    clear_all();
}

//=========================================================
//! \brief  Re-initialise tous les heuristiques
//---------------------------------------------------------
void OrderInfo::clear_all()
{
    // std::memset(killer1, 0, sizeof(killer1));
    // std::memset(killer2, 0, sizeof(killer2));
    std::memset(history, 0, sizeof(history));
    std::memset(counter, 0, sizeof(counter));
    // std::memset(excluded, 0, sizeof(excluded));
}



//=========================================================
//! \brief  Incrémente l'heuristique "History"
//---------------------------------------------------------
void OrderInfo::update_history(Color color, MOVE move, int bonus)
{
    history[color][Move::piece(move)][Move::dest(move)] += bonus;
}

//=========================================================
//! \brief  Récupère l'heuristique "History"
//---------------------------------------------------------
int OrderInfo::get_history(const Color color, const MOVE move) const
{
    return(history[color][Move::piece(move)][Move::dest(move)]);
}

//=================================================================
//! \brief  Update countermove.
//! \param[in] color        couleur du camp qui joue
//! \param[in] ply          profondeur courante de la recherche
//! \param[in] prev_move    coup recherché au ply précédant
//! \param[in] move         coup de réfutation
//-----------------------------------------------------------------
void OrderInfo::update_counter(Color color, int ply, MOVE prev_move, MOVE move)
{
    if (prev_move != Move::MOVE_NONE && prev_move != Move::MOVE_NULL)
        counter[color][Move::piece(prev_move)][Move::dest(prev_move)] = move;
}

