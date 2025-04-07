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
    std::memset(capture_history,       0, sizeof(CaptureHistory));
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

//==============================================================
//! \brief  Retourne la somme de tous les history "quiet"
//--------------------------------------------------------------
int History::get_quiet_history(Color color, const SearchInfo* info, const MOVE move, int& cm_hist, int& fm_hist) const
{
    // Main History
    int score = main_history[color][Move::from(move)][Move::dest(move)];

    if (Move::is_ok((info-1)->move))
    {
        cm_hist =  (*(info - 1)->cont_hist)[static_cast<U32>(Move::piece(move))][Move::dest(move)];
        score += cm_hist;
    }

    if (Move::is_ok((info-2)->move))
    {
        fm_hist =  (*(info - 2)->cont_hist)[static_cast<U32>(Move::piece(move))][Move::dest(move)];
        score += fm_hist;
    }

    return score;
}

void History::update_quiet_history(Color color, SearchInfo* info, MOVE best_move, I16 depth,
                                   int quiet_count, std::array<MOVE, MAX_MOVES>& quiet_moves)
{
    // Counter Move
    if (Move::piece_type(best_move) != PieceType::QUEEN)
    {
        MOVE previous_move = (info-1)->move;

        if (Move::is_ok(previous_move))
            counter_move[static_cast<U32>(Move::piece(previous_move))][Move::dest(previous_move)] = best_move;

        // Killer Moves
        if (info->killer1 != best_move)
        {
            info->killer2 = info->killer1;
            info->killer1 = best_move;
        }
    }

    // credits to ethereal
    // only update quiet history if best move was important
    if (!depth || (depth <= 3 && quiet_count <= 1))
        return;

    MOVE move;
    int bonus = stat_bonus(depth);
    int malus = stat_malus(depth);

    // Bonus pour le coup quiet ayant provoqué un cutoff (fail-high)
    update_main(color, best_move, bonus);
    update_cont(info, best_move, bonus);

    // Malus pour les autres coups quiets
    for (int i = 0; i < quiet_count; i++)
    {
        move = quiet_moves[i];
        if (move != best_move)
        {
            update_main(color, move, malus);
            update_cont(info, move, malus);
        }
    }
}

void History::update_main(Color color, MOVE move, int bonus)
{
    I16* histo = &(main_history[color][Move::from(move)][Move::dest(move)]);
    gravity(histo, bonus);
}

void History::update_cont(SearchInfo* info, MOVE move, int bonus)
{
    for (int delta : {1, 2})    //TODO essayer aussi 4 et 6
    {
        if (Move::is_ok((info - delta)->move))
        {
            I16* histo = &(*(info - delta)->cont_hist)[static_cast<int>(Move::piece(move))][Move::dest(move)];
            gravity(histo, bonus);
        }
    }
}

void History::update_capt(MOVE move, int malus)
{
    I16* histo = &(capture_history[static_cast<U32>(Move::piece(move))][Move::dest(move)][static_cast<U32>(Move::captured_type(move))]);
    gravity(histo, malus);
}

//=================================================================
//! \brief  Capture History : [moved piece][target square][captured piece type]
//! \param[in] move
//-----------------------------------------------------------------
I16 History::get_capture_history(MOVE move) const
{
    return capture_history[static_cast<U32>(Move::piece(move))][Move::dest(move)][static_cast<U32>(Move::captured_type(move))];
}

void History::update_capture_history(MOVE best_move, I16 depth,
                                     int capture_count, std::array<MOVE, MAX_MOVES>& capture_moves)
{
    int bonus = stat_bonus(depth);
    int malus = stat_malus(depth);

    // Bonus pour le coup ayant provoqué un cutoff (fail-high)
    update_capt(best_move, bonus);

    // Malus pour les autres coups
    for (int i = 0; i < capture_count; i++)
    {
        MOVE move = capture_moves[i];
        if (move == best_move)
            continue;
        update_capt(move, malus);
    }
}

//=====================================================
//  Ajoute un bonus à l'historique
//      history gravity
//-----------------------------------------------------
void History::gravity(I16* entry, int bonus)
{
    // Formulation du WIKI :
    // int clamped_bonus = std::clamp(bonus, -MAX_HISTORY, MAX_HISTORY);
    // entry += clamped_bonus - (*entry) * abs(clamped_bonus) / MAX_HISTORY;

    *entry += bonus - *entry * abs(bonus) / MAX_HISTORY;
}

int History::stat_bonus(int depth)
{
    return std::min(MAX_HISTORY_BONUS, HISTORY_MULT*depth - HISTORY_MINUS);

}

int History::stat_malus(int depth)
{
    return -stat_bonus(depth);
}


