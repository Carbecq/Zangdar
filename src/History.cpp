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

    int bonus = stat_bonus(depth);
    int malus = stat_malus(depth);

    // Bonus pour le coup quiet ayant provoqué un cutoff (fail-high)

    // I16* histo = &(main_history[color][Move::from(best_move)][Move::dest(best_move)]);
    // gravity(histo, bonus);

    // if (Move::is_ok((info-1)->move))
    // {
    //     histo = &(*(info - 1)->cont_hist)[static_cast<int>(Move::piece(best_move))][Move::dest(best_move)];
    //     gravity(histo, bonus);
    // }

    // if (Move::is_ok((info-2)->move))
    // {
    //     histo = &(*(info - 2)->cont_hist)[static_cast<U32>(Move::piece(best_move))][Move::dest(best_move)];
    //     gravity(histo, bonus);
    // }

    MOVE move;
    I16* histo;

    // Malus pour les autres coups quiets
    for (int i = 0; i < quiet_count; i++)
    {
        move = quiet_moves[i];
        histo = &(main_history[color][Move::from(move)][Move::dest(move)]);
        gravity(histo, (quiet_moves[i] == best_move) ? bonus  : malus);

        if (Move::is_ok((info-1)->move))
        {
            histo = &(*(info - 1)->cont_hist)[static_cast<int>(Move::piece(move))][Move::dest(move)];
            gravity(histo, (quiet_moves[i] == best_move) ? bonus  : malus);
        }

        if (Move::is_ok((info-2)->move))
        {
            histo = &(*(info - 2)->cont_hist)[static_cast<U32>(Move::piece(move))][Move::dest(move)];
            gravity(histo, (quiet_moves[i] == best_move) ? bonus  : malus);
        }
    }
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
    // Update the history for each capture move that was attempted. One of them
    // might have been the move which produced a cutoff, and thus earn a bonus

    I16* histo;
    MOVE move;
    int bonus = stat_bonus(depth);
    int malus = stat_malus(depth);

    // if (good)
    //     bonus = stat_bonus(depth);
    // else
    //     bonus = stat_malus(depth);

    // histo = &(capture_history[static_cast<U32>(Move::piece(move))][Move::dest(move)][static_cast<U32>(Move::captured_type(move))]);
    // gravity(histo, bonus);

    for (int i = 0; i < capture_count; i++)
    {
        move  = capture_moves[i];
        histo = &(capture_history[static_cast<U32>(Move::piece(move))][Move::dest(move)][static_cast<U32>(Move::captured_type(move))]);
        gravity(histo, (capture_moves[i] == best_move) ? bonus  : malus);
    }
}

//=====================================================
//  Ajoute un bonus à l'historique
//      history gravity
//-----------------------------------------------------
void History::gravity(I16* entry, int bonus)
{
    // int clamped_bonus = std::clamp(bonus, -MAX_HISTORY, MAX_HISTORY);
    // entry += clamped_bonus - (*entry) * abs(clamped_bonus) / MAX_HISTORY;

    *entry += bonus - *entry * abs(bonus) / 16384;
}

int History::stat_bonus(int depth)
{
    // return depth*depth;

    // Ethereal
    // Approximately verbatim stat bonus formula from Stockfish
    return depth > 13 ? 32 : 16 * depth * depth + 128 * std::max(depth - 1, 0);

    // Berserk
    // return std::min(1729, 4*depth*depth + 164*depth - 113);

    // Devre
    // return std::min(400 * depth - 100, 1500);

    // Stormphrax
    // return std::clamp(depth * 174  - 105, -1177, 1177);
}
int History::stat_malus(int depth)
{
    return -stat_bonus(depth);

    // Stormphrax
    // return -std::clamp(depth * 180 - 105, -1177, 1177);
}


