#include "MovePicker.h"
#include "Search.h"
#include "ThreadPool.h"
#include "Timer.h"
#include "TranspositionTable.h"
#include "Move.h"

#define CLAMP(x, low, high)  (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))


//======================================================
//! \brief  Lancement d'une recherche
//!
//! itérative deepening
//! alpha-beta
//!
//------------------------------------------------------
template<Color C>
void Search::think(const Board &m_board, const Timer &m_timer, int m_index)
{
#if defined DEBUG_LOG
    char message[100];
    sprintf(message, "Search::think (thread=%d)", _index);
    printlog(message);
#endif

    board = m_board;
    timer = m_timer;

    ThreadData* td = &threadPool.threadData[m_index];
    SearchInfo* si = &td->info[STACK_OFFSET];
/*
    0   4                                     131 135
    +---|--------------------------------------+---+        136 éléments total (128 + 2*4)

    +---+--------------------------------------+---+
        0                                     127 131       ply
 */

    for (int i = 0; i < MAX_PLY+STACK_OFFSET; i++)
        (si + i)->ply = i;

    // for (int i=0; i<STACK_SIZE; i++)
    //     td->info[i].eval = NOSCORE;  // nécessaire pour "improving" ???

    // iterative deepening
    iterative_deepening<C>(td, si);

    if (m_index == 0)
    {
        if (threadPool.get_logUci())
        {
            show_uci_best(td);
            timer.show_time();
        }

        threadPool.main_thread_stopped(); // arrêt des autres threads
        threadPool.wait(1);               // attente de ces threads
    }
    //  Transtable.stats();
}

//======================================================
//! \brief  iterative deepening
//!
//! alpha-beta
//!
//------------------------------------------------------
template<Color C>
void Search::iterative_deepening(ThreadData* td, SearchInfo* si)
{
    si->pv.length = 0;

    for (td->depth = 1; td->depth <= timer.getSearchDepth(); td->depth++)
    {
        // Search position, using aspiration windows for higher depths
        td->score = aspiration_window<C>(td, si);

        if (td->stopped)
            break;

        // L'itération s'est terminée sans problème
        // On peut mettre à jour les infos UCI
        if (td->index == 0)
        {
            bool uncertain = si->pv.line[0] != td->best_move;

            td->best_depth = td->depth;
            td->best_move  = si->pv.line[0];
            td->best_score = td->score;

            auto elapsed = timer.elapsedTime();

            if (threadPool.get_logUci())
                show_uci_result(td, elapsed, si->pv);

            // If an iteration finishes after optimal time usage, stop the search
            if (timer.finishOnThisDepth(elapsed, uncertain))
                break;

            td->seldepth = 0;
        }
    }
}

//======================================================
//! \brief  Aspiration Window
//!
//!
//------------------------------------------------------
template<Color C>
int Search::aspiration_window(ThreadData* td, SearchInfo* si)
{
    int alpha  = -INFINITE;
    int beta   = INFINITE;
    int depth  = td->depth;
    int score  = td->score;

    const int initialWindow = 12;
    int delta = 16;

    // After a few depths use a previous result to form the window
    if (depth >= 6)
    {
        alpha = std::max(score - initialWindow, -INFINITE);
        beta  = std::min(score + initialWindow, INFINITE);
    }

    while (true)
    {
        score = alpha_beta<C>(alpha, beta, std::max(1, depth), td, si);

        if (td->stopped)
            break;

        // Search failed low, adjust window and reset depth
        if (score <= alpha)
        {
            alpha = std::max(score - delta, -INFINITE); // alpha/score-delta
            beta  = (alpha + beta) / 2;
            depth = td->depth;
        }

        // Search failed high, adjust window and reduce depth
        else if(score >= beta)  // Fail High
        {
            beta  = std::min(score + delta, INFINITE);   // beta/score+delta
            // idée de Berserk
            if (abs(score) < TBWIN_IN_X)
                depth--;
        }

        // Score within the bounds is accepted as correct
        else
            return score;

        delta += delta*2 / 3;
    }

    return score;
}

