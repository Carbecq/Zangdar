#include "MovePicker.h"
#include "ThreadPool.h"
#include "Search.h"
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
void Search::think(Board board, Timer timer, size_t m_index)
{
#if defined DEBUG_LOG
    char message[100];
    sprintf(message, "Search::think (thread=%d)", m_index);
    printlog(message);
#endif

    // capacity n'est pas conservé lors de la copie ...
    board.reserve_capacity();

    nnue.start_search(board);

    // Grace au décalage, la position root peut regarder en arrière
    /*
    0   4                                     131 135
    +---|--------------------------------------+---+        136 éléments total (128 + 2*4)

    +---+--------------------------------------+---+
        0                                     127 131       ply
    */
    std::array<SearchInfo, STACK_SIZE> _info{};
    SearchInfo* si  = &(_info[STACK_OFFSET]);

    for (int i = 0; i < MAX_PLY+STACK_OFFSET; i++)
    {
        (si + i)->ply = i;
        (si + i)->cont_hist = &history.continuation_history[0][0];
    }

    // iterative deepening
    iterative_deepening<C>(board, timer, si);

    // Arrêt des threads, affichage du résultat
    if (m_index == 0)
    {
        // Toujours arrêter et attendre les autres threads
        threadPool.main_thread_stopped();
        threadPool.wait(1);

        //TODO : choix du bestmove entre les threads ?

        if (threadPool.get_logUci())
            show_uci_best();

        table->update_age();
    }
}

//======================================================
//! \brief  iterative deepening
//!
//! alpha-beta
//!
//------------------------------------------------------
template<Color C>
void Search::iterative_deepening(Board& board, Timer& timer, SearchInfo* si)
{
    for (iter_depth = 1; iter_depth <= timer.getSearchDepth(); iter_depth++)
    {
        // Search position, using aspiration windows for higher depths
        iter_score = aspiration_window<C>(board, timer, si);

        if (stopped)
            break;

        // L'itération s'est terminée sans problème
        // On peut mettre à jour les infos UCI
        if (index == 0)
        {
            iter_best_depth = iter_depth;
            iter_best_move  = si->pv.line[0];
            iter_best_score = iter_score;

            auto elapsed = timer.elapsedTime();

            if (threadPool.get_logUci())
                show_uci_result(elapsed, si->pv);

            // If an iteration finishes after optimal time usage, stop the search
            if (timer.finishOnThisDepth(elapsed, iter_depth, iter_best_move, nodes))
                break;

            seldepth = 0;
        }
    }
}

