#include "Search.h"
#include "MovePicker.h"
#include "Move.h"

#include "Bitboard.h"


//=============================================================
//! \brief  Recherche jusqu'à obtenir une position calme,
//!         donc sans prise ou promotion.
//-------------------------------------------------------------
template <Color C>
int Search::quiescence(int alpha, int beta, ThreadData* td, SearchInfo* si)
{
    assert(beta > alpha);

    //  Time-out
    if (td->stopped || check_limits(td))
    {
        td->stopped = true;
        return 0;
    }


    // Update node count and selective depth
    td->nodes++;
    if (si->ply > td->seldepth)
        td->seldepth = si->ply;


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
   // int   tt_flag;
   // int   tt_depth;
   // bool  tt_hit   = transpositionTable.probe(board.hash, ply, tt_move, tt_score, tt_eval, tt_flag, tt_depth);

   // // note : on ne teste pas la profondeur, car dasn la Quiescence, elle est à 0
   // if (tt_hit)
   // {
   //     if (   (tt_flag == BOUND_EXACT)
   //         || (tt_flag == BOUND_LOWER && tt_score >= beta)
   //         || (tt_flag == BOUND_UPPER && tt_score <= alpha))
   //         return tt_score;
   // }


    int  best_score;
    int  score;

    // stand pat

    if (!in_check)
    {
        // you do not allow the side to move to stand pat if the side to move is in check.
        best_score = board.evaluate();

        // le score est trop mauvais pour moi, on n'a pas besoin
        // de chercher plus loin
        if (best_score >= beta)
            return best_score;

        // l'évaluation est meilleure que alpha. Donc on peut améliorer
        // notre position. On continue à chercher.
        if (best_score > alpha)
            alpha = best_score;
    }
    else
    {
        best_score = -MATE + si->ply; // idée de Koivisto
    }
    int eval = best_score;
    
    MOVE move;
    MLMove mlm;
    MovePicker movePicker(&board, td->history, Move::MOVE_NONE,
                          Move::MOVE_NONE, Move::MOVE_NONE, Move::MOVE_NONE,
                          true, 0);

    // Boucle sur tous les coups
    while (true)
    {
        mlm = movePicker.next_move();
        if (mlm.move == Move::MOVE_NONE)
                break;
        move = mlm.move;

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
                if (eval + 300 + EGPieceValue[Move::captured(move)] <= alpha)
                {
                    continue;
                }
            }
        }


        // https://www.chessprogramming.org/CPW-Engine_quiescence
        // if ((stand_pat + e.PIECE_VALUE[movelist[i].piece_cap] + 200 < alpha) &&
        //     (b.PieceMaterial[!b.stm] - e.PIECE_VALUE[movelist[i].piece_cap] > e.ENDGAME_MAT) &&
        //     (!move_isprom(movelist[i])))
        //     continue;

        board.make_move<C>(move);
        si->move = move;
        score = -quiescence<~C>(-beta, -alpha, td, si+1);
        board.undo_move<C>();

        if (td->stopped)
            return 0;

        // try for an early cutoff:
        if(score >= beta)
        {
            /* we have a cutoff, so update our killers: */
            // if (Move::is_capturing(move) == false)
            // {
            //     si->update_killers(ply, move);
            //     si->update_counter(C, ply, move);
            // }
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
