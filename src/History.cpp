#include <cstring>
#include "History.h"
#include "Move.h"
#include "types.h"
#include "Board.h"

//==================================================================
//! \brief  Constructeur
//------------------------------------------------------------------
History::History()
{

}

//==================================================================
//! \brief  Remise à zéro de toutes les tables d'historique
//------------------------------------------------------------------
void History::reset()
{
    std::memset(main_history,           0, sizeof(MainHistoryTable));
    std::memset(pawn_history,           0, sizeof(PawnHistoryTable));
    std::memset(counter_move,           0, sizeof(CounterMoveTable));
    std::memset(continuation_history,   0, sizeof(ContinuationHistoryTable));
    std::memset(capture_history,        0, sizeof(CaptureHistory));
    std::memset(pawn_correction_history,     0, sizeof(PawnCorrectionHistoryTable));
    std::memset(non_pawn_correction_history, 0, N_COLORS*sizeof(NonPawnCorrectionHistoryTable));
}

//==================================================================
//! \brief  Récupère le counter_move
//! \param[in]  info    recherche actuelle
//!
//! \return Coup ayant provoqué un cutoff au coup précédent, MOVE_NONE si aucun
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
//! \param[in]  color   camp qui joue
//! \param[in]  info    recherche actuelle
//! \param[in]  move    coup quiet évalué
//! \param[in]  pawnkey clé de hachage de la structure de pions
//!
//! \return Somme des scores main history, pawn history et continuation history
//--------------------------------------------------------------
int History::get_quiet_history(Color color, const SearchInfo* info, const MOVE move, KEY pawnkey) const
{
    // Main History
    Piece piece  = Move::piece(move);
    SQUARE from  = Move::from(move);
    SQUARE dest  = Move::dest(move);

    int score = main_history[color][BB::test_bit(info->threats, from)][BB::test_bit(info->threats, dest)][from][dest];

    score += pawn_history[pawnkey & PAWNHIST_MASK][piece][dest];

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
//! \param[in]  color       camp qui joue
//! \param[in,out] info     recherche actuelle (killer moves mis à jour)
//! \param[in]  best_move   coup quiet ayant provoqué le cutoff (fail-high)
//! \param[in]  pawnkey     clé de hachage de la structure de pions
//! \param[in]  depth       profondeur de recherche du nœud
//! \param[in]  quiet_count nombre de coups quiets essayés à ce nœud
//! \param[in]  quiet_moves liste des coups quiets essayés à ce nœud
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

//==================================================================
//! \brief  Met à jour la main history
//! \param[in]  color   camp qui joue
//! \param[in]  info    recherche actuelle
//! \param[in]  move    coup quiet concerné
//! \param[in]  bonus   bonus (positif) ou malus (négatif)
//------------------------------------------------------------------
void History::update_main(Color color, SearchInfo* info, MOVE move, int bonus)
{
    const SQUARE from = Move::from(move);
    const SQUARE dest = Move::dest(move);

    gravity(main_history[color][BB::test_bit(info->threats, from)][BB::test_bit(info->threats, dest)][from][dest], bonus);
}

//==================================================================
//! \brief  Met à jour la pawn history
//! \param[in]  pawnkey clé de hachage de la structure de pions
//! \param[in]  move    coup quiet concerné
//! \param[in]  bonus   bonus (positif) ou malus (négatif)
//------------------------------------------------------------------
void History::update_pawn(KEY pawnkey, MOVE move, int bonus)
{
    gravity(pawn_history[pawnkey & PAWNHIST_MASK][Move::piece(move)][Move::dest(move)], bonus);
}

//==================================================================
//! \brief  Met à jour la continuation history (1-ply, 2-ply, 4-ply)
//! \param[in]  info    recherche actuelle
//! \param[in]  move    coup quiet concerné
//! \param[in]  bonus   bonus (positif) ou malus (négatif)
//------------------------------------------------------------------
void History::update_continuation(SearchInfo* info, MOVE move, int bonus)
{
    if (Move::is_ok((info - 1)->move))
        gravity( (*(info - 1)->cont_hist)[Move::piece(move)][Move::dest(move)], bonus);
    if (Move::is_ok((info - 2)->move))
        gravity( (*(info - 2)->cont_hist)[Move::piece(move)][Move::dest(move)], bonus);
    if (Move::is_ok((info - 4)->move))
        gravity( (*(info - 4)->cont_hist)[Move::piece(move)][Move::dest(move)], bonus);
}

//==================================================================
//! \brief  Met à jour la continuation history après LMR
//! Bonus si score >= beta, malus si score <= alpha
//! \param[in]  info    recherche actuelle
//! \param[in]  move    coup concerné
//! \param[in]  score   score retourné par la recherche
//! \param[in]  alpha   borne inférieure de la fenêtre de recherche
//! \param[in]  beta    borne supérieure de la fenêtre de recherche
//! \param[in]  depth   profondeur de recherche du nœud
//------------------------------------------------------------------
void History::update_continuation_history(SearchInfo* info, MOVE move, int score, int alpha, int beta, int depth)
{
    int bonus = score <= alpha ? stat_malus(depth)
              : score >= beta ? stat_bonus(depth)
              : 0;

    update_continuation(info, move, bonus);
}

//=================================================================
//! \brief  Mise à jour de la capture history
//! Bonus pour le coup ayant provoqué un cutoff, malus pour les autres captures essayées
//! \param[in]  info            recherche actuelle
//! \param[in]  best_move       coup de capture ayant provoqué le cutoff (fail-high)
//! \param[in]  depth           profondeur de recherche du nœud
//! \param[in]  capture_count   nombre de coups de capture essayés à ce nœud
//! \param[in]  capture_moves   liste des coups de capture essayés à ce nœud
//-----------------------------------------------------------------
void History::update_capture_history(const SearchInfo *info, MOVE best_move, I16 depth,
                                     size_t capture_count, std::array<MOVE, MAX_MOVES>& capture_moves)
{
    int bonus = stat_bonus(depth);
    int malus = stat_malus(depth);

    // Bonus pour le coup ayant provoqué un cutoff (fail-high)
    update_capture(info, best_move, bonus);

    // Malus pour les autres coups
    for (size_t i = 0; i < capture_count; i++)
    {
        MOVE move = capture_moves[i];
        if (move == best_move)
            continue;
        update_capture(info, move, malus);
    }
}

//==================================================================
//! \brief  Met à jour la capture history pour un coup
//! \param[in]  info    recherche actuelle
//! \param[in]  move    coup de capture concerné
//! \param[in]  delta   bonus (positif) ou malus (négatif)
//------------------------------------------------------------------
void History::update_capture(const SearchInfo *info, MOVE move, int delta)
{
    const SQUARE from = Move::from(move);
    const SQUARE dest = Move::dest(move);
    gravity(capture_history[Move::piece(move)]
                           [BB::test_bit(info->threats, from)]
                           [BB::test_bit(info->threats, dest)]
                           [dest][Move::captured_type(move)], delta);
}

//=================================================================
//! \brief  Mise à jour des tables de correction history (pawn et non-pawn)
//! en fonction de l'écart entre le score de recherche et l'éval statique
//! \param[in]  board       échiquier courant
//! \param[in]  depth       profondeur de recherche du nœud
//! \param[in]  best_score  score retourné par la recherche
//! \param[in]  static_eval éval statique (NNUE) du nœud
//-----------------------------------------------------------------
void History::update_correction_history(const Board& board, int depth, int best_score, int static_eval)
{
    const Color color = board.turn();
    const int eval_diff = best_score - static_eval;

    I16& pawn = pawn_correction_history[color][board.get_pawn_key() & CORRHIST_MASK];
    update_correction(pawn, eval_diff, depth, Tunable::PawnCorrScale, Tunable::PawnCorrMax);

    I16& wmat = non_pawn_correction_history[WHITE][color][board.get_non_pawn_key(WHITE) & CORRHIST_MASK];
    update_correction(wmat, eval_diff, depth, Tunable::NonPawnCorrScale, Tunable::NonPawnCorrMax);

    I16& bmat = non_pawn_correction_history[BLACK][color][board.get_non_pawn_key(BLACK) & CORRHIST_MASK];
    update_correction(bmat, eval_diff, depth, Tunable::NonPawnCorrScale, Tunable::NonPawnCorrMax);
}

//==================================================================
 //! \brief  Met à jour une entrée de correction history (gravity formula).
 //! Le bonus reflète la magnitude de l'écart eval_diff, pondérée par
 //! la profondeur. La gravity (entry * |change| / max_value) sature
 //! naturellement vers ±max_value sans clamp explicite.
 //! \param[in,out] entry           entrée de la table à mettre à jour
 //! \param[in]      eval_diff      écart entre score de recherche et éval statique
 //! \param[in]      depth          profondeur de recherche du nœud
 //! \param[in]      scale          facteur d'échelle du bonus (tunable)
 //! \param[in]      correction_max valeur maximale de correction (tunable)
 //------------------------------------------------------------------
void History::update_correction(I16& entry, int eval_diff, int depth, int scale, int correction_max)
{
    const int eval_scale = MAX_HISTORY / correction_max;
    const int max_value  = correction_max * eval_scale;
    const int bonus      = std::clamp(eval_diff * depth * scale / 64, -max_value / 4, max_value / 4);
    entry += bonus - entry * std::abs(bonus) / max_value;
}

//=========================================================
//! \brief  Retourne la valeur de l'évaluation statique corrigée
//! en fonction de Correction_History
//! \param[in]  board       échiquier courant
//! \param[in]  raw_eval    éval statique (NNUE) brute
//!
//! \return Éval corrigée, clampée à ±(TBWIN_IN_X-1)
//---------------------------------------------------------
int History::corrected_eval(const Board& board, int raw_eval)
{
    // règle des 50 coups : ramener l'évaluation vers 0 quand on s'approche de la nulle
    raw_eval = (raw_eval * (200 - board.get_fiftymove_counter())) / 200;

    const int pawn_eval_scale     = MAX_HISTORY / Tunable::PawnCorrMax;
    const int non_pawn_eval_scale = MAX_HISTORY / Tunable::NonPawnCorrMax;

    int pawn = pawn_correction_history[board.turn()][board.get_pawn_key() & CORRHIST_MASK];
    int wmat = non_pawn_correction_history[WHITE][board.turn()][board.get_non_pawn_key(WHITE) & CORRHIST_MASK];
    int bmat = non_pawn_correction_history[BLACK][board.turn()][board.get_non_pawn_key(BLACK) & CORRHIST_MASK];

    int corrected = raw_eval
                  + pawn / pawn_eval_scale
                  + wmat / non_pawn_eval_scale
                  + bmat / non_pawn_eval_scale;

    return std::clamp(corrected, -TBWIN_IN_X+1, TBWIN_IN_X-1);
}
