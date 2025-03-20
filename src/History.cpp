#include <cstring>
#include "History.h"
#include "Move.h"
#include "types.h"

History::History()
{

}

//*********************************************************************
void History::reset()
{
    std::memset(main_history,         0, sizeof(MainHistoryTable));
    std::memset(counter_move,          0, sizeof(CounterMoveTable));
    std::memset(continuation_history,  0, sizeof(ContinuationHistoryTable));
    std::memset(capture_history,       0, sizeof(CaptureHistory));
}

//=========================================================
//! \brief  Met à jour les heuristiques "Killer"
//---------------------------------------------------------
void History::update_killers(SearchInfo* info, MOVE move)
{
    if (info->killer1 != move)
    {
        info->killer2 = info->killer1;
        info->killer1 = move;
    }
}

//==================================================================
//! \brief  Récupère le counter_move
//! \param[in] info         recherche actuelle
//------------------------------------------------------------------
MOVE History::get_counter_move(const SearchInfo* info) const
{
    MOVE previous_move = (info-1)->move;

    return (Move::is_ok(previous_move)) ?
                counter_move[static_cast<U32>(Move::piece(previous_move))][Move::dest(previous_move)] :
                Move::MOVE_NONE;
}

//=================================================================
//! \brief  Update counter_move.
//! \param[in] info         recherche actuelle
//! \param[in] move         coup de réfutation
//-----------------------------------------------------------------
void History::update_counter_move(const SearchInfo* info, MOVE move)
{
    MOVE previous_move = (info-1)->move;

    if (Move::is_ok(previous_move))
        counter_move[static_cast<U32>(Move::piece(previous_move))][Move::dest(previous_move)] = move;
}


I16 History::get_capture_history(MOVE move) const
{
    return capture_history[static_cast<U32>(Move::piece(move))][Move::dest(move)][static_cast<U32>(Move::captured_type(move))];
}

void History::update_capture_history(MOVE move, I16 bonus)
{
    I16& histo = capture_history
            [static_cast<U32>(Move::piece(move))]
            [Move::dest(move)]
            [static_cast<U32>(Move::captured_type(move))];
    histo += scale_bonus(histo, bonus);
}

I16 History::scale_bonus(I16 score, I16 bonus)
{
    auto c_bonus = std::clamp(bonus, BONUS_MIN, BONUS_MAX);

    return 32 * c_bonus - score * abs(c_bonus) / 512;
}

I16 History::get_quiet_history(Color color, const SearchInfo* info, const MOVE move) const
{
    I16 score = main_history[color][Move::from(move)][Move::dest(move)];

    MOVE previous_move;
    const int  delta[3] = {1, 2, 4};

    for (int n=0; n<3; n++)
    {
        int d = delta[n];
        previous_move = (info - d)->move;
        if (Move::is_ok(previous_move))
            score += *(info - d)->continuation_history[static_cast<U32>(Move::piece(move))][Move::dest(move)];
    }

    return score;
}


void History::update_quiet_history(Color color, const SearchInfo* info, MOVE move, I16 bonus)
{
    // valeur actuelle de l'main_history
    I16& mh = main_history[color][Move::from(move)][Move::dest(move)];
    mh += scale_bonus(mh, bonus);

    //TODO : vérifier le bonus

    MOVE previous_move;

    const int  delta[3] = {1, 2, 4};

    for (int n=0; n<3; n++)
    {
        int d = delta[n];
        previous_move = (info - d)->move;
        if (Move::is_ok(previous_move))
        {
            I16& histo = (*(info - d)->continuation_history)[static_cast<U32>(Move::piece(move))][Move::dest(move)];
            histo += scale_bonus(histo, bonus);
        }
    }

}
