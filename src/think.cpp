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
void Search::think(Board board, Timer timer, int m_index)
{
#if defined DEBUG_LOG
    char message[100];
    sprintf(message, "Search::think (thread=%d)", m_index);
    printlog(message);
#endif

    // capacity n'est pas conservé lors de la copie ...
    board.reserve_capacity();

    ThreadData* td = &threadPool.threadData[m_index];
    SearchInfo* si = &td->info[0];

    for (int i = 0; i < MAX_PLY+STACK_OFFSET; i++)
    {
        (si + i)->ply = i;
        (si + i)->cont_hist = &td->history.continuation_history[0][0];
    }

    /*
    0   4                                     131 135
    +---|--------------------------------------+---+        136 éléments total (128 + 2*4)

    +---+--------------------------------------+---+
        0                                     127 131       ply
 */

    // iterative deepening
    iterative_deepening<C>(board, timer, td, si);

    // Arrêt des threads, affichage du résultat
    if (m_index == 0)
    {
        if (threadPool.get_logUci())
        {
            threadPool.main_thread_stopped(); // arrêt des autres threads
            threadPool.wait(1);               // attente de ces threads

            //TODO : choix du bestmove entre les threads ?

            // Aussitôt que Fastchess recoit "bestmove", il lance une nouvelle commande "position"

            // * bestmove <move1> [ ponder <move2> ]
            // 	the engine has stopped searching and found the move <move> best in this position.
            // 	the engine can send the move it likes to ponder on. The engine must not start pondering automatically.
            // 	this command must always be sent if the engine stops searching, also in pondering mode if there is a
            // 	"stop" command, so for every "go" command a "bestmove" command is needed!
            // 	Directly before that the engine should send a final "info" command with the final search information,
            // 	the the GUI has the complete statistics about the last search.

            show_uci_best(td);
        }

        transpositionTable.update_age();
    }
    // transpositionTable.stats();
}

