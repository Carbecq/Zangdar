#include <cstring>
#include "History.h"
#include "Move.h"
#include "types.h"
#include "Board.h"

History::History()
{

}

//*********************************************************************
void History::reset()
{
    std::memset(main_history,           0, sizeof(MainHistoryTable));
    std::memset(pawn_history,           0, sizeof(PawnHistoryTable));
    std::memset(counter_move,           0, sizeof(CounterMoveTable));
    std::memset(continuation_history,   0, sizeof(ContinuationHistoryTable));
    std::memset(capture_history,        0, sizeof(CaptureHistory));
    std::memset(pawn_correction_history,     0, sizeof(PawnCorrectionHistoryTable));
    std::memset(material_correction_history, 0, N_COLORS*sizeof(MaterialCorrectionHistoryTable));
}

//==================================================================
//! \brief  Récupère le counter_move
//! \param[in] info         recherche actuelle
//------------------------------------------------------------------
MOVE History::get_counter_move(const SearchInfo* info) const
{
    MOVE previous_move = (info-1)->move;

    return (Move::is_ok(previous_move))
            ? counter_move[Move::piece(previous_move)][Move::dest(previous_move)]
              : Move::MOVE_NONE;
}

//==============================================================
//! \brief  Retourne la somme de tous les history "quiet"
//! Utilisé pour la recherche
//--------------------------------------------------------------
int History::get_quiet_history(Color color, const SearchInfo* info, const MOVE move, KEY pawnkey) const
{
    // Main History
    Piece piece  = Move::piece(move);
    SQUARE from  = Move::from(move);
    SQUARE dest  = Move::dest(move);

    int score = main_history[color][BB::test_bit(info->threats, from)][BB::test_bit(info->threats, dest)][from][dest];

    score += pawn_history[get_index(pawnkey)][piece][dest];

    /*
     * Continuation History

        Continuation History is a generalization of Counter Moves History and Follow Up History.
        An n-ply Continuation History is the history score indexed by the move played
        n-ply ago and the current move.
        1-ply and 2-ply continuation histories are most popular and correspond
        to Counter Moves History and Follow Up History respectively.
        Many programs, notably Stockfish, also makes use of 3, 4, 5, and 6-ply continuation histories.
     */

    if (Move::is_ok((info-1)->move))
        score += (*(info - 1)->cont_hist)[piece][dest];

    if (Move::is_ok((info-2)->move))
        score +=  (*(info - 2)->cont_hist)[piece][dest];

    if (Move::is_ok((info-4)->move))
        score += (*(info - 4)->cont_hist)[piece][dest];

    return score;
}