//======================================================
//! \brief  Aspiration Window
//!
//!
//------------------------------------------------------
template<Color C>
int Search::aspiration_window(Board& board, Timer& timer, SearchInfo* si)
{
    int alpha  = -INFINITE;
    int beta   = INFINITE;
    int depth  = iter_depth;
    int score  = iter_score;

    const int initialWindow = AspirationWindowsInitial;
    int delta = AspirationWindowsDelta;

    // After a few depths use a previous result to form the window
    if (depth >= AspirationWindowsDepth)
    {
        alpha = std::max(score - initialWindow, -INFINITE);
        beta  = std::min(score + initialWindow, INFINITE);
    }

    while (true)
    {
        score = alpha_beta<C>(board, timer, alpha, beta, std::max(1, depth), false, si);

        if (stopped)
            break;

        // Search failed low, adjust window and reset depth
        if (score <= alpha)
        {
            alpha = std::max(score - delta, -INFINITE); // alpha/score-delta
            beta  = (alpha + beta) / 2;
            depth = iter_depth;
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

        delta += delta * AspirationWindowsExpand / 10000;
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
int Search::alpha_beta(Board& board, Timer& timer, int alpha, int beta, int depth, bool cut_node, SearchInfo* si)
{
    assert(beta > alpha);

    constexpr Color THEM = ~C;
    const bool isRoot     = (si->ply == 0);

    //  Time-out
    if (stopped || timer.check_limits(iter_depth, index, nodes)) // depth ? td_depth ?
    {
        stopped = true;
        return 0;
    }

    // Ensure a fresh PV
    si->pv.length     = 0;

    /* On a atteint la fin de la recherche
        Certains codes n'appellent la quiescence que si on n'est pas en échec
        ceci amène à une explosion des coups recherchés */
    if (depth <= 0)
        return (quiescence<C>(board, timer, alpha, beta, si));

    // If the position has a move that causes a repetition, and we are losing,
    // then we can cut off early since we can secure a draw
    if (   !isRoot
           && board.get_fiftymove_counter() >= 3
           && alpha < VALUE_DRAW
           && board.upcoming_repetition(si->ply))
    {
        alpha = VALUE_DRAW;
        if (alpha >= beta)
            return alpha;
    }

    //  Position nulle ?
    if (!isRoot && board.is_draw(si->ply))
        return VALUE_DRAW;

    // On a atteint la limite en profondeur de recherche ?
    if (si->ply >= MAX_PLY)
        return evaluate(board);

    // Mate distance pruning
    if (!isRoot)
    {
        alpha = std::max(alpha, -MATE + si->ply);
        beta  = std::min(beta,   MATE - si->ply - 1);
        if (alpha >= beta)
            return alpha;
    }

    // Prefetch La table de transposition aussitôt que possible
    table->prefetch(board.get_key());
    //  Caractéristiques de la position
    const bool isInCheck  = board.is_in_check();
    const bool isExcluded = si->excluded != Move::MOVE_NONE;
    si->threats           = board.squares_attacked<THEM>();

    // For PVS, the node is a PV node if beta - alpha != 1 (full-window = not a null window)
    // We do not want to do most pruning techniques on PV nodes
    // Ne pas confondre avec PV NODE (notation Knuth)
    // Utiliser plutôt la notation : EXACT
    const bool isPV = ((beta - alpha) != 1);

     // Update node count and selective depth
    nodes++;
    seldepth = isRoot ? 0 : std::max(seldepth, si->ply);

    int  score      = -INFINITE;
    int  best_score = -INFINITE;        // initially assume the worst case
    MOVE best_move  = Move::MOVE_NONE;  // meilleur coup local


    //  Check Extension
    if (isInCheck == true && depth + 1 < MAX_PLY)
        depth++;


    //  Recherche de la position actuelle dans la table de transposition
    int   tt_score = VALUE_NONE;
    int   tt_eval  = VALUE_NONE;
    MOVE  tt_move  = Move::MOVE_NONE;
    int   tt_bound = BOUND_NONE;
    int   tt_depth = 0;
    bool  tt_pv    = false;
    bool  tt_hit   = isExcluded ? false : table->probe(board.get_key(), si->ply, tt_move, tt_score, tt_eval, tt_bound, tt_depth, tt_pv);

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
        tbhits++;

        // Check to see if the WDL value would cause a cutoff
        if (    tb_bound == BOUND_EXACT
                || (tb_bound == BOUND_LOWER && tb_score >= beta)
                || (tb_bound == BOUND_UPPER && tb_score <= alpha))
        {
            table->store(board.get_key(), Move::MOVE_NONE, tb_score, VALUE_NONE, tb_bound, depth, si->ply, ttPV);
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
    int raw_eval = VALUE_NONE;
    if (!isExcluded)
    {
        if (isInCheck)
        {
            raw_eval = static_eval = si->static_eval = -MATE + si->ply;
        }
        else
        {
            if (tt_hit && tt_eval != VALUE_NONE)
            {
                raw_eval = tt_eval;
                if (isPV)
                    nnue.lazy_updates(board, nnue.get_accumulator());
            }
            else
            {
                raw_eval = evaluate(board);
            }

            static_eval = si->static_eval = history.corrected_eval(board, raw_eval);

            // Amélioration de static-eval, au cas où tt_score est assez bon
            if(tt_hit
                    && (    tt_bound == BOUND_EXACT
                            || (tt_bound == BOUND_LOWER && tt_score >= static_eval)
                            || (tt_bound == BOUND_UPPER && tt_score <= static_eval) ))
            {
                static_eval = tt_score;
            }
        }
    }
    else
    {
        nnue.lazy_updates(board, nnue.get_accumulator());
        raw_eval = static_eval = si->static_eval;
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
        if (si->ply > 1 && (si-2)->static_eval != VALUE_NONE)
            return si->static_eval > (si - 2)->static_eval;
        if (si->ply > 3 && (si-4)->static_eval != VALUE_NONE)
            return si->static_eval > (si - 4)->static_eval;
        return true;
    }();



    if (!isInCheck && !isRoot && !isPV && !isExcluded)
    {
        //---------------------------------------------------------------------
        //  RAZORING
        //---------------------------------------------------------------------
        if (   depth <= Tunable::RazoringDepth
               && (static_eval + Tunable::RazoringMargin * depth) <= alpha)
        {
            score = quiescence<C>(board, timer, alpha, beta, si);
            if (score <= alpha)
            {
                nodes--;
                return score;
            }
        }

        //---------------------------------------------------------------------
        //  STATIC NULL MOVE PRUNING ou aussi REVERSE FUTILITY PRUNING
        //---------------------------------------------------------------------
        if (
                depth <= Tunable::SNMPDepth
                && abs(beta) < MATE_IN_X
                && board.getNonPawnMaterial<C>())
        {
            int eval_margin = Tunable::SNMPMargin * depth;

            if (static_eval - eval_margin >= beta)
                return static_eval - eval_margin; // Fail Soft
        }

        //---------------------------------------------------------------------
        //  NULL MOVE PRUNING
        //---------------------------------------------------------------------
        if (
                depth >= Tunable::NMPDepth
                && static_eval >= beta
                && (si-1)->move != Move::MOVE_NULL
                && (si-2)->move != Move::MOVE_NULL
                && board.getNonPawnMaterial<C>())
        {
            int R = Tunable::NMPReduction
                    + (Tunable::NMPMargin*depth + std::min<int>(static_eval - beta, Tunable::NMPMax)) / Tunable::NMPDivisor;

            si->move = Move::MOVE_NULL;
            si->cont_hist = &history.continuation_history[0][0];
            board.make_nullmove<C>();
            int null_score = -alpha_beta<~C>(board, timer, -beta, -beta + 1, depth - 1 - R, !cut_node, si+1);
            board.undo_nullmove<C>();

            if (stopped)
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
        int betaCut = beta + Tunable::ProbCutMargin;
        if (   !isInCheck
               && !ttPV
               && depth >= Tunable::ProbCutDepth
               && !(tt_hit && tt_depth >= depth - 3 && tt_score < betaCut))
        {
            MovePicker movePicker(board, history, si, Move::MOVE_NONE, Move::MOVE_NONE, Move::MOVE_NONE, Move::MOVE_NONE, 0);
            MOVE pbMove;

            while ( (pbMove = movePicker.next_move(true).move ) != Move::MOVE_NONE )
            {
                make_move<C, true>(board, pbMove);
                si->move = pbMove;
                si->cont_hist = &history.continuation_history[Move::piece(pbMove)][Move::dest(pbMove)];

                // Teste si une recherche de quiescence donne un score supérieur à betaCut
                int pbScore = -quiescence<~C>(board, timer, -betaCut, -betaCut+1, si+1);

                // Si oui, alors on effectue une recherche normale, avec une profondeur réduite
                if (pbScore >= betaCut)
                    pbScore = -alpha_beta<~C>(board, timer, -betaCut, -betaCut+1, depth-Tunable::ProbcutReduction, cut_node, si+1);

                undo_move<C, true>(board);

                // Coupure si cette dernière recherche bat betaCut
                if (pbScore >= betaCut)
                {
                    table->store(board.get_key(), pbMove, pbScore, static_eval, BOUND_LOWER, depth-3, si->ply, false);
                    return pbScore;
                }
            }
        }

    } // end Pruning

    //---------------------------------------------------------------------
    // Internal Iterative Deepening.
    //---------------------------------------------------------------------
    if ((isPV || cut_node) && depth >= 2+2*cut_node && tt_move == Move::MOVE_NONE)
    {
        depth--;
    }

    si->doubleExtensions = (si-1)->doubleExtensions;

    //====================================================================================
    //  Génération des coups
    //------------------------------------------------------------------------------------
    bool skipQuiets = false;

    MovePicker movePicker(board, history, si, tt_move,
                          si->killer1, si->killer2, history.get_counter_move(si), 0);

    int  bound = BOUND_UPPER;
    int  move_count = 0;
    std::array<MOVE, MAX_MOVES> quiet_moves;
    int  quiet_count = 0;
    std::array<MOVE, MAX_MOVES> capture_moves;
    int  capture_count = 0;
    MOVE move;
    int  hist = 0;  // pas I16 !!

    // Static Exchange Evaluation Pruning Margins
    int  seeMargin[2] = {
        Tunable::SEENoisyMargin * depth * depth,
        Tunable::SEEQuietMargin * depth
    };

    // Futility Pruning Margin
    int futility_pruning_margin = static_eval + Tunable::FPMargin * depth;

    // Boucle sur tous les coups
    while ( (move = movePicker.next_move(skipQuiets).move ) != Move::MOVE_NONE )
    {
        if (move == si->excluded)
            continue;

        const U64  starting_nodes = nodes;
        const bool isQuiet   = !Move::is_tactical(move);    // capture, promotion (avec capture ou non), prise en-passant

        move_count++;

        if (isQuiet)
            hist = history.get_quiet_history(C, si, move, board.get_pawn_key());
        else
            hist = history.get_capture_history(move);

        //-------------------------------------------------
        // Futility Pruning.
        //-------------------------------------------------
        if (   !isRoot
               &&  isQuiet
               &&  best_score > -TBWIN_IN_X
               &&  futility_pruning_margin <= alpha
               &&  depth <= Tunable::FPDepth
               &&  hist < (Tunable::FPHistoryLimit - Tunable::FPHistoryLimitImproving*improving) )
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
               &&  quiet_count > LateMovePruningCount[improving][depth])
        {
            skipQuiets = true;
            continue;
        }

        //-------------------------------------------------
        // History Pruning.
        // Prune moves with a low history score moves at near-leaf nodes
        //-------------------------------------------------
        if (   !isRoot
               &&  isQuiet
               &&  best_score > -TBWIN_IN_X
               &&  depth <= (Tunable::HistoryPruningDepth - improving)
               && hist < -Tunable::HistoryPruningLimit * depth)
        {
            continue;
        }

        //-------------------------------------------------
        //  Static Exchange Evaluation Pruning
        //-------------------------------------------------
        if (   !isRoot
               &&  best_score > -TBWIN_IN_X
               &&  depth <= Tunable::SEEPruningDepth
               &&  movePicker.get_stage() > STAGE_GOOD_NOISY
               && !board.fast_see(move, seeMargin[isQuiet]))
        {
            continue;
        }

        //-------------------------------------------------
        //  SINGULAR EXTENSION
        //-------------------------------------------------
        int extension = 0;

        if (   depth > Tunable::SEDepth
               && si->ply < 2 * depth
               && !isExcluded  // Avoid recursive singular search
               && !isRoot
               && move == tt_move
               && tt_depth > depth - 3
               && tt_bound == BOUND_LOWER
               && abs(tt_score) < TBWIN_IN_X)
        {
            // Search to reduced depth with a zero window a bit lower than ttScore
            int sing_beta  = tt_score - depth*2;
            int sing_depth = (depth-1)/2;

            si->excluded = move;
            int SE_score = alpha_beta<C>(board, timer, sing_beta-1, sing_beta, sing_depth, cut_node, si);
            si->excluded = Move::MOVE_NONE;

            if (SE_score < sing_beta)
            {
                if (   !isPV
                       && SE_score < sing_beta - 50
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
                extension = -3 + isPV;
            }
            else if (cut_node)
            {
                extension = -2;
            }
        }


        // execute current move
        si->move = move;
        si->cont_hist = &history.continuation_history[Move::piece(move)][Move::dest(move)];
        make_move<C, true>(board, move);

        if (isQuiet)
        {
            assert(quiet_count+1 < quiet_moves.size());
            quiet_moves[quiet_count++] = move;
        }
        else
        {
            assert(capture_count+1 < capture_moves.size());
            capture_moves[capture_count++] = move;
        }

        //------------------------------------------------------------------------------------
        //  LATE MOVE REDUCTION
        //------------------------------------------------------------------------------------
        int newDepth = depth - 1 + extension;

        if (   depth > 2
               && move_count > (2 + isPV) )
        {
            // Base reduction
            int R = Reductions[isQuiet][std::min(31, depth)][std::min(31, move_count)];

            // Reduce less in pv nodes
            R -= ttPV + isPV;

            // Reduce less when improving
            R += !improving;

            if (cut_node)
                R += 2 - ttPV;

            // Reduce quiets more if ttMove is a capture
            R += Move::is_capturing(tt_move);

            // Reduce more when opponent has few pieces
            R += board.getNonPawnMaterialCount<THEM>() < 2;

            // Adjust based on history
            R -= std::max(-2, std::min(2, hist / Tunable::LMR_HistReductionDivisor));

            // Depth after reductions, avoiding going straight to quiescence
            int lmrDepth = std::clamp(newDepth - R, 1, newDepth + 1);

            // Search this move with reduced depth:
            score = -alpha_beta<~C>(board, timer, -alpha-1, -alpha, lmrDepth, true, si+1);

            // Do full depth search when reduced LMR search fails high
            if (score > alpha && lmrDepth < newDepth)
            {
                newDepth += (score > best_score + Tunable::LMR_DeeperMargin + Tunable::LMR_DeeperScale*newDepth);
                newDepth -= (score < best_score + Tunable::LMR_ShallowerMargin);

                if (lmrDepth < newDepth)
                {
                    score = -alpha_beta<~C>(board, timer, -alpha-1, -alpha, newDepth, !cut_node, si+1);
                }

                history.update_continuation_history(si, move, score, alpha, beta, newDepth);
            }
        }
        else if (!isPV || move_count > 1)
        {
            score = -alpha_beta<~C>(board, timer, -alpha-1, -alpha, newDepth, !cut_node, si+1);
        }

        if (isPV && (move_count == 1 || score > alpha))
        {
            score = -alpha_beta<~C>(board, timer, -beta, -alpha, newDepth, false, si+1);
        }

        // retract current move
        undo_move<C, true>(board);

        // Track where nodes were spent in the Main thread at the Root
        if (isRoot && index==0)
            timer.updateMoveNodes(move, nodes - starting_nodes);

        //  Time-out
        if (stopped)
            return 0;

        // On a trouvé un nouveau meilleur coup
        if (score > best_score)
        {
            best_score = score; // le meilleur que l'on ait trouvé, pas forcément intéressant

            // If score beats alpha we update alpha
            if (score > alpha)
            {
                best_move  = move;
                alpha = score;
                bound = BOUND_EXACT;

                // update the PV
                if (index == 0)
                    update_pv(si, move);

                // If score beats beta we have a cutoff
                if (score >= beta)
                {
                    history.update_quiet_history(C, si, best_move, board.get_pawn_key(), depth, quiet_count, quiet_moves);
                    history.update_capture_history(best_move, depth, capture_count, capture_moves);

                    // non, ce coup est trop bon pour l'adversaire
                    // ça ne sert à rien de rechercher plus loin
                    bound = BOUND_LOWER;
                    break;
                }
            } // score > alpha
        }     // meilleur coup
    }         // boucle sur les coups

    // est-on mat ou pat ?
    if (move_count == 0)
    {
        if (isExcluded)
            return alpha;

        // On est en échec, et on n'a aucun coup : on est MAT
        // On n'est pas en échec, et on n'a aucun coup : on est PAT
        return isInCheck ? -MATE + si->ply : 0;
    }

    best_score = std::min(best_score, max_score);

    if(   !isInCheck
          && (best_move == Move::MOVE_NONE || !Move::is_capturing(best_move))
          && !(bound == BOUND_LOWER && best_score <= si->static_eval)
          && !(bound == BOUND_UPPER && best_score >= si->static_eval))
    {
        history.update_correction_history(board, depth, best_score, static_eval );
    }

    if (!stopped && !isExcluded)
    {
        //  si on est ici, c'est que l'on a trouvé au moins 1 coup
        //  et de plus : score < beta
        //  si score >  alpha    : c'est un bon coup : HASH_EXACT
        //  si score <= alpha    : c'est un coup qui n'améliore pas alpha : HASH_ALPHA

        table->store(board.get_key(), best_move, best_score, static_eval, bound, depth, si->ply, ttPV);
    }

    return best_score;
}

template void Search::think<WHITE>(Board board, Timer timer, size_t _index);
template void Search::think<BLACK>(Board board, Timer timer, size_t _index);

template int Search::aspiration_window<WHITE>(Board& board, Timer& timer, SearchInfo* si);
template int Search::aspiration_window<BLACK>(Board& board, Timer& timer, SearchInfo* si);
