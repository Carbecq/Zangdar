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
    // std::memset(capture_history,       0, sizeof(CaptureHistory));
    // std::memset(counter_move_history,  0, sizeof(CounterMoveHistoryTable));
    // std::memset(followup_move_history, 0, sizeof(FollowupMoveHistoryTable));
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

    return (Move::is_ok(previous_move))
            ? counter_move[static_cast<U32>(Move::piece(previous_move))][Move::dest(previous_move)]
              : Move::MOVE_NONE;
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

//*********************************************************************
int History::get_main_history(Color color, const MOVE move) const
{
    return(main_history[color][Move::from(move)][Move::dest(move)]);
}

//=========================================================
//! \brief  Incrémente l'heuristique "History"
//---------------------------------------------------------
void History::update_main_history(Color color, MOVE move, I16 bonus)
{
    int histo = main_history[color][Move::from(move)][Move::dest(move)];

     // le bonus est contenu dans la zone [-400 , 400]
    bonus = std::clamp(bonus, BONUS_MIN, BONUS_MAX);
     // bonus  = std::max(-400, std::min(400, bonus));

     // la nouvelle valeur de l'history est contenue dans la zone [-16384, 16384]
     histo += 32 * bonus - histo * abs(bonus) / 512;

     // Sauve la nouvelle valeur de l'history
     main_history[color][Move::from(move)][Move::dest(move)] = histo;
}

//==================================================================
//! \brief  Récupère le counter_move history
//! \param[in] ply    ply cherché
//------------------------------------------------------------------
I16 History::get_counter_move_history(const SearchInfo* info, MOVE move) const
{
    // MOVE previous_move = (info-1)->move;

    if (Move::is_ok((info-1)->move))
    {
        return (*(info - 1)->cont_hist)[static_cast<int>(Move::piece(move))][Move::dest(move)];
    }
    else
    {
        return 0;
    }
}
// : continuation_history [static_cast<int>(Move::piece(previous_move))]  [Move::dest(previous_move)][static_cast<int>(Move::piece(move))][Move::dest(move)] );

//=================================================================
//! \brief  Update Counter Move History.
//! \param[in] prev_move    coup recherché au ply précédant
//! \param[in] move         coup de réfutation
//-----------------------------------------------------------------
void History::update_counter_move_history(const SearchInfo*info, MOVE move, I16 bonus)
{

    // Check for root position or null moves
    // if (previous_move == Move::MOVE_NONE || previous_move == Move::MOVE_NULL)
    //     return;

    if (Move::is_ok((info-1)->move))
    {
        // I16* histo = &(*(info - 1)->cont_hist)[static_cast<int>(Move::piece(move))][Move::dest(move)];

        // continuation_history[static_cast<int>(prev_piece)][prev_dest][static_cast<int>(piece)][dest];

        // scale_bonus(histo, bonus);
        int cm_histo;

        bonus = std::clamp(bonus, BONUS_MIN, BONUS_MAX);
        // bonus = std::max(-400, std::min(400, bonus));

        MOVE previous_move = (info-1)->move;
        Piece prev_piece = Move::piece(previous_move);
        int       prev_dest  = Move::dest(previous_move);

        Piece piece = Move::piece(move);
        int       dest  = Move::dest(move);

        cm_histo = continuation_history[static_cast<int>(prev_piece)][prev_dest][static_cast<int>(piece)][dest];
        cm_histo += 32 * bonus - cm_histo * abs(bonus) / 512;
        // // continuation_history[static_cast<int>(prev_piece)][prev_dest][static_cast<int>(piece)][dest] = cm_histo;

        (*(info - 1)->cont_hist)[static_cast<int>(Move::piece(move))][Move::dest(move)] = cm_histo;
    }
}

