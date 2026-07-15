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
//! \param[in]  board     position de départ de la recherche (copie locale à la thread)
//! \param[in]  timer     gestion du temps de la recherche
//! \param[in]  m_index   indice de la thread qui exécute cette recherche
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

    // Réinitialise la table LMR (nécessaire car les TunableParam
    // peuvent ne pas être initialisés lors de la construction globale,
    // et aussi pour prendre en compte les changements via setoption)
#if defined USE_TUNING
    init_reductions();
#endif

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

        // Sélection et affichage du meilleur résultat parmi toutes les threads
        if (threadPool.get_logUci())
        {
            const int     bt  = threadPool.get_best_thread();
            const Search& bts = threadPool.search[bt];

            // Si le meilleur résultat vient d'une thread helper, la dernière
            // ligne "info" affichée est celle de la thread 0 et son premier
            // coup peut différer du bestmove. On réaffiche donc la PV de la
            // thread retenue avant le bestmove.
            if (bt != 0 && bts.last_pv.length > 0)
                bts.show_uci_result(timer.elapsedTime(), bts.last_pv);

            show_uci_best(bts.pv_moves[bts.best_depth]);
        }

        table->update_age();
    }
}

//======================================================
//! \brief  iterative deepening
//!
//! Boucle sur les profondeurs croissantes, en s'appuyant
//! sur aspiration_window() pour chaque itération, et gère
//! l'affichage UCI et le time management pour la thread principale.
//!
//! \param[in,out]  board   position racine de la recherche
//! \param[in,out]  timer   gestion du temps de la recherche
//! \param[in,out]  si      pile d'information de recherche (nœud racine)
//------------------------------------------------------
template<Color C>
void Search::iterative_deepening(Board& board, Timer& timer, SearchInfo* si)
{
    int prev_score = -INFINITE;

    for (iter_depth = 1; iter_depth <= timer.getSearchDepth(); iter_depth++)
    {
        // Recherche la position, avec des aspiration windows pour les profondeurs élevées
        const int score = aspiration_window<C>(board, timer, si, prev_score);

        if (is_stopped())
            break;

        // L'itération s'est terminée sans problème
        // Toutes les threads sauvegardent leur meilleur résultat
        best_depth = iter_depth;

        // Historique par profondeur
        pv_scores[iter_depth] = score;
        pv_moves [iter_depth] = si->pv.line[0];
        last_pv               = si->pv;

        prev_score = score;

        // Seule la thread principale gère l'affichage UCI et le time management
        if (index == 0)
        {
            auto elapsed = timer.elapsedTime();

            if (threadPool.get_logUci())
                show_uci_result(elapsed, si->pv);

            // Mise à jour de la stabilité de la PV
            timer.update(iter_depth, pv_moves[iter_depth-1], pv_moves[iter_depth]);

            // Si une itération se termine après le temps optimal, on arrête la recherche
            if (timer.finishOnThisDepth(elapsed, iter_depth, nodes, pv_scores, pv_moves))
                break;

            seldepth = 0;
        }
    }
}

//======================================================
//! \brief  Aspiration Window
//!
//! Recherche alpha_beta() avec une fenêtre initialement
//! resserrée autour du score de l'itération précédente,
//! ré-élargie progressivement en cas de fail low/fail high.
//!
//! \param[in,out]  board        position racine de la recherche
//! \param[in,out]  timer        gestion du temps de la recherche
//! \param[in,out]  si           pile d'information de recherche (nœud racine)
//! \param[in]      prev_score   score obtenu à l'itération précédente
//!
//! \return Score de la position à la profondeur iter_depth
//------------------------------------------------------
template<Color C>
int Search::aspiration_window(Board& board, Timer& timer, SearchInfo* si, int prev_score)
{
    int alpha  = -INFINITE;
    int beta   = INFINITE;
    int depth  = iter_depth;
    int delta  = Tunable::AspirationWindowsDelta;
    int score  = prev_score;
    const int initialWindow = Tunable::AspirationWindowsInitial;

    // Après quelques profondeurs, on utilise un résultat précédent pour former la fenêtre
    if (depth >= Tunable::AspirationWindowsDepth)
    {
        alpha = std::max(score - initialWindow, -INFINITE);
        beta  = std::min(score + initialWindow, INFINITE);
    }

    while (true)
    {
        score = alpha_beta<C>(board, timer, alpha, beta, std::max(1, depth), false, si);

        if (is_stopped())
            break;

        // Fail low : on élargit la fenêtre vers le bas et on réinitialise la profondeur
        if (score <= alpha)
        {
            alpha = std::max(score - delta, -INFINITE); // alpha/score-delta
            beta  = (alpha + beta) / 2;
            depth = iter_depth;
        }

        // Fail high : on élargit la fenêtre vers le haut et on réduit la profondeur
        else if(score >= beta)
        {
            beta  = std::min(score + delta, INFINITE);   // beta/score+delta
            // idée de Berserk
            if (abs(score) < TBWIN_IN_X)
                depth = std::max(depth-1, 1);
        }

        // Le score est dans les bornes : il est accepté comme correct
        else
        {
            return score;
        }

        delta += delta * Tunable::AspirationWindowsExpand / 10000;
    }

    return score;
}

