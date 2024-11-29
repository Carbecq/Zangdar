#include <string>
#include "DataGen.h"
#include "defines.h"
#include <cstring>
#include <fstream>
#include <random>

#include "Board.h"
#include "Search.h"
#include "ThreadPool.h"
#include "TranspositionTable.h"

//  Code inspiré par plusieurs codes puis adapté et commenté
//      Midnight, Illumina, Clover, Clarity, SmallBrain

// #define DEBUG_GEN

//===========================================================================
//! \brief  Constructeur
//! \param[in]  _nbr    nombre de threads demandé
//! \param[in]  _depth  profondeur de recherche
//! \param[in]  _output répertoire de sortie des fichers
//---------------------------------------------------------------------------
DataGen::DataGen(const int nbr, const int max_fens, const std::string& output)
{
    printf("DataGen : nbr_threads=%d max_fens=%d output=%s \n", nbr, max_fens, output.c_str());

    //================================================
    //  Initialisations
    //================================================
    int nbr_threads = set_threads(nbr);
    std::atomic<Usize> total_fens{0};           // nombre de fens écrites par toutes les threads

    //================================================
    //  Lancement de la génération par thread
    //================================================
    for (int i = 0; i < nbr_threads; i++)
    {
        std::string str_file(output);
        str_file += "/data" + std::to_string(i) + ".txt";

        threads.emplace_back(
            [this, i, str_file, max_fens, &total_fens] {
                datagen(i, str_file, max_fens, total_fens);
            });
    }

    //================================================
    //  Fin du job
    //================================================
    for (auto& t : threads)
        t.join();
}