//======================================================
//! \brief  iterative deepening
//!
//! alpha-beta
//!
//------------------------------------------------------
template<Color C>
void Search::iterative_deepening(Board& board, Timer& timer, ThreadData* td, SearchInfo* si)
{
    si->pv.length = 0;

    for (td->depth = 1; td->depth <= timer.getSearchDepth(); td->depth++)
    {
        // Search position, using aspiration windows for higher depths
        td->score = aspiration_window<C>(board, timer, td, si);

        if (td->stopped)
            break;

        // L'itération s'est terminée sans problème
        // On peut mettre à jour les infos UCI
        if (td->index == 0)
        {
            td->best_depth = td->depth;
            td->best_move  = si->pv.line[0];
            td->best_score = td->score;

            auto elapsed = timer.elapsedTime();

            if (threadPool.get_logUci())
                show_uci_result(td, elapsed, si->pv);

            // If an iteration finishes after optimal time usage, stop the search
            if (timer.finishOnThisDepth(elapsed, td->depth, td->best_move, td->nodes))
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
int Search::aspiration_window(Board& board, Timer& timer, ThreadData* td, SearchInfo* si)
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
        score = alpha_beta<C>(board, timer, alpha, beta, std::max(1, depth), td, si);

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
        {
            return score;
        }

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
int Search::alpha_beta(Board& board, Timer& timer, int alpha, int beta, int depth, ThreadData* td, SearchInfo* si)
{
    assert(beta > alpha);

    constexpr Color THEM = ~C;

     //  Time-out
    if (td->stopped || timer.check_limits(td->depth, td->index, td->nodes))
    {
         td->stopped = true;
         return 0;
    }


    // Prefetch La table de transposition aussitôt que possible
    transpositionTable.prefetch(board.get_key());


    //  Caractéristiques de la position
    const bool isInCheck  = board.is_in_check();
    const bool isExcluded = si->excluded != Move::MOVE_NONE;
    const bool isRoot     = (si->ply == 0);

    // For PVS, the node is a PV node if beta - alpha != 1 (full-window = not a null window)
    // We do not want to do most pruning techniques on PV nodes
    // Ne pas confondre avec PV NODE (notation Knuth)
    // Utiliser plutôt la notation : EXACT
    const bool isPV = ((beta - alpha) != 1);


    // Ensure a fresh PV
    si->pv.length     = 0;

    /* On a atteint la fin de la recherche
        Certains codes n'appellent la quiescence que si on n'est pas en échec
        ceci amène à une explosion des coups recherchés */
    if (depth <= 0)
    {
        return (quiescence<C>(board, timer, alpha, beta, td, si));
    }

    // Update node count and selective depth
    td->nodes++;
    td->seldepth = isRoot ? 0 : std::max(td->seldepth, si->ply);


    // On a atteint la limite en profondeur de recherche ?
    if (si->ply >= MAX_PLY)
        return board.evaluate();


    int  score      = -INFINITE;
    int  best_score = -INFINITE;        // initially assume the worst case
    MOVE best_move  = Move::MOVE_NONE;  // meilleur coup local


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
    int   tt_score = VALUE_NONE;
    int   tt_eval  = VALUE_NONE;
    MOVE  tt_move  = Move::MOVE_NONE;
    int   tt_bound = BOUND_NONE;
    int   tt_depth = 0;
    bool  tt_pv    = false;
    bool  tt_hit   = isExcluded ? false : transpositionTable.probe(board.get_key(), si->ply, tt_move, tt_score, tt_eval, tt_bound, tt_depth, tt_pv);

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


    // Probe the Syzygy Tablebases
    int tb_score = VALUE_NONE;
    int tb_bound = BOUND_NONE;
    int max_score = MATE;
    const bool ttPV = isPV || tt_pv;

    if (!isExcluded && threadPool.get_useSyzygy() && board.probe_wdl(tb_score, tb_bound, si->ply) == true)
    {
        td->tbhits++;

        // Check to see if the WDL value would cause a cutoff
        if (    tb_bound == BOUND_EXACT
            || (tb_bound == BOUND_LOWER && tb_score >= beta)
            || (tb_bound == BOUND_UPPER && tb_score <= alpha))
        {
            transpositionTable.store(board.get_key(), Move::MOVE_NONE, tb_score, VALUE_NONE, tb_bound, depth, si->ply, ttPV);
            return tb_score;
        }

        // Limit the score of this node based on the tb result
        if (isPV)
        {
            // Never score something worse than the known Syzygy value
            if (tb_bound == BOUND_LOWER)
            {
                best_score = tb_score;
                alpha = std::max(alpha, tb_score);
            }

            // Never score something better than the known Syzygy value
            else if (tb_bound == BOUND_UPPER)
            {
                max_score = tb_score;
            }
        }
    }


    //  Evaluation Statique
    int static_eval = VALUE_NONE;
    if (!isExcluded)
    {
        if (isInCheck)
        {
            static_eval = si->eval = -MATE + si->ply;
        }
        else
        {
            static_eval = si->eval = (tt_hit && tt_eval != VALUE_NONE) ?
                    tt_eval : board.evaluate();

            // Amélioration de static-eval, au cas où tt_score est assez bon
            if(tt_hit
                && (    tt_bound == BOUND_EXACT
                    || (tt_bound == BOUND_LOWER && tt_score >= static_eval)
                    || (tt_bound == BOUND_UPPER && tt_score <= static_eval) ))
            {
                static_eval = tt_score;
            }
        }

        // if (!ttHit)
        //     m_ttable.put(pos.key(), ScoreNone, rawStaticEval, NullMove, 0, 0, TtFlag::None, ttpv);

    }


    // Re-initialise les killer des enfants
    (si+1)->killer1 = Move::MOVE_NONE;
    (si+1)->killer2 = Move::MOVE_NONE;


    //  Avons-nous amélioré la position ?
    //  Si on ne s'est pas amélioré dans cette ligne, on va pouvoir couper un peu plus
    const bool improving = [&]
    {
        if (isInCheck)
            return false;
        if (si->ply > 1 && (si-2)->eval != VALUE_NONE)
            return si->eval > (si - 2)->eval;
        if (si->ply > 3 && (si-4)->eval != VALUE_NONE)
            return si->eval > (si - 4)->eval;
        return true;
    }();



    if (!isInCheck && !isRoot && !isPV && !isExcluded)
    {
        //---------------------------------------------------------------------
        //  RAZORING
        //---------------------------------------------------------------------
        if (   depth <= 3
            && (static_eval + 200 * depth) <= alpha)
        {
            score = quiescence<C>(board, timer, alpha, beta, td, si);
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
            && board.getNonPawnMaterial<C>())
        {
            int R = 3 + (32.0 * depth + std::min(static_eval - beta, 384)) / 128.0;

            si->move = Move::MOVE_NULL;
            si->cont_hist = &td->history.continuation_history[0][0];
            board.make_nullmove<C>();
            int null_score = -alpha_beta<~C>(board, timer, -beta, -beta + 1, depth - 1 - R, td, si+1);
            board.undo_nullmove<C>();

            if (td->stopped)
                return 0;

            // Cutoff
            if (null_score >= beta)
            {
                // (Stockfish) : on ne retourne pas un score proche du mat
                //               car ce score ne serait pas prouvé
                return(null_score >= TBWIN_IN_X ? beta : null_score);
            }
        }

        //---------------------------------------------------------------------
        //  ProbCut
        //---------------------------------------------------------------------
        int betaCut = beta + ProbCutMargin;
        if (   !isInCheck
            && !ttPV
            && depth >= ProbCutDepth
            && !(tt_hit && tt_depth >= depth - 3 && tt_score < betaCut))
        {
            MovePicker movePicker(&board, td->history, si, Move::MOVE_NONE, Move::MOVE_NONE, Move::MOVE_NONE, Move::MOVE_NONE, 0);
            MOVE pbMove;

            while ( (pbMove = movePicker.next_move(true).move ) != Move::MOVE_NONE )
            {
                board.make_move<C, true>(pbMove);
                si->move = pbMove;
                si->cont_hist = &td->history.continuation_history[static_cast<U32>(Move::piece(pbMove))][Move::dest(pbMove)];

                // Teste si une recherche de quiescence donne un score supérieur à betaCut
                int pbScore = -quiescence<~C>(board, timer, -betaCut, -betaCut+1, td, si+1);

                // Si oui, alors on effectue une recherche normale, avec une profondeur réduite
                if (pbScore >= betaCut)
                    pbScore = -alpha_beta<~C>(board, timer, -betaCut, -betaCut+1, depth-4, td, si+1);

                board.undo_move<C, true>();

                // Coupure si cette dernière recherche bat betaCut
                if (pbScore >= betaCut)
                {
                    transpositionTable.store(board.get_key(), pbMove, pbScore, static_eval, BOUND_LOWER, depth-3, si->ply, false);
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

    si->doubleExtensions = (si-1)->doubleExtensions;

    //====================================================================================
    //  Génération des coups
    //------------------------------------------------------------------------------------
    bool skipQuiets = false;

    MovePicker movePicker(&board, td->history, si, tt_move,
                          si->killer1, si->killer2, td->history.get_counter_move(si), 0);

    const int old_alpha = alpha;
    int  move_count = 0;
    std::array<MOVE, MAX_MOVES> quiets_moves;
    int  quiets_count = 0;
    std::array<MOVE, MAX_MOVES> noisy_moves;
    int  noisy_count = 0;
    MOVE move;
    // int  history_score = 0;
    int  hist = 0, cmhist = 0, fuhist = 0;

    // Static Exchange Evaluation Pruning Margins
    int  seeMargin[2] = {
        SEENoisyMargin * depth * depth,
        SEEQuietMargin * depth
    };

    static constexpr int FutilityMargin = 95;
    static constexpr int FutilityPruningDepth = 8;
    static constexpr int FutilityPruningHistoryLimit[] = { 12000, 6000 };

    static constexpr int CounterMovePruningDepth[] = { 3, 2 };
    static constexpr int CounterMoveHistoryLimit[] = { 0, -1000 };

    static constexpr int FollowUpMovePruningDepth[] = { 3, 2 };
    static constexpr int FollowUpMoveHistoryLimit[] = { -2000, -4000 };

    // Futility Pruning Margin
    int futilityMargin = static_eval + FutilityMargin * depth;

    // Boucle sur tous les coups
    while ( (move = movePicker.next_move(skipQuiets).move ) != Move::MOVE_NONE )
    {
        if (move == si->excluded)
            continue;

        const U64 starting_nodes = td->nodes;

        const bool isQuiet   = !Move::is_tactical(move);    // capture, promotion (avec capture ou non), prise en-passant
        const bool isCapture = Move::is_capturing(move);

        if (isCapture)
        {
            noisy_moves[noisy_count++] = move;
            // history_score = td->history.get_capture_history(C);
        }
        else if (isQuiet)
        {
            quiets_moves[quiets_count++] = move;
            // history_score = td->history.get_quiet_history(C, si, move);
            cmhist = td->history.get_counter_move_history(si, move);
            fuhist = td->history.get_followup_move_history(si, move);
            hist   = td->history.get_main_history(C, move) + cmhist + fuhist;
        }

        //-------------------------------------------------
        //  SINGULAR EXTENSION
        //-------------------------------------------------
        int extension = 0;

        if (   depth > 8
            && si->ply < 2 * depth
            && !isExcluded  // Avoid recursive singular search
            && !isRoot
            && move == tt_move
            && tt_depth > depth - 3
            && tt_bound == BOUND_LOWER
            && abs(tt_score) < TBWIN_IN_X)
        {
            // Search to reduced depth with a zero window a bit lower than ttScore
            int sing_beta  = tt_score - depth*2;    //TODO essai -depth*3
            int sing_depth = (depth-1)/2;

            si->excluded = move;
            score = alpha_beta<C>(board, timer, sing_beta-1, sing_beta, sing_depth, td, si); //TODO si ou si+1 ?
            si->excluded = Move::MOVE_NONE;

            if (score < sing_beta)
            {
                if (   !isPV
                    && score < sing_beta - 50
                    && si->doubleExtensions <= 20)  // Avoid search explosion by limiting the number of double extensions
                {
                    extension = 2;
                    si->doubleExtensions++;
                }
                else
                {
                    extension = 1;
                }
            }
            else if (sing_beta >= beta)
            {
                // multicut!
                return sing_beta;
            }
            else if (tt_score >= beta)
            {
                // negative extension!
                extension = -2;
            }
        }

        //-------------------------------------------------
        // Futility Pruning.
        //-------------------------------------------------

        // Futility Pruning Margin
        // int futilityMargin = static_eval + FutilityMargin * depth;
        //                                    95

        // history_score =
        //     move.IsCapture(state)
        //         ? history.GetCaptureMoveScore(state, move)
        //         : history.GetQuietMoveScore(state, move, stack->threats, stack);

        if (   !isRoot
            &&  isQuiet
            &&  best_score > -TBWIN_IN_X
            &&  futilityMargin <= alpha
            &&  depth <= FutilityPruningDepth
            &&  hist < FutilityPruningHistoryLimit[improving])
        {
            skipQuiets = true;
        }


        // if (   !isRoot
        //     &&  depth <= 8
        //     &&  isQuiet
        //     &&  best_score > -TBWIN_IN_X
        //     &&  history_score < FutilityPruningHistoryLimit[improving]
        //     &&  static_eval + 95*depth <= alpha)
        // {
        //     skipQuiets = true;
        // }

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

        if (   !isRoot
            &&  isQuiet
            &&  best_score > -TBWIN_IN_X
            &&  depth <= CounterMovePruningDepth[improving]
            &&  cmhist < CounterMoveHistoryLimit[improving])
        {
            continue;
        }

        if (   !isRoot
            &&  isQuiet
            &&  best_score > -TBWIN_IN_X
            &&  depth <= FollowUpMovePruningDepth[improving]
            &&  fuhist < FollowUpMoveHistoryLimit[improving])
        {
            continue;
        }

        //-------------------------------------------------
        // Countinuation Move Pruning.
        //-------------------------------------------------
        // if (   !isRoot
        //     &&  isQuiet
        //     &&  best_score > -TBWIN_IN_X
        //     &&  depth <= ContinuationMovePruningDepth[improving]
        //     &&  history_score < ContinuationMoveHistoryLimit[improving]) //TODO voir cette marge
        // {
        //     continue;
        // }

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
        si->move = move;
        si->cont_hist = &td->history.continuation_history[static_cast<U32>(Move::piece(move))][Move::dest(move)];
        board.make_move<C, true>(move);

        // Update counter of moves actually played
        move_count++;

        //------------------------------------------------------------------------------------
        //  LATE MOVE REDUCTION
        //------------------------------------------------------------------------------------
        int newDepth = depth - 1 + extension;
        bool doFullDepthSearch;

        if (   depth > 2
            && move_count > (2 + isPV) )
        {
            // Base reduction
            int R = Reductions[isQuiet][std::min(31, depth)][std::min(31, move_count)];

            // Reduce less in pv nodes
            R -= ttPV;
            // Reduce less when improving
            R -= improving;

            // Reduce quiets more if ttMove is a capture
            R += Move::is_capturing(tt_move);

            // Reduce more when opponent has few pieces
            R += board.getNonPawnMaterialCount<THEM>() < 2;

            // Adjust based on history
            R -= std::max(-2, std::min(2, hist / 5000));

            // Depth after reductions, avoiding going straight to quiescence
            int lmrDepth = std::clamp(newDepth - R, 1, newDepth);

            // Search this move with reduced depth:
            score = -alpha_beta<~C>(board, timer, -alpha-1, -alpha, lmrDepth, td, si+1);

            doFullDepthSearch = score > alpha && lmrDepth < newDepth;
        }
        else
        {
            doFullDepthSearch = !isPV || move_count > 1;
        }

        // Full depth zero-window search
        if (doFullDepthSearch)
            score = -alpha_beta<~C>(board, timer, -alpha-1, -alpha, newDepth, td, si+1);

        // Full depth alpha-beta window search
        if (isPV && ((score > alpha && score < beta) || move_count == 1))
            score = -alpha_beta<~C>(board, timer, -beta, -alpha, newDepth, td, si+1);


        // retract current move
        board.undo_move<C, true>();

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
                    td->update_pv(si, move);

                // If score beats beta we have a cutoff
                if (score >= beta)
                {
                    // non, ce coup est trop bon pour l'adversaire
                    // Update Killers
                    if (isQuiet)
                    {
                        // Bonus pour le coup quiet ayant provoqué un cutoff (fail-high)
                        // td->history.update_quiet_history(C, si, move, depth*depth);
                        td->history.update_main_history(C, move, depth*depth);
                        td->history.update_counter_move_history(si, move, depth*depth);
                        td->history.update_followup_move_history(si, move, depth*depth);

                        // Malus pour les autres coups quiets
                        // for (int i = 0; i < quiets_count - 1; i++)
                        //     td->history.update_quiet_history(C, si, quiets_moves[i], -depth*depth);
                        for (int i = 0; i < quiets_count - 1; i++)
                        {
                            td->history.update_main_history(C, quiets_moves[i], -depth*depth);
                            td->history.update_counter_move_history(si, quiets_moves[i], -depth*depth);
                            td->history.update_followup_move_history(si, quiets_moves[i], -depth*depth);
                        }

                        // Met à jour les Killers
                        td->history.update_killers(si, move);

                        // Met à jour le Counter-Move
                        td->history.update_counter_move(si, move);
                    }
                    else
                    {
                        // td->history.update_capture_history(move, depth*depth);

                        // for (int i = 0; i < noisy_count - 1; i++)
                        // {
                        //     td->history.update_capture_history(move, -depth*depth);
                        // }
                    }

                    break;
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

    if (best_move)
    {
      // Since "good" captures are expected to be the best moves, we apply a
      // penalty to all captures even in the case where the best move was quiet
        for (int i = 0; i < noisy_count - 1; i++)
        {
            // td->history.update_capture_history(move, -depth*depth);
        }
    }

    // don't let our score inflate too high (tb)
    // if (isPV)
    //     best_score = std::max(syzygyMin, std::min(best_score, syzygyMax));
    best_score = std::min(best_score, max_score);

    if (!td->stopped && !isExcluded)
    {
        //  si on est ici, c'est que l'on a trouvé au moins 1 coup
        //  et de plus : score < beta
        //  si score >  alpha    : c'est un bon coup : HASH_EXACT
        //  si score <= alpha    : c'est un coup qui n'améliore pas alpha : HASH_ALPHA

        int bound = best_score >= beta     ? BOUND_LOWER
                  : best_score > old_alpha ? BOUND_EXACT
                  : BOUND_UPPER;
        // tt_store(board->hash, thread->height, bestMove, best, eval, depth, ttBound);
        // int flag = (alpha != old_alpha) ? BOUND_EXACT : BOUND_UPPER;
        transpositionTable.store(board.get_key(), best_move, best_score, static_eval, bound, depth, si->ply, ttPV);
    }

    return best_score;
}

template void Search::think<WHITE>(Board board, Timer timer, int _index);
template void Search::think<BLACK>(Board board, Timer timer, int _index);

template int Search::aspiration_window<WHITE>(Board& board, Timer& timer, ThreadData* td, SearchInfo* si);
template int Search::aspiration_window<BLACK>(Board& board, Timer& timer, ThreadData* td, SearchInfo* si);
