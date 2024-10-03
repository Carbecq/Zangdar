#include "MovePicker.h"
#include "Search.h"
#include "ThreadPool.h"
#include "Timer.h"
#include "TranspositionTable.h"
#include "Move.h"

//======================================================
//! \brief  Lancement d'une recherche
//!
//! itérative deepening
//! alpha-beta
//!
//------------------------------------------------------
template<Color C>
void Search::think(int m_index)
{
#if defined DEBUG_LOG
    char message[100];
    sprintf(message, "Search::think (thread=%d)", m_index);
    printlog(message);
#endif

    ThreadData* td = &threadPool.threadData[m_index];
    SearchInfo* si = &td->info[0];

    for (int i = 0; i < MAX_PLY+STACK_OFFSET; i++)
        (si + i)->ply = i;

    /*
    0   4                                     131 135
    +---|--------------------------------------+---+        136 éléments total (128 + 2*4)

    +---+--------------------------------------+---+
        0                                     127 131       ply
 */

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
    si->pv.score = -INFINITE;

    for (td->depth = 1; td->depth <= timer.getSearchDepth(); td->depth++)
    {
        // Search position, using aspiration windows for higher depths
        aspiration_window<C>(td, si);

        if (td->stopped)
            break;

        // L'itération s'est terminée sans problème
        // On peut mettre à jour les infos UCI
        if (td->index == 0)
        {
            td->best_depth     = td->depth;
            td->pvs[td->depth] = si->pv;

            auto elapsed = timer.elapsedTime();

            if (threadPool.get_logUci())
                show_uci_result(td, elapsed);

            timer.update(td->depth, td->pvs);

            // If an iteration finishes after optimal time usage, stop the search
            if (timer.finishOnThisDepth(elapsed, td->depth, td->pvs, td->nodes))
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
void Search::aspiration_window(ThreadData* td, SearchInfo* si)
{
    int alpha  = -INFINITE;
    int beta   = INFINITE;
    int depth  = td->depth;

    const int initialWindow = 12;
    int delta = 16;

    // After a few depths use a previous result to form the window
    if (depth >= 6)
    {
        alpha = std::max(td->pvs[td->best_depth].score - initialWindow, -INFINITE);
        beta  = std::min(td->pvs[td->best_depth].score + initialWindow, INFINITE);
    }

    while (true)
    {
        si->pv.score = alpha_beta<C>(alpha, beta, std::max(1, depth), td, si);

        if (td->stopped)
            break;

        // Search failed low, adjust window and reset depth
        if (si->pv.score <= alpha)
        {
            alpha = std::max(si->pv.score - delta, -INFINITE); // alpha/score-delta
            beta  = (alpha + beta) / 2;
            depth = td->depth;
        }

        // Search failed high, adjust window and reduce depth
        else if(si->pv.score >= beta)  // Fail High
        {
            beta  = std::min(si->pv.score + delta, INFINITE);   // beta/score+delta
            // idée de Berserk
            if (abs(si->pv.score) < TBWIN_IN_X)
                depth--;
        }

        // Score within the bounds is accepted as correct
        else
            return ;

        delta += delta*2 / 3;
    }
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

    constexpr Color THEM = ~C;

    // Prefetch La table de transposition aussitôt que possible
    transpositionTable.prefetch(board.get_hash());


    bool isRoot = (si->ply == 0);

    // For PVS, the node is a PV node if beta - alpha != 1 (full-window = not a null window)
    // We do not want to do most pruning techniques on PV nodes
    // Ne pas confondre avec PV NODE (notation Knuth)
    // Utiliser plutôt la notation : EXACT
    bool isPV = ((beta - alpha) != 1);


    // Ensure a fresh PV
    si->pv.length     = 0;

    /* On a atteint la fin de la recherche
        Certains codes n'appellent la quiescence que si on n'est pas en échec
        ceci amène à une explosion des coups recherchés */
    if (depth <= 0)
    {
        return (quiescence<C>(alpha, beta, td, si));
    }

    // Update node count and selective depth
    td->nodes++;
    td->seldepth = isRoot ? 0 : std::max(td->seldepth, si->ply);


    //  Time-out
    if (td->stopped || check_limits(td))
    {
        td->stopped = true;
        return 0;
    }

    // On a atteint la limite en profondeur de recherche ?
    if (si->ply >= MAX_PLY)
        return board.evaluate();


    //  Caractéristiques de la position
    bool isInCheck  = board.is_in_check<C>();
    int  score      = -INFINITE;
    int  best_score = -INFINITE;        // initially assume the worst case
    MOVE best_move  = Move::MOVE_NONE;  // meilleur coup local
    int  max_score  = INFINITE;         // best possible
    MOVE excluded   = si->excluded;

    //  Check Extension
    if (isInCheck == true && depth + 1 < MAX_PLY)
        depth++;


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

    // Trust TT if not a pvnode and the entry depth is sufficiently high
    // At non-PV nodes we check for an early TT cutoff
    //    (we do not return immediately on full PVS windows since this cuts
    //     short the PV line)
    if (tt_hit && !isPV && tt_depth >= depth)
    {
            if (   (tt_bound == BOUND_EXACT)
                || (tt_bound == BOUND_LOWER && tt_score >= beta)
                || (tt_bound == BOUND_UPPER && tt_score <= alpha))
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
        if (isPV)
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

    if (isInCheck)
    {
        si->eval = static_eval = -MATE + si->ply;
    }
    else if (excluded)
    {
        static_eval = si->eval;
    }
    else
    {
        si->eval = static_eval = (tt_eval != NOSCORE) ? tt_eval : board.evaluate();
    }



    // Re-initialise les killer des enfants
    td->killer1[si->ply+1] = Move::MOVE_NULL;
    td->killer2[si->ply+1] = Move::MOVE_NULL;

    /*  Avons-nous amélioré la position ?
        Si on ne s'est pas amélioré dans cette ligne, on va pouvoir couper un peu plus */
    bool improving = (si->ply >= 2) && !isInCheck && (static_eval > (si-2)->eval);


    if (!isInCheck && !isRoot && !isPV && !si->excluded)
    {
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

        //---------------------------------------------------------------------
        //  NULL MOVE PRUNING
        //---------------------------------------------------------------------
        if (
            depth >= 3
            && static_eval >= beta
            && (si-1)->move != Move::MOVE_NULL
            && (si-2)->move != Move::MOVE_NULL
            && !excluded
            && board.getNonPawnMaterial<C>())
        {
            int R = 2 + (32 * depth + std::min(static_eval - beta, 384)) / 128;

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

        //---------------------------------------------------------------------
        //  ProbCut
        //---------------------------------------------------------------------
        int betaCut = beta + ProbCutMargin;
        if (   !isInCheck
            && !isPV
            && depth >= ProbCutDepth
            && !excluded
            && !(tt_hit && tt_depth >= depth - 3 && tt_score < betaCut))
        {
            MovePicker movePicker(&board, td, si->ply, Move::MOVE_NONE, Move::MOVE_NONE, Move::MOVE_NONE, Move::MOVE_NONE, 0);
            MOVE pbMove;

            while ( (pbMove = movePicker.next_move(true).move ) != Move::MOVE_NONE )
            {
                board.make_move<C>(pbMove);
                si->move = pbMove;

                // Teste si une recherche de quiescence donne un score supérieur à betaCut
                int pbScore = -quiescence<~C>(-betaCut, -betaCut+1, td, si+1);

                // Si oui, alors on effectue une recherche normale, avec une profondeur réduite
                if (pbScore >= betaCut)
                    pbScore = -alpha_beta<~C>(-betaCut, -betaCut+1, depth-4, td, si+1);

                board.undo_move<C>();

                // Coupure si cette dernière recherche bat betaCut
                if (pbScore >= betaCut)
                {
                    transpositionTable.store(board.hash, pbMove, pbScore, static_eval, BOUND_LOWER, depth-3, si->ply);
                    return pbScore;
                }
            }
        }

    } // end Pruning

    //---------------------------------------------------------------------
    // Internal Iterative Deepening.
    //---------------------------------------------------------------------
    if (   tt_move == Move::MOVE_NONE
        && depth >= 4)
    {
        depth--;
    }

    //====================================================================================
    //  Génération des coups
    //------------------------------------------------------------------------------------
    bool skipQuiets = false;
    MOVE mc = get_counter_move(td, THEM, si->ply);

    MovePicker movePicker(&board, td, si->ply, tt_move,
                          td->killer1[si->ply], td->killer2[si->ply], mc, 0);

    const int old_alpha = alpha;
    int  move_count = 0;
    MOVE quiets_moves[MAX_MOVES];
    int  quiets_count = 0;
    MOVE move;
    bool isQuiet;
    int  hist = 0, cmhist = 0, fuhist = 0;

    // Static Exchange Evaluation Pruning Margins
    int  seeMargin[2] = {
        SEENoisyMargin * depth * depth,
        SEEQuietMargin * depth
    };

    // Futility Pruning Margin
    int futilityMargin = static_eval + FutilityMargin * depth;

    // Boucle sur tous les coups
    while ( (move = movePicker.next_move(skipQuiets).move ) != Move::MOVE_NONE )
    {
        if (move == excluded)
            continue;

        const U64 starting_nodes = td->nodes;

        isQuiet = !Move::is_tactical(move);    // capture, promotion (avec capture ou non), prise en-passant
        if (isQuiet)
        {
            quiets_moves[quiets_count++] = move;

            cmhist = td->get_counter_move_history(si->ply, move);
            fuhist = td->get_followup_move_history(si->ply, move);
            hist   = td->get_history(C, move) + cmhist + fuhist;
        }

#ifdef ACC
        // Affichage du coup courant
        if (ply==0 && PVNode && !td->index && my_timer.elapsedTime() > CurrmoveTimerMS)
        {
            show_uci_current(move, legalMoves, depth);
        }
#endif

        int extension = 0;

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
            score = alpha_beta<C>(sing_beta-1, sing_beta, sing_depth, td, si); //TODO si ou si+1 ?
            si->excluded = Move::MOVE_NONE;

            // Extend as this move seems forced
            if (score < sing_beta)
                extension = 1;
        }

        //-------------------------------------------------
        // Futility Pruning.
        //-------------------------------------------------
        if (   !isRoot
            &&  isQuiet
            &&  best_score > -TBWIN_IN_X
            &&  futilityMargin <= alpha
            &&  depth <= FutilityPruningDepth
            &&  hist < FutilityPruningHistoryLimit[improving])
        {
            skipQuiets = true;
        }

        //-------------------------------------------------
        //  Late Move Pruning / Move Count Pruning
        //-------------------------------------------------
        if (   !isRoot
            &&  isQuiet
            &&  best_score > -TBWIN_IN_X
            &&  depth <= LateMovePruningDepth
            &&  quiets_count > LateMovePruningCount[improving][depth])
        {
            skipQuiets = true;
            continue;
        }

        //-------------------------------------------------
        // Counter Move Pruning.
        //-------------------------------------------------
        if (   !isRoot
            &&  isQuiet
            &&  best_score > -TBWIN_IN_X
            &&  depth <= CounterMovePruningDepth[improving]
            &&  cmhist < CounterMoveHistoryLimit[improving])
        {
            continue;
        }

        //-------------------------------------------------
        // Follow Up Move Pruning.
        //-------------------------------------------------
        if (   !isRoot
            &&  isQuiet
            &&  best_score > -TBWIN_IN_X
            &&  depth <= FollowUpMovePruningDepth[improving]
            &&  fuhist < FollowUpMoveHistoryLimit[improving])
        {
            continue;
        }

        //-------------------------------------------------
        //  Static Exchange Evaluation Pruning
        //-------------------------------------------------
        if (   !isRoot
            &&  best_score > -TBWIN_IN_X
            &&  depth <= SEEPruningDepth
            &&  movePicker.get_stage() > STAGE_GOOD_NOISY
            && !board.fast_see(move, seeMargin[isQuiet]))
        {
            continue;
        }

        // execute current move
        board.make_move<C>(move);
        si->move = move;

        // Update counter of moves actually played
        move_count++;

        //------------------------------------------------------------------------------------
        //  LATE MOVE REDUCTION
        //------------------------------------------------------------------------------------
        int newDepth = depth - 1 + extension;
        bool doFullDepthSearch;

        if (   depth > 2
            && move_count > (2 + isPV)
            && !isInCheck
            && isQuiet)
        {
            // Base reduction
            int R = Reductions[isQuiet][std::min(31, depth)][std::min(31, move_count)];
            // Reduce less in pv nodes
            R -= isPV;
            // Reduce less when improving
            R -= improving;

            // Adjust based on history
            R -= std::max(-2, std::min(2, hist / 5000));

            // Reduce less for killers
            //            r -= move == mp.kill1 || move == mp.kill2;
            // Reduce more for the side that last null moved
            //           r += sideToMove == thread->nullMover;
            // Adjust reduction by move history (-2 to +2)
            //           r -= histScore / 6000;

            // Depth after reductions, avoiding going straight to quiescence
            int lmrDepth = std::clamp(newDepth - R, 1, newDepth - 1);

            // Search this move with reduced depth:
            score = -alpha_beta<~C>(-alpha-1, -alpha, lmrDepth, td, si+1);

            doFullDepthSearch = score > alpha && lmrDepth < newDepth;
        }
        else
        {
            doFullDepthSearch = !isPV || move_count > 1;
        }

        // Full depth zero-window search
        if (doFullDepthSearch)
            score = -alpha_beta<~C>(-alpha-1, -alpha, newDepth, td, si+1);

        // Full depth alpha-beta window search
        if (isPV && ((score > alpha && score < beta) || move_count == 1))
            score = -alpha_beta<~C>(-beta, -alpha, newDepth, td, si+1);


        // retract current move
        board.undo_move<C>();

        // Track where nodes were spent in the Main thread at the Root
        if (isRoot && td->index==0)
            timer.updateMoveNodes(move, td->nodes - starting_nodes);

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

                // If score beats beta we have a cutoff
                if (score >= beta)
                {
                    // non, ce coup est trop bon pour l'adversaire
                    // Update Killers
                    if (isQuiet)
                    {
                        // Bonus pour le coup quiet ayant provoqué un cutoff (fail-high)
                        update_history(td, C, move, depth*depth);
                        update_counter_move_history(td, si->ply, move, depth*depth);
                        update_followup_move_history(td, si->ply, move, depth*depth);

                        // Malus pour les autres coups quiets
                        for (int i = 0; i < quiets_count - 1; i++)
                        {
                            update_history(td, C, quiets_moves[i], -depth*depth);
                            update_counter_move_history(td, si->ply, quiets_moves[i], -depth*depth);
                            update_followup_move_history(td, si->ply, quiets_moves[i], -depth*depth);
                        }

                        // Met à jour les Killers
                        update_killers(td, si->ply, move);

                        // Met à jour le Counter-Move
                        update_counter_move(td, THEM, si->ply, move);
                    }
                    transpositionTable.store(board.hash, move, score, static_eval, BOUND_LOWER, depth, si->ply);
                    return score;
                }


            } // score > alpha
        }     // meilleur coup
    }         // boucle sur les coups

    // est-on mat ou pat ?
    if (move_count == 0)
    {
        // On est en échec, et on n'a aucun coup : on est MAT
        // On n'est pas en échec, et on n'a aucun coup : on est PAT
        return isInCheck ? -MATE + si->ply : 0;
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

template void Search::think<WHITE>(int _index);
template void Search::think<BLACK>(int _index);