//====================================================================
//! \brief  Lancement d'une génération de données , dans une thread donnée
//--------------------------------------------------------------------
void DataGen::datagen(int thread_id, const std::string& str_file,
                      const Usize max_fens,
                      std::atomic<Usize>& total_fens)
{
    printf("thread id = %d : max_fens=%zu out=%s \n", thread_id, max_fens, str_file.c_str());

    std::ofstream file;
    file.open(str_file, std::ios::out);     //app = append ; out = write
    if (file.is_open() == false)
    {
        std::cout << "file " << str_file << "not opened" << std::endl;
        return;
    }

    std::array<fenData, 10000> fens{};  // liste des positions conservées, pour une partie donnée
    int nbr_fens;                       // nombre de positions conservées

    // Set the time manager
    // On va utiliser une limite par "node",
    // sauf dans "random_game" où on va utilisre une limite par "depth".
    Timer timer;
    timer.setup(SOFT_NODE_LIMIT, HARD_NODE_LIMIT);
    auto start = std::chrono::high_resolution_clock::now();

    //================================================
    //  Boucle infinie
    //  On simule l'exécution de N parties
    //================================================

    std::random_device rd;
    std::mt19937_64 generator(rd());

    /*  TODO
     *  TT : ne PAS la partager entre threads
     *
     *
     */

    MoveList movelist;
    Usize    nbr_moves;
    Board    board(START_FEN);
    int      drawCount = 0;
    int      winCount = 0;

    double  result = 0;  // 0.5 for draw, 1.0 for white win, 0.0 for black win. = ~Color = 1 - Color
    MOVE    move;
    I32     score;
    // Usize   nbr_games = 0;

    while (total_fens < max_fens)
    {
        // printf("-------------------------------------------nouvelle partie : total=%lu max=%lu \n", total_fens.load(), max_fens);
        board.set_fen(START_FEN, false);

        //  Initalisation random de l'échiquier
        random_game(board);

        drawCount   = 0;
        winCount    = 0;
        nbr_fens    = 0;

        movelist.clear();                   // liste des coups

        /*  clear history
         *  new hashtable
         */
        // printf("----------------------------random ok \n");

        // Boucle de jeu
        while (true)
        {
            // printf("----------------------------nouveau coup \n");

            // Partie nulle ?
            if (board.is_draw(0))   //TODO vérifier ce 0
            {
                result = 0.5;
                break;
            }

            if (board.turn() == WHITE)
                board.legal_moves<WHITE, MoveGenType::ALL>(movelist);
            else
                board.legal_moves<BLACK, MoveGenType::ALL>(movelist);
            nbr_moves = movelist.size();

            // Aucun coup possible
            if (!nbr_moves) {
                if (board.is_in_check()) {
                    // 0.0 : Noir gagne ; 1.0 : Blanc gagne
                    result = (board.turn() == WHITE ? 0.0 : 1.0);
                }
                else {
                    result = 0.5;
                }

                break;
            }

            // thread_data.TT->age();
            // thread_data.board.clear();

            if (board.turn() == WHITE)
                data_search<WHITE>(board, timer, move, score);
            else
                data_search<BLACK>(board, timer, move, score);

            if (nbr_moves == 1) { /// in this case, engine reports score 0, which might be misleading
                // thread_data.make_move(move);
                if (board.turn() == WHITE)
                    board.make_move<WHITE>(move);
                else
                    board.make_move<BLACK>(move);
                continue;
            }

            //  Filtrage de la position
            //  On cherche des positions tranquilles
            if (   !board.is_in_check()
                && !Move::is_capturing(move)
                && abs(score) < HIGH_SCORE )
            {
                fens[nbr_fens++] = { board.get_fen() , score * (1 - 2*board.turn()) };
                total_fens++;
                if (/* thread_id==0 && */ total_fens % 100 == 0)
                {
                    auto elapsed = ( std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start ).count() + 1.00)/1000.0;
                    std::cout << "total_fens_collected = " << total_fens << " ; fens / sec = "
                              << total_fens / elapsed << std::endl;
                }
            }

            // Adjucation de la partie
            winCount  = (std::abs(score) >= HIGH_SCORE ? winCount  + 1 : 0);
            drawCount = (std::abs(score) <= LOW_SCORE  ? drawCount + 1 : 0);

            if (winCount >= MIN_WIN_COUNT || std::abs(score) >= MATE_IN_X) {
                score *= (board.turn() == WHITE ? 1 : -1);
                result = (score < 0 ? 0.0 : 1.0);
                break;
            }

            if (drawCount >= MIN_DRAW_COUNT) {
                result = 0.5;
                break;
            }

            // Exécution du coup trouvé lors de la recherche
            if (board.turn() == WHITE)
                board.make_move<WHITE>(move);
            else
                board.make_move<BLACK>(move);

        } // fin de la partie

        // nbr_games++;

        // Ecriture dans le fichier des fens collectées dans cette partie
        for (int i = 0; i < nbr_fens; i++) {
            file << fens[i].fen << " | "
                 << fens[i].score << " | "
                 << (result > 0.6 ? "1.0" : (result < 0.4 ? "0.0" : "0.5")) <<  " | "
                 // << "nbr_games=" << nbr_games <<  " | "
                 // << "Noir gagne : 0.0 ; Blanc gagne : 1.0 "
                 << "\n";
        }
        // file << "\n";

    } // fin des parties

    // std::cout << "nombre de parties = " << nbr_games << std::endl;

    // dernière étape : on ferme le fichier
    file.close();
}

