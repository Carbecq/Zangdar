#include "Search.h"
#include "MovePicker.h"
#include "Move.h"
#include "TranspositionTable.h"

//=============================================================
//! \brief  Recherche jusqu'à obtenir une position calme,
//!         donc sans prise ou promotion.
//-------------------------------------------------------------
template <Color C>
int Search::quiescence(Board& board, Timer& timer, int alpha, int beta, ThreadData* td, SearchInfo* si)
{
    assert(beta > alpha);

    //  Time-out
    if (td->stopped || timer.check_limits(td->depth, td->index, td->nodes))
    {
        td->stopped = true;
        return 0;
    }

    // Prefetch La table de transposition aussitôt que possible
    transpositionTable.prefetch(board.get_key());

    // Update node count and selective depth
    td->nodes++;
    td->seldepth = std::max(td->seldepth, si->ply);


    // partie nulle ?
    if(board.is_draw(si->ply))
        return CONTEMPT;


    // profondeur de recherche max atteinte
    // prevent overflows
    const bool isInCheck = board.is_in_check();
    if (si->ply >= MAX_PLY)
        return isInCheck ? 0 : board.evaluate();

    const int  old_alpha = alpha;
    const bool isPV      = ((beta - alpha) != 1);

    // Est-ce que la table de transposition est utilisable ?
    int   tt_score = 0;
    int   tt_eval  = VALUE_NONE;
    MOVE  tt_move  = Move::MOVE_NONE;
    int   tt_bound = BOUND_NONE;
    int   tt_depth = 0;
    bool  tt_pv    = false;
    bool  tt_hit   = transpositionTable.probe(board.get_key(), si->ply, tt_move, tt_score, tt_eval, tt_bound, tt_depth, tt_pv);

    // note : on ne teste pas la profondeur, car dans la Quiescence, elle est à 0
    //        dans la cas de la Quiescence, on cut tous les coups, y compris la PV ????
    if (tt_hit && !isPV)    //TODO tester isPV ou non ??
    {
        if (   (tt_bound == BOUND_EXACT)
               || (tt_bound == BOUND_LOWER && tt_score >= beta)
               || (tt_bound == BOUND_UPPER && tt_score <= alpha))
        {
            return tt_score;
        }
    }

    const bool ttPV = isPV || tt_pv;


    // Evaluation statique
    int static_eval;

    if (!isInCheck)
    {
        static_eval = (tt_hit && tt_eval != VALUE_NONE) ?
                    tt_eval : board.evaluate();

        // if (!tt_hit)
        //     transpositionTable.store(board.get_key(), Move::MOVE_NONE, VALUE_NONE, static_eval, BOUND_NONE, 0, si->ply);

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

    //----------------------------------------------------------
    //  Delta Pruning
    //----------------------------------------------------------
    // if (!isInCheck && static_eval + Tunable::DeltaPruningMargin < alpha)
    //     return alpha;


    int  best_score = static_eval;
    MOVE best_move  = Move::MOVE_NONE;  // meilleur coup local
    int  score;
    MOVE move;
    MovePicker movePicker(&board, td->history, si, Move::MOVE_NONE,
                          Move::MOVE_NONE, Move::MOVE_NONE, Move::MOVE_NONE, 0);

    // Boucle sur tous les coups
    // Si on est en échec, on génère aussi les coups "quiet"
    while ((move = movePicker.next_move(!isInCheck).move ) != Move::MOVE_NONE)
    {
        // Prune des prises inintéressantes
        if (!isInCheck && movePicker.get_stage() > STAGE_GOOD_NOISY)
            break;


        /* Delta Pruning, a technique similar in concept to futility pruning,
            only used in the quiescence search.
            It works as follows: before we make a capture, we test whether
            the captured piece value plus some safety margin (typically around 200 centipawns)
            are enough to raise alpha for the current node.
        */

        if (!isInCheck && Move::is_capturing(move))
        {
            if (static_eval + Tunable::DeltaPruningBias + EGPieceValue[static_cast<U32>(Move::captured_type(move))] <= alpha)
            {
                continue;
            }
        }

        board.make_move<C, true>(move);
        si->move = move;
        si->cont_hist = &td->history.continuation_history[static_cast<U32>(Move::piece(move))][Move::dest(move)];
        score = -quiescence<~C>(board, timer, -beta, -alpha, td, si+1);
        board.undo_move<C, true>();

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
                    // transpositionTable.store(board.get_key(), move, score, static_eval, BOUND_LOWER, 0, si->ply);
                    // return score;
                    break;
                }
            }
        }
    }

    if (!td->stopped)
    {
        int bound = best_score >= beta    ? BOUND_LOWER
                                          : best_score > old_alpha ? BOUND_EXACT
                                                                   : BOUND_UPPER;
        transpositionTable.store(board.get_key(), best_move, best_score, static_eval, bound, 0, si->ply, ttPV);
    }

    return best_score;
}

template int Search::quiescence<WHITE>(Board& board, Timer& timer, int alpha, int beta, ThreadData* td, SearchInfo* si);
template int Search::quiescence<BLACK>(Board& board, Timer& timer, int alpha, int beta, ThreadData* td, SearchInfo* si);