//==============================================================
//! \brief  Mise à jour de tous les history "quiet"
//--------------------------------------------------------------
void History::update_quiet_history(Color color, SearchInfo* info, MOVE best_move, KEY pawnkey, I16 depth,
                                   size_t quiet_count, std::array<MOVE, MAX_MOVES>& quiet_moves)
{
    // Counter Move
    if (Move::piece_type(best_move) != PieceType::QUEEN)
    {
        MOVE previous_move = (info-1)->move;

        if (Move::is_ok(previous_move))
            counter_move[Move::piece(previous_move)][Move::dest(previous_move)] = best_move;

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
    update_main(color, info, best_move, bonus);
    update_pawn(pawnkey, best_move, bonus);
    update_continuation(info, best_move, bonus);

    // Malus pour les autres coups quiets
    for (size_t i = 0; i < quiet_count; i++)
    {
        move = quiet_moves[i];
        if (move != best_move)
        {
            update_main(color, info, move, malus);
            update_pawn(pawnkey, move,  malus);
            update_continuation(info, move,  malus);
        }
    }
}

void History::update_main(Color color, SearchInfo* info, MOVE move, int bonus)
{
    const SQUARE from = Move::from(move);
    const SQUARE dest = Move::dest(move);

    gravity(main_history[color][BB::test_bit(info->threats, from)][BB::test_bit(info->threats, dest)][from][dest], bonus);
}

void History::update_pawn(KEY pawnkey, MOVE move, int bonus)
{
    gravity(pawn_history[get_index(pawnkey)][Move::piece(move)][Move::dest(move)], bonus);
}

void History::update_continuation(SearchInfo* info, MOVE move, int bonus)
{
    if (Move::is_ok((info - 1)->move))
        gravity( (*(info - 1)->cont_hist)[Move::piece(move)][Move::dest(move)], bonus);
    if (Move::is_ok((info - 2)->move))
        gravity( (*(info - 2)->cont_hist)[Move::piece(move)][Move::dest(move)], bonus);
    if (Move::is_ok((info - 4)->move))
        gravity( (*(info - 4)->cont_hist)[Move::piece(move)][Move::dest(move)], bonus);
}

void History::update_continuation_history(SearchInfo* info, MOVE move, int score, int alpha, int beta, int depth)
{
    int bonus = score <= alpha ? stat_malus(depth)
              : score >= beta ? stat_bonus(depth)
              : 0;

    update_continuation(info, move, bonus);
}

//=================================================================
//! \brief  Capture History : [moved piece][target square][captured piece type]
//! \param[in] move
//-----------------------------------------------------------------
void History::update_capture_history(MOVE best_move, I16 depth,
                                     size_t capture_count, std::array<MOVE, MAX_MOVES>& capture_moves)
{
    int bonus = stat_bonus(depth);
    int malus = stat_malus(depth);

    // Bonus pour le coup ayant provoqué un cutoff (fail-high)
    update_capture(best_move, bonus);

    // Malus pour les autres coups
    for (size_t i = 0; i < capture_count; i++)
    {
        MOVE move = capture_moves[i];
        if (move == best_move)
            continue;
        update_capture(move, malus);
    }
}

void History::update_capture(MOVE move, int malus)
{
    gravity(capture_history[Move::piece(move)][Move::dest(move)][Move::captured_type(move)], malus);
}

//=================================================================
//! \brief  Correction History : [color][pawn_key_index]
//! \param[in] move
//-----------------------------------------------------------------
void History::update_correction_history(const Board& board, int depth, int best_score, int static_eval)
{
    const Color color = board.turn();

    // Compute the new correction value, and retrieve the old value
    const int scaled_bonus = (best_score - static_eval) * CorrectionHistoryScale;

    // Weight the new value based on the search depth, and the old value based on the remaining weight
    const int weight = std::min(depth + 1, 16);

    int& pawn = pawn_correction_history[color][board.get_pawn_key() % PAWN_HASH_SIZE];
    update_correction(pawn, scaled_bonus, weight);

    int& wmat = material_correction_history[WHITE][color][board.get_mat_key(WHITE) % PAWN_HASH_SIZE];
    update_correction(wmat, scaled_bonus, weight);

    int& bmat = material_correction_history[BLACK][color][board.get_mat_key(BLACK) % PAWN_HASH_SIZE];
    update_correction(bmat, scaled_bonus, weight);
}

void History::update_correction(int& entry, int scaled_bonus, int weight)
{
    // Compute the weighted sum of the old and new values, and clamp the result.

    entry = (entry * (CorrectionHistoryScale - weight) + scaled_bonus * weight) / CorrectionHistoryScale;
    entry = std::clamp(entry, -CorrectionHistoryMax, CorrectionHistoryMax);
}

//=========================================================
//! \brief  Retourne la valeur de l'évaluation statique corrigée
//! en fonction de Correction_History
//!
//---------------------------------------------------------
int History::corrected_eval(const Board& board, int raw_eval)
{
    // règle des 50 coups : ramener l'évaluation vers 0 quand on s'approche de la nulle
    raw_eval = (raw_eval * (200 - board.get_fiftymove_counter())) / 200;

    int pawn = pawn_correction_history[board.turn()][board.get_pawn_key() % PAWN_HASH_SIZE];
    int wmat = material_correction_history[WHITE][board.turn()][board.get_mat_key(WHITE) % PAWN_HASH_SIZE];
    int bmat = material_correction_history[BLACK][board.turn()][board.get_mat_key(BLACK) % PAWN_HASH_SIZE];

    int corrected = raw_eval + (pawn + wmat + bmat) / CorrectionHistoryScale;

    return std::clamp(corrected, -TBWIN_IN_X+1, TBWIN_IN_X-1);
}