//=====================================================
//! \brief  Recherche du meilleur coup
//!
//!     Méthode Apha-Beta - Fail Soft
//!
//! \param[in]  ply     profondeur actuelle de recherche
//! \param[in]  alpha   borne inférieure : on est garanti d'avoir au moins alpha
//! \param[in]  beta    borne supérieure : meilleur coup pour l'adversaire
//! \param[in]  depth   profondeur maximum de recherche
//! \param[out] pv      tableau de stockage de la Variation Principale
//!
//! \return Valeur du score
//-----------------------------------------------------
template<Color C>
int Search::alpha_beta(int alpha, int beta, int depth, ThreadData* td, SearchInfo* si)
{
    assert(board.valid());
    assert(beta > alpha);
    
    MOVE quietsTried[MAX_MOVES];
    int  quietsPlayed = 0;

    // Update node count and selective depth
    td->nodes++;
    if (si->ply > td->seldepth)
        td->seldepth = si->ply;

    si->pv.length     = 0;

    // On a atteint la limite en profondeur de recherche ?
    if (si->ply >= MAX_PLY - 1)
        return board.evaluate();

    // On a atteint la limite de taille de la partie ?
    if (board.gamemove_counter >= MAX_HIST - 1)
        return board.evaluate();

    //  Time-out
    if (td->stopped || check_limits(td))
    {
        td->stopped = true;
        return 0;
    }

    //  Caractéristiques de la position
    bool isRoot   = (si->ply == 0);
    bool isPVNode = ((beta - alpha) != 1); // We are in a PV-node if we aren't in a null window.
    bool inCheck  = board.is_in_check<C>();
    int  best_score = -INFINITE;        // initially assume the worst case
    MOVE best_move  = Move::MOVE_NONE;  // meilleur coup local
    int  max_score  = INFINITE;         // best possible
    MOVE excluded   = si->excluded;

    //  Check Extension
    if (inCheck == true && depth + 1 < MAX_PLY)
        depth++;

    /* On a atteint la fin de la recherche
        Certains codes n'appellent la quiescence que si on n'est pas en échec
        ceci amène à une explosion des coups recherchés */
    if (depth <= 0)
    {
        td->nodes--;
        return (quiescence<C>(alpha, beta, td, si));
    }

    if (!isRoot)
    {
        //  Position nulle ?
        if (board.is_draw(si->ply))
            return CONTEMPT;

        // Mate distance pruning
        alpha = std::max(alpha, -MATE + si->ply);
        beta  = std::min(beta,   MATE - si->ply - 1);
        if (alpha >= beta)
            return alpha;
    }

    //  Recherche de la position actuelle dans la table de transposition
    Score tt_score = NOSCORE;
    Score tt_eval  = NOSCORE;
    MOVE  tt_move  = Move::MOVE_NONE;
    int   tt_bound = BOUND_NONE;
    int   tt_depth = -1;
    bool  tt_hit   = excluded ? false : transpositionTable.probe(board.hash, si->ply, tt_move, tt_score, tt_eval, tt_bound, tt_depth);

    if (tt_hit)
    {
        // Trust TT if not a pvnode and the entry depth is sufficiently high
        if (   (tt_depth >= depth && !isPVNode)
            && (   (tt_bound == BOUND_EXACT)
                || (tt_bound == BOUND_LOWER && tt_score >= beta)
                || (tt_bound == BOUND_UPPER && tt_score <= alpha)))
        {
            return tt_score;
        }
    }

    // Probe the Syzygy     Tablebases
    int tbScore, tbBound;
    if (threadPool.get_useSyzygy() && board.probe_wdl(tbScore, tbBound, si->ply) == true)
    {
        td->tbhits++;

        // Check to see if the WDL value would cause a cutoff
        if (    tbBound == BOUND_EXACT
            || (tbBound == BOUND_LOWER && tbScore >= beta)
            || (tbBound == BOUND_UPPER && tbScore <= alpha))
        {
            transpositionTable.store(board.hash, Move::MOVE_NONE, tbScore, NOSCORE, tbBound, depth, MAX_PLY);
            return tbScore;
        }

        // Limit the score of this node based on the tb result
        if (isPVNode)
        {
            // Never score something worse than the known Syzygy value
            if (tbBound == BOUND_LOWER)
            {
                best_score = tbScore;
                alpha = std::max(alpha, tbScore);
            }

            // Never score something better than the known Syzygy value
            if (tbBound == BOUND_UPPER)
            {
                max_score = tbScore;
            }
        }
    }


    //  Evaluation Statique
    int static_eval;

    if (inCheck)
    {
        si->eval = static_eval = -MATE + si->ply;
    }
    else if (excluded)
    {
        static_eval = si->eval;
    }
    else
    {
        if (tt_eval != NOSCORE)
            si->eval = static_eval = tt_eval;
        else
            si->eval = static_eval = board.evaluate();
    }


    /*  Avons-nous amélioré la position ?
        Si on ne s'est pas amélioré dans cette ligne, on va pouvoir couper un peu plus */
    bool improving = false;
    if (si->ply > 2)
        improving = !inCheck && (static_eval > (si-2)->eval);

    //  Controle si on va pouvoir utiliser des techniques de coupe pre-move
    int  score;

    if (!inCheck && !isRoot && !isPVNode && !si->excluded)
    {
#if defined USE_RAZORING
        //---------------------------------------------------------------------
        //  RAZORING
        //---------------------------------------------------------------------
        if (   depth <= 3
            && (static_eval + 200 * depth) <= alpha)
        {
            score = quiescence<C>(alpha, beta, td, si);
            if (score <= alpha)
            {
                td->nodes--;
                return score;
            }
        }
#endif

#if defined USE_REVERSE_FUTILITY_PRUNING
        //---------------------------------------------------------------------
        //  STATIC NULL MOVE PRUNING ou aussi REVERSE FUTILITY PRUNING
        //---------------------------------------------------------------------
        if (
            depth <= 6
            && abs(beta) < MATE_IN_X
            && board.getNonPawnMaterial<C>())
        {
            int eval_margin = 70 * depth;

            if (static_eval - eval_margin >= beta)
                return static_eval - eval_margin; // Fail Soft
        }
#endif

#if defined USE_NULL_MOVE_PRUNING
        //---------------------------------------------------------------------
        //  NULL MOVE PRUNING
        //---------------------------------------------------------------------
        if (
            depth >= 3
            && static_eval >= beta
            && (si-1)->move != Move::MOVE_NULL  //TODO vérifier move
            && (si-2)->move != Move::MOVE_NULL
            && !excluded
            && board.non_pawn_count<C>() > 0)
        {
            int R = 3 + depth / 5 + std::min(3, (static_eval - beta)/256); //ZZZEVAL

            board.make_nullmove<C>();
            si->move = Move::MOVE_NULL;
            score = -alpha_beta<~C>(-beta, -beta + 1, depth - 1 - R, td, si+1);
            board.undo_nullmove<C>();

            if (td->stopped)
                return 0;

            // Cutoff
            if (score >= beta)
            {
                // (Stockfish) : on ne retourne pas un score proche du mat
                //               car ce score ne serait pas prouvé
                return(score >= TBWIN_IN_X ? beta : score);
            }
        }
#endif

#if defined USE_PROBCUT
        //---------------------------------------------------------------------
        //  ProbCut
        //---------------------------------------------------------------------
        if (depth >= 5
            && abs(beta) < TBWIN_IN_X
            && !(   tt_hit
                 && tt_bound == BOUND_UPPER
                 && tt_score < beta))
        {
            int threshold = beta + 200;

            MovePicker movePicker(&board, td->history, Move::MOVE_NONE, Move::MOVE_NONE, Move::MOVE_NONE, Move::MOVE_NONE, true, 0);
            MOVE pbMove;

            while ( (pbMove = movePicker.next_move().move ) != Move::MOVE_NONE )
            {
                board.make_move<C>(pbMove);
                si->move = pbMove;

                // See if a quiescence search beats pbBeta
                int pbScore = -quiescence<~C>(-threshold, -threshold+1, td, si+1);

                // If it did, do a proper search with reduced depth
                if (pbScore >= threshold)
                    pbScore = -alpha_beta<~C>(-threshold, -threshold + 1, depth-4, td, si+1);

                board.undo_move<C>();

                // Cut if the reduced depth search beats pbBeta
                if (pbScore >= threshold)
                {
                    // Store pbScore in TT ??
                    return pbScore;
                }
            }
        }
#endif
    } // end Pruning

#if defined USE_INTERNAL_ITERATIVE_DEEPENING
    //---------------------------------------------------------------------
    // Internal Iterative Deepening.
    //---------------------------------------------------------------------
    if (   tt_move == Move::MOVE_NONE
        && depth >= 4)
    {
        depth--;
    }
#endif

    //====================================================================================
    //  Génération des coups
    //------------------------------------------------------------------------------------

    MOVE mc = get_counter(td, C, (si-1)->move);

    MovePicker movePicker(&board, td->history, tt_move,
                          si->killer1, si->killer2, mc, false, 0);

    MOVE move;
    const int old_alpha = alpha;
    int moveCount = 0;

    // Boucle sur tous les coups
    while ( (move = movePicker.next_move().move ) != Move::MOVE_NONE )
    {
        if (move == excluded)
            continue;

        bool isQuiet = !Move::is_tactical(move);    // capture, promotion (avec capture ou non), prise en-passant

#ifdef ACC
        // Affichage du coup courant
        if (ply==0 && isPVNode && !td->index && my_timer.elapsedTime() > CurrmoveTimerMS)
        {
            show_uci_current(move, legalMoves, depth);
        }
#endif

        int extension = 0;

#if defined USE_SINGULAR_EXTENSION
        //-------------------------------------------------
        //  SINGULAR EXTENSION
        //-------------------------------------------------
        if (   depth > 8
            && si->ply < 2 * depth
            && excluded == Move::MOVE_NONE
            && !isRoot
            && move == tt_move
            && tt_depth > depth - 3
            && tt_bound == BOUND_LOWER
            && abs(tt_score) < TBWIN_IN_X)
        {
            // Search to reduced depth with a zero window a bit lower than ttScore
            int sing_beta  = tt_score - depth*2;
            int sing_depth = (depth-1)/2;
            // PVariation sing_pv;
            // sing_pv.length = 0;

            si->excluded = move;
            score = alpha_beta<C>(sing_beta-1, sing_beta, sing_depth, td, si);
            si->excluded = Move::MOVE_NONE;

            // Extend as this move seems forced
            if (score < sing_beta)
                extension = 1;
        }


#endif

#if defined USE_LATE_MOVE_PRUNING
        //-------------------------------------------------
        //  Late Move Pruning / Move Count Based Pruning
        //-------------------------------------------------
        if (   !isPVNode
            && !inCheck
            && best_score > -TBWIN_IN_X
            && moveCount > (3 + 2 * depth * depth) / (2 - improving) )
            continue;
#endif

        // execute current move
        board.make_move<C>(move);
        si->move = move;

        // Update counter of moves actually played
        moveCount++;
        if (isQuiet)
            quietsTried[quietsPlayed++] = move;

#if defined USE_LATE_MOVE_REDUCTION
        //------------------------------------------------------------------------------------
        //  LATE MOVE REDUCTION
        //------------------------------------------------------------------------------------
        int newDepth = depth - 1 + extension;
        bool doFullDepthSearch;

        if (   depth > 2
            && moveCount > (2 + isPVNode)
            && !inCheck
            && isQuiet)
        {
            // Base reduction
            int R = Reductions[isQuiet][std::min(31, depth)][std::min(31, moveCount)];
            // Reduce less in pv nodes
            R -= isPVNode;
            // Reduce less when improving
            R -= improving;
            // Reduce less for killers
            //            r -= move == mp.kill1 || move == mp.kill2;
            // Reduce more for the side that last null moved
            //           r += sideToMove == thread->nullMover;
            // Adjust reduction by move history (-2 to +2)
            //           r -= histScore / 6000;

            // Depth after reductions, avoiding going straight to quiescence
            int lmrDepth = CLAMP(newDepth - R, 1, newDepth - 1);

            // Search this move with reduced depth:
            score = -alpha_beta<~C>(-alpha-1, -alpha, lmrDepth, td, si+1);

            doFullDepthSearch = score > alpha && lmrDepth < newDepth;
        } else
            doFullDepthSearch = !isPVNode || moveCount > 1;

        // Full depth zero-window search
        if (doFullDepthSearch)
            score = -alpha_beta<~C>(-alpha-1, -alpha, newDepth, td, si+1);
        //    -AlphaBeta(thread, ss+1, -alpha-1, -alpha, newDepth);

        // Full depth alpha-beta window search
        if (isPVNode && ((score > alpha && score < beta) || moveCount == 1))
            score = -alpha_beta<~C>(-beta, -alpha, newDepth, td, si+1);
                // -AlphaBeta(thread, ss+1, -beta, -alpha, newDepth);
#endif


        // retract current move
        board.undo_move<C>();

        //  Time-out
        if (td->stopped)
            return 0;

        // On a trouvé un nouveau meilleur coup
        if (score > best_score)
        {
            best_score = score; // le meilleur que l'on ait trouvé, pas forcément intéressant
            best_move  = move;

            // If score beats alpha we update alpha
            if (score > alpha)
            {
                alpha = score;

                // update the PV
                if (td->index == 0)
                    update_pv(si, move);

                // Update search history
                // if (isQuiet)
                //     si->increment_history(C, move, depth);

                // If score beats beta we have a cutoff
                if (score >= beta)
                {
                    // non, ce coup est trop bon pour l'adversaire
                    // Update Killers
                    if (isQuiet)
                    {
                        update_history(td, C, move, depth);
                        update_killers(si, move);
                        update_counter(td, C, (si-1)->move , move);
                    }
                    transpositionTable.store(board.hash, move, score, static_eval, BOUND_LOWER, depth, si->ply);
                    return score;
                }


            } // score > alpha
        }     // meilleur coup
    }         // boucle sur les coups

    // est-on mat ou pat ?
    if (moveCount == 0)
    {
        // On est en échec, et on n'a aucun coup : on est MAT
        // On n'est pas en échec, et on n'a aucun coup : on est PAT
        return inCheck ? -MATE + si->ply : 0;
    }

    // don't let our score inflate too high (tb)
    best_score = std::min(best_score, max_score);

    if (!td->stopped && !excluded)
    {
        //  si on est ici, c'est que l'on a trouvé au moins 1 coup
        //  et de plus : score < beta
        //  si score >  alpha    : c'est un bon coup : HASH_EXACT
        //  si score <= alpha    : c'est un coup qui n'améliore pas alpha : HASH_ALPHA
        int flag = (alpha != old_alpha) ? BOUND_EXACT : BOUND_UPPER;
        transpositionTable.store(board.hash, best_move, best_score, static_eval, flag, depth, si->ply);
    }

    return best_score;
}

template void Search::think<WHITE>(const Board &m_board, const Timer &m_timer, int _index);
template void Search::think<BLACK>(const Board &m_board, const Timer &m_timer, int _index);