//=====================================================
//! \brief  Recherche du meilleur coup
//!
//!     Méthode Apha-Beta - Fail Soft
//!
//! \param[in,out]  board      position courante de la recherche
//! \param[in,out]  timer      gestion du temps de la recherche
//! \param[in]      alpha      borne inférieure : on est garanti d'avoir au moins alpha
//! \param[in]      beta       borne supérieure : meilleur coup pour l'adversaire
//! \param[in]      depth      profondeur maximum de recherche
//! \param[in]      cut_node   true si ce nœud est attendu comme un nœud de coupure (fail-high)
//! \param[in,out]  si         pile d'information de recherche (nœud courant, PV mise à jour)
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
    if (is_stopped() || timer.check_limits(iter_depth, index, nodes))
    {
        signal_stop();
        return 0;
    }

    // Ensure a fresh PV
    si->pv.length     = 0;

    // Hindsight : par défaut, ce nœud n'a encore réduit aucun coup enfant
    si->reduction     = 0;

    /* On a atteint la fin de la recherche
        Certains codes n'appellent la quiescence que si on n'est pas en échec
        ceci amène à une explosion des coups recherchés */
    if (depth <= 0)
        return (quiescence<C>(board, timer, alpha, beta, si));

    // Si la position a un coup qui provoque une répétition, et que l'on perd,
    // on peut couper tôt puisqu'on peut s'assurer la nulle
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

    // Pour la PVS, le nœud est un PV node si beta - alpha != 1 (full-window = pas une null window)
    // On ne veut pas appliquer la plupart des techniques de pruning sur les PV nodes
    // Ne pas confondre avec PV NODE (notation Knuth)
    // Utiliser plutôt la notation : EXACT
    const bool isPV = ((beta - alpha) != 1);

     // Met à jour le compteur de nodes et la profondeur sélective
    nodes++;
    seldepth = isRoot ? 0 : std::max(seldepth, si->ply);

    int  score      = -INFINITE;
    int  best_score = -INFINITE;        // on suppose d'abord le pire cas
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

    // On fait confiance à la TT si ce n'est pas un pvnode et que la profondeur
    // de l'entrée est suffisamment élevée.
    // Dans les nœuds non-PV, on vérifie une coupure TT anticipée
    //    (on ne retourne pas immédiatement sur une fenêtre PVS complète,
    //     car cela raccourcirait la ligne PV)
    if (tt_hit && !isPV && tt_depth >= depth)
    {
        if (   (tt_bound == BOUND_EXACT)
               || (tt_bound == BOUND_LOWER && tt_score >= beta)
               || (tt_bound == BOUND_UPPER && tt_score <= alpha))
        {
            return tt_score;
        }
    }


    // Sonde les tablebases Syzygy
    int tb_score = VALUE_NONE;
    int tb_bound = BOUND_NONE;
    int max_score = MATE;
    const bool ttPV = isPV || tt_pv;

    if (!isExcluded && threadPool.get_useSyzygy() && board.probe_wdl(tb_score, tb_bound, si->ply) == true)
    {
        tbhits++;

        // Vérifie si la valeur WDL provoquerait une coupure
        if (    tb_bound == BOUND_EXACT
                || (tb_bound == BOUND_LOWER && tb_score >= beta)
                || (tb_bound == BOUND_UPPER && tb_score <= alpha))
        {
            table->store(board.get_key(), Move::MOVE_NONE, tb_score, VALUE_NONE, tb_bound, depth, si->ply, ttPV);
            return tb_score;
        }

        // Limite le score de ce nœud d'après le résultat des tablebases
        if (isPV)
        {
            // Ne jamais donner un score pire que la valeur Syzygy connue
            if (tb_bound == BOUND_LOWER)
            {
                best_score = tb_score;
                alpha = std::max(alpha, tb_score);
            }

            // Ne jamais donner un score meilleur que la valeur Syzygy connue
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


    //---------------------------------------------------------------------
    //  HINDSIGHT EXTENSION / REDUCTION
    //---------------------------------------------------------------------
    const int opp_worsening_rate = (isRoot || isInCheck)
                                 ? 0
                                 : si->static_eval + (si-1)->static_eval;

    if (   !isRoot && !isInCheck && !isExcluded
        && abs((si-1)->static_eval) < TBWIN_IN_X)   // parent en échec → static_eval = -MATE+ply, à exclure
    {
        if (   depth >= Tunable::HindsightExtMinDepth
            && (si-1)->reduction >= Tunable::HindsightExtMinReduction
            && opp_worsening_rate < Tunable::HindsightExtEvalDiff)
        {
            depth++;
        }
        else if (   !isPV
                 && depth >= Tunable::HindsightRedMinDepth
                 && (si-1)->reduction >= Tunable::HindsightRedMinReduction
                 && opp_worsening_rate > Tunable::HindsightRedEvalDiff)
        {
            depth--;
        }
    }


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
            // corrplexity : différence entre eval brute et eval corrigée par correction history.
            // Positif → on surestimait → marge plus grande (pruning moins agressif).
            // Négatif → on sous-estimait → marge plus petite (pruning plus agressif).
            int corrplexity = raw_eval - si->static_eval;
            int eval_margin = Tunable::SNMPMargin * (depth - improving)
                            + corrplexity * Tunable::SNMPCorrplexityScale / 128;

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
                && board.getNonPawnMaterial<C>())           // protection contre le zugzwang
        {
            int R = Tunable::NMPReduction
                    + (Tunable::NMPMargin*depth + std::min<int>(static_eval - beta, Tunable::NMPMax)) / Tunable::NMPDivisor
                    + (si-1)->tactical;     // le coup adverse précédent était tactique → réduire un cran de plus

            si->move = Move::MOVE_NULL;
            si->tactical = false;
            si->cont_hist = &history.continuation_history[0][0];

            board.make_nullmove<C>();
            int null_score = -alpha_beta<~C>(board, timer, -beta, -beta + 1, depth - R, !cut_node, si+1);
            board.undo_nullmove<C>();

            if (is_stopped())
                return 0;

            // Cutoff
            if (null_score >= beta)
            {
                // (Stockfish) : on ne retourne pas un score proche du mat
                //               car ce score ne serait pas prouvé
                return(null_score >= TBWIN_IN_X ? beta : null_score);
            }
            // Hindsight NMP : le parent nous a réduits en LMR, mais le
            // null move échoue franchement → le nœud est sous-estimé, on rend un ply.
            else if (   (si-1)->reduction
                     && abs(null_score) < TBWIN_IN_X
                     && null_score < beta - Tunable::NMPHindsightMargin)
            {
                depth++;
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
                si->tactical = true;        // ProbCut ne joue que des coups tactiques
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
                    table->store(board.get_key(), pbMove, pbScore, raw_eval, BOUND_LOWER, depth-(Tunable::ProbcutReduction-1), si->ply, false);
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
            hist = history.get_capture_history(si, move);

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
        // L'idée : un coup quiet avec un historique très négatif est inutile à chercher :
        // c'est un coup que le moteur a démontré comme mauvais dans des positions similaires.
        // À faible profondeur, on le prune.
        //-------------------------------------------------
        if (   !isRoot
            && isQuiet
            && best_score > -TBWIN_IN_X
            && depth <= (Tunable::HistoryPruningDepth - improving)
            && hist  < -(Tunable::HistoryPruningScale * depth))
        {
            skipQuiets = true;
            continue;
        }

        //-------------------------------------------------
        //  Static Exchange Evaluation Pruning
        //-------------------------------------------------
        if (   !isRoot
               &&  best_score > -TBWIN_IN_X
               &&  depth <= Tunable::SEEPruningDepth
               &&  movePicker.get_stage() > STAGE_GOOD_NOISY
               && !board.fast_see(move, seeMargin[isQuiet] - hist / Tunable::SEEHistScale))
        {
            continue;
        }

        //-------------------------------------------------
        //  SINGULAR EXTENSION
        //-------------------------------------------------
        int extension = 0;

        if (   depth > Tunable::SEDepth
               && si->ply < 2 * depth
               && !isExcluded  // évite une recherche singulière récursive
               && !isRoot
               && move == tt_move
               && tt_depth > depth - 3
               && tt_bound == BOUND_LOWER
               && abs(tt_score) < TBWIN_IN_X)
        {
            // Recherche à profondeur réduite avec une zero window un peu sous ttScore
            int sing_beta  = tt_score - depth*2;
            int sing_depth = (depth-1)/2;

            si->excluded = move;
            int SE_score = alpha_beta<C>(board, timer, sing_beta-1, sing_beta, sing_depth, cut_node, si);
            si->excluded = Move::MOVE_NONE;

            if (SE_score < sing_beta)
            {
                if (   !isPV
                       && SE_score < sing_beta - 50
                       && si->doubleExtensions <= 20)  // évite une explosion de la recherche en limitant le nombre de double extensions
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


        // joue le coup courant
        si->move = move;
        si->tactical = !isQuiet;
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
            // Réduction de base
            int R = Reductions[isQuiet][std::min(31, depth)][std::min(31, move_count)];

            // Réduit moins dans les PV nodes
            R -= ttPV + isPV;

            // Réduit moins quand on improving
            R += !improving;

            if (cut_node)
                R += 2 - ttPV;

            // Réduit plus si ttMove est une capture
            R += Move::is_capturing(tt_move);

            // Réduit plus quand l'adversaire a peu de pièces
            R += board.getNonPawnMaterialCount<THEM>() < 2;

            // Ajuste en fonction de l'history
            R -= std::max(-2, std::min(2, hist / Tunable::LMR_HistReductionDivisor));

            // Profondeur après réductions, en évitant de tomber directement en quiescence
            // TODO vérifier ce newDepth+1
            int lmrDepth = std::clamp(newDepth - R, 1, newDepth + 1);

            // Recherche ce coup à profondeur réduite :
            // On mémorise la réduction pour la hindsight ext/red de l'enfant
            si->reduction = R;
            score = -alpha_beta<~C>(board, timer, -alpha-1, -alpha, lmrDepth, true, si+1);
            si->reduction = 0;

            // Refait une recherche à pleine profondeur si la recherche LMR réduite fail high
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

        // annule le coup courant
        undo_move<C, true>(board);

        // Suit où les nodes ont été dépensés dans la thread principale, à la racine
        if (isRoot && index==0)
            timer.updateMoveNodes(move, nodes - starting_nodes);

        //  Time-out
        if (is_stopped())
            return 0;

        // On a trouvé un nouveau meilleur coup
        if (score > best_score)
        {
            best_score = score; // le meilleur que l'on ait trouvé, pas forcément intéressant

            // Si le score dépasse alpha, on met à jour alpha
            if (score > alpha)
            {
                best_move  = move;
                alpha = score;
                bound = BOUND_EXACT;

                // met à jour la PV
                update_pv(si, move);

                // Si le score dépasse beta, on a une coupure
                if (score >= beta)
                {
                    history.update_quiet_history(C, si, best_move, board.get_pawn_key(), depth, quiet_count, quiet_moves);
                    history.update_capture_history(si, best_move, depth, capture_count, capture_moves);

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

    if (!is_stopped() && !isExcluded)
    {
        //  si on est ici, c'est que l'on a trouvé au moins 1 coup
        //  et de plus : score < beta
        //  si score >  alpha    : c'est un bon coup : HASH_EXACT
        //  si score <= alpha    : c'est un coup qui n'améliore pas alpha : HASH_ALPHA

        table->store(board.get_key(), best_move, best_score, raw_eval, bound, depth, si->ply, ttPV);
    }

    return best_score;
}

template void Search::think<WHITE>(Board board, Timer timer, size_t _index);
template void Search::think<BLACK>(Board board, Timer timer, size_t _index);

template int Search::aspiration_window<WHITE>(Board& board, Timer& timer, SearchInfo* si, int prev_score);
template int Search::aspiration_window<BLACK>(Board& board, Timer& timer, SearchInfo* si, int prev_score);