I16 History::get_followup_move_history(const SearchInfo* info, MOVE move) const
{
    // MOVE folowup_move = (info-2)->move;

    if (Move::is_ok((info-2)->move))
    {
        return (*(info - 2)->cont_hist)[static_cast<int>(Move::piece(move))][Move::dest(move)];
    }
    else
    {
        return 0;
    }
    // : continuation_history[static_cast<int>(Move::piece(folowup_move))][Move::dest(folowup_move)][static_cast<int>(Move::piece(move))][Move::dest(move)] );
}

//=================================================================
//! \brief  Update Counter Move History.
//! \param[in] prev_move    coup recherché au ply précédant
//! \param[in] move         coup de réfutation
//-----------------------------------------------------------------
void History::update_followup_move_history(const SearchInfo*info, MOVE move, I16 bonus)
{

    // Check for root position or null moves
    // if (followup_move == Move::MOVE_NONE || followup_move == Move::MOVE_NULL)
    //     return;

    if (Move::is_ok((info-2)->move))
    {
        // I16* histo = &(*(info - 2)->cont_hist)[static_cast<int>(Move::piece(move))][Move::dest(move)];

        // continuation_history[static_cast<int>(prev_piece)][prev_dest][static_cast<int>(piece)][dest];

        // scale_bonus(histo, bonus);

        int fm_histo;

        // bonus = std::max(-400, std::min(400, bonus));
        bonus = std::clamp(bonus, BONUS_MIN, BONUS_MAX);
        MOVE followup_move = (info-2)->move;

        Piece followup_piece = Move::piece(followup_move);
        int       followup_dest  = Move::dest(followup_move);

        Piece piece = Move::piece(move);
        int       dest  = Move::dest(move);

        fm_histo = continuation_history[static_cast<int>(followup_piece)][followup_dest][static_cast<int>(piece)][dest];
        fm_histo += 32 * bonus - fm_histo * abs(bonus) / 512;
        // continuation_history[static_cast<int>(followup_piece)][followup_dest][static_cast<int>(piece)][dest] = fm_histo;
        (*(info - 2)->cont_hist)[static_cast<int>(Move::piece(move))][Move::dest(move)] = fm_histo;
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


//==============================================================
//! \brief  Retourne la somme de tous les history "quiet"
//--------------------------------------------------------------
// I16 History::get_quiet_history(Color color, const SearchInfo* info, const MOVE move) const
// {
//     // Main History
//     I16 score = main_history[color][Move::from(move)][Move::dest(move)];

//     // Continuation History
//     const int  delta[2] = {1, 2};   //TODO utiliser aussi info-4 ??

//     for (int n=0; n<2; n++)
//     {
//         int d = delta[n];
//         if (Move::is_ok((info - d)->move))
//             score += *(info - d)->continuation_history[static_cast<U32>(Move::piece(move))][Move::dest(move)];
//     }

//     return score;
// }

// void History::update_quiet_history(Color color, const SearchInfo* info, MOVE move, I16 bonus)
// {
//     // valeur actuelle du Main History
//     I16& mh = main_history[color][Move::from(move)][Move::dest(move)];
//     mh += scale_bonus(mh, bonus);

//     //TODO : vérifier le bonus

//     const int  delta[2] = {1, 2};

//     for (int n=0; n<2; n++)
//     {
//         int d = delta[n];
//         if (Move::is_ok((info - d)->move))
//         {
//             I16& histo = (*(info - d)->continuation_history)[static_cast<U32>(Move::piece(move))][Move::dest(move)];
//             histo += scale_bonus(histo, bonus);
//         }
//     }

// }

//=====================================================
//  Retourne le bonus
//-----------------------------------------------------
I16 History::scale_bonus(I16 &score, I16 bonus)
{
    // le bonus est contenu dans la zone [-400 , 400]
    auto clamped_bonus = std::clamp(bonus, BONUS_MIN, BONUS_MAX);

    // la nouvelle valeur de l'history est contenue dans la zone [-16384, 16384]
    return 32*clamped_bonus - (score) * abs(clamped_bonus) / 512;
}
