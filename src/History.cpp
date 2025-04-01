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
    std::memset(counter_move,         0, sizeof(CounterMoveTable));
    std::memset(continuation_history, 0, sizeof(ContinuationHistoryTable));
    // std::memset(capture_history,       0, sizeof(CaptureHistory));
}

//==================================================================
//! \brief  Récupère le counter_move
//! \param[in] info         recherche actuelle
//------------------------------------------------------------------
MOVE History::get_counter_move(const SearchInfo* info) const
{
    MOVE previous_move = (info-1)->move;

    return (Move::is_ok(previous_move))
            ? counter_move[static_cast<U32>(Move::piece(previous_move))][Move::dest(previous_move)]
              : Move::MOVE_NONE;
}

//*********************************************************************
I16 History::get_main_history(Color color, const MOVE move) const
{
    return(main_history[color][Move::from(move)][Move::dest(move)]);
}

//==================================================================
//! \brief  Récupère le counter_move history
//! \param[in] ply    ply cherché
//------------------------------------------------------------------
I16 History::get_counter_move_history(const SearchInfo* info, MOVE move) const
{
    if (Move::is_ok((info-1)->move))
    {
        return (*(info - 1)->cont_hist)[static_cast<U32>(Move::piece(move))][Move::dest(move)];
    }
    else
    {
        return 0;
    }
}

I16 History::get_followup_move_history(const SearchInfo* info, MOVE move) const
{
    if (Move::is_ok((info-2)->move))
    {
        return (*(info - 2)->cont_hist)[static_cast<U32>(Move::piece(move))][Move::dest(move)];
    }
    else
    {
        return 0;
    }
}

//==============================================================
//! \brief  Retourne la somme de tous les history "quiet"
//--------------------------------------------------------------
int History::get_quiet_history(Color color, const SearchInfo* info, const MOVE move) const
{
    // Main History
    int score = main_history[color][Move::from(move)][Move::dest(move)];

    if (Move::is_ok((info-1)->move))
    {
        score += (*(info - 1)->cont_hist)[static_cast<U32>(Move::piece(move))][Move::dest(move)];
    }

    if (Move::is_ok((info-2)->move))
    {
        score += (*(info - 2)->cont_hist)[static_cast<U32>(Move::piece(move))][Move::dest(move)];
    }

    return score;
}

void History::update_quiet_history(Color color, SearchInfo* info, MOVE best_move, I16 depth,
                                   int quiet_count, std::array<MOVE, MAX_MOVES>& quiet_moves)
{
    // Counter Move
    //TODO test sur previous_move is_ok utile ??
    MOVE previous_move = (info-1)->move;
    if (Move::is_ok(previous_move))
        counter_move[static_cast<U32>(Move::piece(previous_move))][Move::dest(previous_move)] = best_move;

    // Killer Moves
    if (info->killer1 != best_move)
    {
        info->killer2 = info->killer1;
        info->killer1 = best_move;
    }

    // Bonus pour le coup quiet ayant provoqué un cutoff (fail-high)

    I16* histo = &(main_history[color][Move::from(best_move)][Move::dest(best_move)]);
    scale_bonus(histo, depth*depth);

    if (Move::is_ok((info-1)->move))
    {
        histo = &(*(info - 1)->cont_hist)[static_cast<int>(Move::piece(best_move))][Move::dest(best_move)];
        scale_bonus(histo, depth*depth);
    }

    if (Move::is_ok((info-2)->move))
    {
        histo = &(*(info - 2)->cont_hist)[static_cast<U32>(Move::piece(best_move))][Move::dest(best_move)];
        scale_bonus(histo, depth*depth);
    }

    MOVE move;
    // Malus pour les autres coups quiets
    for (int i = 0; i < quiet_count - 1; i++)
    {
        move = quiet_moves[i];
        histo = &(main_history[color][Move::from(move)][Move::dest(move)]);
        scale_bonus(histo, -depth*depth);

        if (Move::is_ok((info-1)->move))
        {
            histo = &(*(info - 1)->cont_hist)[static_cast<int>(Move::piece(move))][Move::dest(move)];
            scale_bonus(histo, -depth*depth);
        }

        if (Move::is_ok((info-2)->move))
        {
            histo = &(*(info - 2)->cont_hist)[static_cast<U32>(Move::piece(move))][Move::dest(move)];
            scale_bonus(histo, -depth*depth);
        }
    }
}

//=================================================================
//! \brief  Capture History : [moved piece][target square][captured piece type]
//! \param[in] move
//-----------------------------------------------------------------
// I16 History::get_capture_history(MOVE move) const
// {
//     return capture_history[static_cast<U32>(Move::piece(move))][Move::dest(move)][static_cast<U32>(Move::captured_type(move))];
// }

// void History::update_capture_history(MOVE move, I16 bonus)
// {
//     I16& histo = capture_history
//             [static_cast<U32>(Move::piece(move))]
//             [Move::dest(move)]
//             [static_cast<U32>(Move::captured_type(move))];
//     histo += scale_bonus(histo, bonus);
// }

//=====================================================
//  Retourne le bonus
//-----------------------------------------------------
void History::scale_bonus(I16* score, I16 bonus)
{
    // le bonus est contenu dans la zone [-400 , 400]
    auto clamped_bonus = std::clamp(bonus, BONUS_MIN, BONUS_MAX);

    // la nouvelle valeur de l'history est contenue dans la zone [-16384, 16384]
    *score += 32*clamped_bonus - (*score) * abs(clamped_bonus) / 512;
}

