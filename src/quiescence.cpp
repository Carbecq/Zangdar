#include "Search.h"
#include "MovePicker.h"
#include "Move.h"
#include "Bitboard.h"
#include "TranspositionTable.h"

//=============================================================
//! \brief  Recherche jusqu'à obtenir une position calme,
//!         donc sans prise ou promotion.
//-------------------------------------------------------------
template <Color C>
int Search::quiescence(int alpha, int beta, ThreadData* td, SearchInfo* si)
{
    assert(beta > alpha);

    // Prefetch La table de transposition aussitôt que possible
    transpositionTable.prefetch(board.get_hash());


    // Update node count and selective depth
    td->nodes++;
    td->seldepth = std::max(td->seldepth, si->ply);


    //  Time-out
    if (td->stopped || check_limits(td))
    {
        td->stopped = true;
        return 0;
    }


    // partie nulle ?
    if(board.is_draw(si->ply))
        return CONTEMPT;


    // profondeur de recherche max atteinte
    // prevent overflows
    bool isInCheck = board.is_in_check<C>();
    if (si->ply >= MAX_PLY)
        return isInCheck ? 0 : board.evaluate();


    // Est-ce que la table de transposition est utilisable ?
    int old_alpha = alpha;
    Score tt_score;
    Score tt_eval;
    MOVE  tt_move  = Move::MOVE_NONE;
    int   tt_bound;
    int   tt_depth;
    bool  tt_hit   = transpositionTable.probe(board.hash, si->ply, tt_move, tt_score, tt_eval, tt_bound, tt_depth);

    // note : on ne teste pas la profondeur, car dans la Quiescence, elle est à 0
    //        dans la cas de la Quiescence, on cut tous les coups, y compris la PV
    if (tt_hit)
    {
        if (   (tt_bound == BOUND_EXACT)
            || (tt_bound == BOUND_LOWER && tt_score >= beta)
            || (tt_bound == BOUND_UPPER && tt_score <= alpha))
        {
            return tt_score;
        }
    }

    // Evaluation statique
    int static_eval;

    if (!isInCheck)
    {
        // you do not allow the side to move to stand pat if the side to move is in check.
        static_eval = si->eval = tt_eval != NOSCORE
                               ? tt_eval : board.evaluate();

        // le score est trop mauvais pour moi, on n'a pas besoin
        // de chercher plus loin
        if (static_eval >= beta)
            return static_eval;

        // l'évaluation est meilleure que alpha. Donc on peut améliorer
        // notre position. On continue à chercher.
        if (static_eval > alpha)
            alpha = static_eval;
    }
    else
    {
        static_eval = -MATE + si->ply; // idée de Koivisto
    }


    int  best_score = static_eval;
    MOVE best_move  = Move::MOVE_NONE;  // meilleur coup local
    int  score;
    MOVE move;
    MovePicker movePicker(&board, td, si->ply, Move::MOVE_NONE,
                          Move::MOVE_NONE, Move::MOVE_NONE, Move::MOVE_NONE, 0);

    // Boucle sur tous les coups
    while ((move = movePicker.next_move(true).move ) != Move::MOVE_NONE)
    {
        // Prune des prises inintéressantes
        if (!isInCheck && movePicker.get_stage() > STAGE_GOOD_NOISY)
            break;


        // Delta Pruning
        /*****************************************************************
    *  Delta cutoff - a move guarentees the score well below alpha,  *
    *  so  there's no point in searching it. This heuristic is  not  *
    *  used  in the endgame, because of the  insufficient  material  *
    *  issues and special endgame evaluation heuristics.             *
    *****************************************************************/

        if (!isInCheck)
        {
            if (Move::is_capturing(move))
            {
                if (static_eval + 300 + EGPieceValue[Move::captured(move)] <= alpha)
                {
                    continue;
                }
            }
        }

        board.make_move<C>(move);
        si->move = move;
        score = -quiescence<~C>(-beta, -alpha, td, si+1);
        board.undo_move<C>();

        if (td->stopped)
            return 0;

        // Found a new best move in this position
        if (score > best_score)
        {
            best_score = score;
            best_move  = move;

            // If score beats alpha we update alpha
            if (score > alpha)
            {
                alpha = score;

                // try for an early cutoff:
                if(score >= beta)
                {

                    transpositionTable.store(board.hash, move, score, static_eval, BOUND_LOWER, 0, si->ply);
                    return score;
                }
            }
        }
    }

    if (!td->stopped)
    {
        int flag = (alpha != old_alpha) ? BOUND_EXACT : BOUND_UPPER;
        transpositionTable.store(board.hash, best_move, best_score, static_eval, flag, 0, si->ply);
    }

    return best_score;
}

template int Search::quiescence<WHITE>(int alpha, int beta, ThreadData* td, SearchInfo* si);
template int Search::quiescence<BLACK>(int alpha, int beta, ThreadData* td, SearchInfo* si);
