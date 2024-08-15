#include "Search.h"
#include "MovePicker.h"
#include "Move.h"
#include "Bitboard.h"
//#include "TranspositionTable.h"

//=============================================================
//! \brief  Recherche jusqu'à obtenir une position calme,
//!         donc sans prise ou promotion.
//-------------------------------------------------------------
template <Color C>
int Search::quiescence(int alpha, int beta, ThreadData* td, SearchInfo* si)
{
    assert(beta > alpha);

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
    bool in_check = board.is_in_check<C>();
    if (si->ply >= MAX_PLY - 1)
        return in_check ? 0 : board.evaluate();


    // partie trop longue
    if (board.gamemove_counter >= MAX_HIST - 1)
        return board.evaluate();


    // Est-ce que la table de transposition est utilisable ?
   // Score tt_score;
   // Score tt_eval;
   // MOVE  tt_move  = Move::MOVE_NONE;
   // int   tt_bound;
   // int   tt_depth;
   // bool  tt_hit   = transpositionTable.probe(board.hash, ply, tt_move, tt_score, tt_eval, tt_bound, tt_depth);

   // // note : on ne teste pas la profondeur, car dasn la Quiescence, elle est à 0
   // if (tt_hit)
   // {
   //     if (   (tt_bound == BOUND_EXACT)
   //         || (tt_bound == BOUND_LOWER && tt_score >= beta)
   //         || (tt_bound == BOUND_UPPER && tt_score <= alpha))
   //         return tt_score;
   // }

    int static_eval;

    // stand pat
    if (!in_check)
    {
        // you do not allow the side to move to stand pat if the side to move is in check.
        static_eval = board.evaluate();

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

    int     best_score = static_eval;
    int     score;
    MOVE    move;
    MovePicker movePicker(&board, td, si->ply, Move::MOVE_NONE,
                          Move::MOVE_NONE, Move::MOVE_NONE, Move::MOVE_NONE, 0);

    // Boucle sur tous les coups
    while ((move = movePicker.next_move(true).move ) != Move::MOVE_NONE)
    {
        // Prune des prises inintéressantes
        if (!in_check && movePicker.get_stage() > STAGE_GOOD_NOISY)
            break;


        // Delta Pruning
        /*****************************************************************
    *  Delta cutoff - a move guarentees the score well below alpha,  *
    *  so  there's no point in searching it. This heuristic is  not  *
    *  used  in the endgame, because of the  insufficient  material  *
    *  issues and special endgame evaluation heuristics.             *
    *****************************************************************/

        if (!in_check)
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

        // try for an early cutoff:
        if(score >= beta)
        {
            return score;
        }

        // Found a new best move in this position
        if (score > best_score)
        {
            best_score = score;

            // If score beats alpha we update alpha
            if (score > alpha)
                alpha = score;
        }
    }

    return best_score;
}

template int Search::quiescence<WHITE>(int alpha, int beta, ThreadData* td, SearchInfo* si);
template int Search::quiescence<BLACK>(int alpha, int beta, ThreadData* td, SearchInfo* si);