//================================================
//  Initialisation de l'échiquier
//  en jouant quelques coups au hasard
//------------------------------------------------
void DataGen::random_game(Board& board)
{
    MoveList movelist;
    MOVE move  = Move::MOVE_NONE;
    I32  score = -INFINITE;
    bool board_generated = false;

    std::random_device rd;
    std::mt19937_64 generator(rd());

    // Set the time manager
    Timer timer(false, 0, 0, 0, 0, 0, MAX_RANDOM_DEPTH, 0, 0);
    timer.setup(Color::WHITE);

    while (!board_generated)
    {
        board.set_fen(START_FEN, false);
        movelist.clear();

        for (Usize idx = 0; idx < MAX_RANDOM_PLIES; idx++)
        {
            board_generated = true;
            if (board.turn() == WHITE)
            {
                board.legal_moves<WHITE, MoveGenType::ALL>(movelist);

                if (movelist.size() == 0)
                {
                    board_generated = false;
                    break;
                }
                std::uniform_int_distribution<> distribution{0, int(movelist.size() - 1)};
                const int index = distribution(generator);
                board.make_move<WHITE>(movelist.mlmoves[index].move);
            }
            else
            {
                board.legal_moves<BLACK, MoveGenType::ALL>(movelist);

                if (movelist.size() == 0)
                {
                    board_generated = false;
                    break;
                }
                std::uniform_int_distribution<> distribution{0, int(movelist.size() - 1)};
                const int index = distribution(generator);
                board.make_move<BLACK>(movelist.mlmoves[index].move);
            }
        }

        // Evaluation de la position, en fin des MAX_RANDOM_MOVES moves
        // Si elle est trop déséquilibrée, on recommence
        if (board.turn() == WHITE)
            data_search<WHITE>(board, timer, move, score);
        else
            data_search<BLACK>(board, timer, move, score);

        if (std::abs(score) > MAX_RANDOM_SCORE)
        {
            board_generated = false;
            continue;
        }
    }
}

//============================================================================
//  Recherche dans la position actuelle
//----------------------------------------------------------------------------
template <Color C>
void DataGen::data_search(Board& board, Timer& timer, MOVE &move, I32 &score)
{
    ThreadData td;

    td.info        = &(td._info[STACK_OFFSET]);
    SearchInfo* si = &td.info[0];

    td.search   = nullptr;
    td.index    = 0;
    td.depth    = 0;
    td.seldepth = 0;
    td.nodes    = 0;
    td.stopped  = false;
    td.tbhits   = 0;

    transpositionTable.clear();
    td.reset();
#if defined USE_NNUE
    board.nnue.reset();
#endif
    move  = Move::MOVE_NONE;
    score = -INFINITE;

    //==================================================
    // iterative deepening
    //==================================================
    td.search = new Search();

    si->pv.length = 0;
    si->pv.score  = -INFINITE;

#if defined DEBUG_GEN
    timer.debug(WHITE);
    timer.start();
#endif

    for (td.depth = 1; td.depth <= std::max(1, timer.getSearchDepth()); td.depth++)
    {
        // Search position, using aspiration windows for higher depths
        td.search->aspiration_window<C>(board, timer, &td, si);

        if (td.stopped)
            break;

        // L'itération s'est terminée sans problème
        // On peut mettre à jour les infos UCI
        td.best_depth    = td.depth;
        td.pvs[td.depth] = si->pv;

#if defined DEBUG_GEN
        auto elapsed = timer.elapsedTime();

        td.search->show_uci_result(&td, elapsed);
        std::cout << "   nodes " << td.nodes << std::endl;
#else
        auto elapsed = 0;
#endif

        // timer.update(td.depth, td.pvs);

        // If an iteration finishes after optimal time usage, stop the search
        if (timer.finishOnThisDepth(elapsed, td.depth, td.pvs, td.nodes))
            break;

        td.seldepth = 0;
    }

    move  = td.get_best_move();
    score = td.get_best_score();
}

//===================================================
//  Calcul du nombre de threads
//---------------------------------------------------
int DataGen::set_threads(const int nbr)
{
    int nbr_threads = nbr;

    int processorCount = static_cast<int>(std::thread::hardware_concurrency());
    // Check if the number of processors can be determined
    if (processorCount == 0)
        processorCount = MAX_THREADS;

    // Clamp the number of threads to the number of processors
    nbr_threads     = std::min(nbr, processorCount);
    nbr_threads     = std::max(nbr_threads, 1);
    nbr_threads     = std::min(nbr_threads, MAX_THREADS);

    return nbr_threads;
}

template void DataGen::data_search<WHITE>(Board& board, Timer& timer, MOVE& move, I32& score);
template void DataGen::data_search<BLACK>(Board& board, Timer& timer, MOVE& move, I32& score);
