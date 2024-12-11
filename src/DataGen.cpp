
//  Code inspiré par plusieurs codes puis adapté et commenté
//      Midnight, Illumina, Clover, Clarity, SmallBrain

// #define DEBUG_GEN

#include <string>
#include "DataGen.h"
#include "defines.h"
#include <cstring>
#include <fstream>
#include <random>

#include "Board.h"
#include "Search.h"
#include "ThreadPool.h"
#include "pyrrhic/tbprobe.h"

//===========================================================================
//! \brief  Constructeur
//! \param[in]  _nbr    nombre de threads demandé
//! \param[in]  _depth  profondeur de recherche
//! \param[in]  _output répertoire de sortie des fichers
//---------------------------------------------------------------------------
DataGen::DataGen(const int nbr, const int max_games, const std::string& output)
{
    printf("DataGen : nbr_threads=%d max_fens=%d output=%s \n", nbr, max_games, output.c_str());

    //================================================
    //  Initialisations
    //================================================
    int nbr_threads = set_threads(nbr);
    // std::atomic<Usize> total_fens{0};           // nombre de fens écrites par toutes les threads

    // tb_init("/mnt/Datas/Echecs/Syzygy/");
    // threadPool.set_useSyzygy(true);

    //================================================
    //  Lancement de la génération par thread
    //================================================
    std::vector<std::thread> threads;
    for (int i = 0; i < nbr_threads; i++)
    {
        std::string str_file(output);
        str_file += "/data" + std::to_string(i) + ".txt";

        // threads.emplace_back(
        //     // &DataGen::genfens, this, str_file, max_fens);
        //     [this, i, str_file, max_games, &total_fens] {
        //         genfens(i, str_file, max_games, total_fens);
        //     });

        threads.emplace_back(
            // &DataGen::genfens, this, str_file, max_fens);
            [this, i, str_file, max_games] {
                genfens(i, str_file, max_games);
            });

    }

    //================================================
    //  Fin du job
    //================================================
    for (auto& t : threads)
        t.join();
}

//====================================================================
//! \brief  Lancement d'une génération de fens , dans une thread donnée
//--------------------------------------------------------------------
void DataGen::genfens(int thread_id, const std::string& str_file,
                      const Usize max_games)
// std::atomic<Usize>& total_fens)
{
    printf("thread id = %d : max_games=%zu out=%s \n", thread_id, max_games, str_file.c_str());

    std::ofstream file;
    file.open(str_file, std::ios::out);     //app = append ; out = write
    if (file.is_open() == false)
    {
        std::cout << "file " << str_file << "not opened" << std::endl;
        return;
    }

    std::array<fenData, 1000> fens{};  // liste des positions conservées, pour une partie donnée
    int nbr_fens;                       // nombre de positions conservées
    std::random_device rd;
    std::mt19937_64 generator(rd());
    auto start = TimePoint::now();

    int total_fens= 0;

    // Set the time manager
    // On va utiliser une limite par "node",
    // sauf dans "random_game" où on va utiliser une limite par "depth".
    Timer timer(false, 0, 0, 0, 0, 0, MAX_RANDOM_DEPTH, 0, 0);

    //================================================
    //  Boucle infinie
    //  On simule l'exécution de N parties
    //================================================

    // std::unique_ptr<Search> search = std::make_unique<Search>();
    Search* search = new Search();
    ThreadData* td = new ThreadData;

    td->info   = &(td->_info[STACK_OFFSET]);
    td->index  = 0;

    SearchInfo* si = &td->info[0];
    for (int i = 0; i < MAX_PLY+STACK_OFFSET; i++)
        (si + i)->ply = i;

    MoveList movelist;
    Usize    nbr_moves;
    Board    board(START_FEN);
    int      drawCount = 0;
    int      winCount = 0;
    bool     board_generated;

    double  result = 0;  // 0.5 for draw, 1.0 for white win, 0.0 for black win. = ~Color = 1 - Color
    MOVE    move;
    I32     score;
    Usize   nbr_games=0;
    int     ply;

    // Boucle infinie sur une simulation de parties
    while (nbr_games < max_games)
    {
        // printf("-----nouvelle partie : id=%d  game %d \n", thread_id, nbr_games);

        // search->reset();
        // clear history
        // board.nnue.reset();

        board.clear();
        board.set_fen(START_FEN, false); // sans NNUE

        /*  NewGame
         */
        transpositionTable.clear();
        td->reset();

        //====================================================
        //  Initalisation random de l'échiquier
        //----------------------------------------------------

        // printf("-------------------------------------------init random  \n");

        board_generated = false;
        timer.setup(Color::WHITE);
        board.clear();
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
                    continue;
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
                    continue;
                }
                std::uniform_int_distribution<> distribution{0, int(movelist.size() - 1)};
                const int index = distribution(generator);
                board.make_move<BLACK>(movelist.mlmoves[index].move);
            }
        }
        // impossible de générer un échiquier, on passe à une nouvelle partie
        if(board_generated == false)
        {
            nbr_games++;    // évite une boucle infinie
            // printf("-------------------------------------------generated false  \n");
            continue;
        }
        // printf("-------------------------------------------generated OK  \n");

        // printf("-------------------------------------------eval random  \n");
        move  = Move::MOVE_NONE;
        score = -INFINITE;

        // Evaluation de la position, en fin des MAX_RANDOM_MOVES moves
        // Si elle est trop déséquilibrée, on recommence
        if (board.turn() == WHITE)
            data_search<WHITE>(board, timer, search, td, si, move, score);
        else
            data_search<BLACK>(board, timer, search, td, si, move, score);
        if (std::abs(score) > MAX_RANDOM_SCORE)
        {
            // printf("-------------------------------------------score false  %d\n", score);
            nbr_games++;    // évite une boucle infinie
            continue;
        }
        // printf("-------------------------------------------score OK  %d\n", score);


        // printf("----------------------------random ok : id=%d \n", thread_id);

        //====================================================
        //  Boucle de jeu
        //----------------------------------------------------

        drawCount   = 0;
        winCount    = 0;
        nbr_fens    = 0;
        timer.setup(SOFT_NODE_LIMIT, HARD_NODE_LIMIT);
        ply = 0;

        while (ply < MAX_PLY_LOOP)
        {
            // printf("----------------------------nouveau coup \n");
            movelist.clear();
            td->nodes = 0;
            td->stopped  = false;
            std::memset(td->_info, 0, sizeof(SearchInfo)*STACK_SIZE);
            transpositionTable.update_age();

            if (board.turn() == WHITE)
                board.legal_moves<WHITE, MoveGenType::ALL>(movelist);
            else
                board.legal_moves<BLACK, MoveGenType::ALL>(movelist);
            nbr_moves = movelist.size();

            // Aucun coup possible
            if (!nbr_moves)
            {
                if (board.is_in_check()) {
                    // 0.0 : Noir gagne ; 1.0 : Blanc gagne
                    result = (board.turn() == WHITE ? 0.0 : 1.0);
                }
                else {
                    result = 0.5;
                }
                break;
            }

            // if (nbr_moves == 1)
            // {
            //     if (board.turn() == WHITE)
            //         board.make_move<WHITE>(movelist.mlmoves[0].move);
            //     else
            //         board.make_move<BLACK>(movelist.mlmoves[0].move);
            //     continue;
            // }

            if (board.turn() == WHITE)
                data_search<WHITE>(board, timer, search, td, si, move, score);
            else
                data_search<BLACK>(board, timer, search, td, si, move, score);


            //  Filtrage de la position
            //  On cherche une position tranquille
            if (   !board.is_in_check()
                && !Move::is_capturing(move)
                && abs(score) < HIGH_SCORE )
            {
                fens[nbr_fens++] = { board.get_fen() , score * (1 - 2*board.turn()) };
                total_fens++;

                // if (total_fens % 100 == 0)
                // {
                // auto elapsed = ( std::chrono::duration_cast<std::chrono::milliseconds>(TimePoint::now() - start ).count() + 1.00)/1000.0;
                // std::cout << "id = " << thread_id
                //           << "  total_fens_collected = " << total_fens << " ; fens / sec = "
                //           << total_fens / elapsed << std::endl;
                // }
            }

            // Adjudication de la partie

            const int absScore = std::abs(score);
            // MOVE tbmove;
            // int tbScore, tbBound;

            //  Presque mat ?
            if (absScore >= MATE_IN_X)
            {
                score *= (board.turn() == WHITE ? 1 : -1);
                result = (score < 0 ? 0.0 : 1.0);
                break;
            }

            // Partie nulle ?
            // if (board.is_draw(0))   //TODO vérifier ce 0
            // {
            //     score *= (board.turn() == WHITE ? 1 : -1);
            //     result = 0.5;
            //     break;
            // }

            // if (board.probe_wdl(tbScore, tbBound, MAX_RANDOM_PLIES) == true)
            // {
            //     score = tbScore;
            //     score *= (board.turn() == WHITE ? 1 : -1);
            //     result = (score < 0 ? 0.0 : 1.0);
            //     break;
            // }


            if (absScore >= HIGH_SCORE) {       // position gagnante
                winCount++;
                drawCount = 0;
            } else if (absScore <= LOW_SCORE) { // position annulante
                drawCount++;
                winCount = 0;
            } else {
                drawCount = 0;
                winCount = 0;
            }

            if (winCount >= MIN_WIN_COUNT) {            // on est sur de gagner
                score *= (board.turn() == WHITE ? 1 : -1);
                result = (score < 0 ? 0.0 : 1.0);
                break;
            }
            else if (drawCount >= MIN_DRAW_COUNT) {     // on est sur de faire nul
                result = 0.5;
                break;
            }

            // Exécution du coup trouvé lors de la recherche
            if (board.turn() == WHITE)
                board.make_move<WHITE>(move);
            else
                board.make_move<BLACK>(move);

            ply++;

        } // fin de la partie

        // printf("----------------------------partie terminée id=%d ; game=%d nbr_fens=%d \n", thread_id, nbr_games, nbr_fens);

        // Ecriture dans le fichier des fens collectées dans cette partie
        for (int i = 0; i < nbr_fens; i++)
        {
            file << fens[i].fen << " | "
                 << fens[i].score << " | "
                 << (result > 0.6 ? "1.0" : (result < 0.4 ? "0.0" : "0.5"))
                 // <<  " | "
                 // << "nbr_games=" << nbr_games <<  " | "
                 // << "Noir gagne : 0.0 ; Blanc gagne : 1.0 "
                 << "\n";
        }

        // file << "\n";
        // file << "\n";
        // file << "---- nbr_fens  " << nbr_fens << " \n";
        // file << "\n";
        // file << "\n";

        if (nbr_games % 100 == 0)
        {
            auto elapsed = ( std::chrono::duration_cast<std::chrono::milliseconds>(TimePoint::now() - start ).count() + 1.00)/1000.0;
            std::cout << "id = " << thread_id
                      << " ; game = " << nbr_games << " / " << max_games
                      << " ; total_fens_collected = " << total_fens << " ; fens / sec = "
                      << total_fens / elapsed << std::endl;
        }

        nbr_games++;

    } // fin des parties

    // std::cout << "id = " << thread_id << "  nombre de parties = " << nbr_games << std::endl;

    // dernière étape : on ferme le fichier
    file.close();
}


//============================================================================
//  Recherche dans la position actuelle
//----------------------------------------------------------------------------
template <Color C>
void DataGen::data_search(Board& board, Timer& timer,
                          Search *search,
                          // std::unique_ptr<Search>& search,
                          ThreadData *td,
                          SearchInfo* si,
                          MOVE &move, I32 &score)
{
#if defined USE_NNUE
    board.nnue.reset();
#endif

    move  = Move::MOVE_NONE;
    score = -INFINITE;

    //==================================================
    // iterative deepening
    //==================================================

    si->pv.length = 0;
    td->score     = -INFINITE;
    td->stopped   = false;
    td->nodes    = 0;

    td->best_depth = 0;
    td->best_move  = Move::MOVE_NONE;
    td->best_score = -INFINITE;


#if defined DEBUG_GEN
    timer.debug(WHITE);
    timer.start();
#endif
    // printf("max depth = %d ;  \n", timer.getSearchDepth());

    for (td->depth = 1; td->depth <= std::max(1, timer.getSearchDepth()); td->depth++)
    {
        // Search position, using aspiration windows for higher depths
        td->score = search->aspiration_window<C>(board, timer, td, si);

        if (td->stopped)
            break;
        // printf("depth = %d ; tdscore = %d \n", td->depth, td->score);
        // L'itération s'est terminée sans problème
        // On peut mettre à jour les infos UCI
        td->best_depth = td->depth;
        td->best_move  = si->pv.line[0];
        td->best_score = td->score;

#if defined DEBUG_GEN
        auto elapsed = timer.elapsedTime();

        // td->search->show_uci_result(&td, elapsed);
        std::cout << "  depth " << td->depth << "   nodes " << td->nodes << std::endl;
#else
        auto elapsed = 0;
#endif

        // If an iteration finishes after optimal time usage, stop the search
        if (timer.finishOnThisDepth(elapsed, td->depth, td->best_move, td->nodes))
            break;

        td->seldepth = 0;
    }

    move  = td->best_move;
    score = td->best_score;
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

template void DataGen::data_search<WHITE>(Board& board, Timer& timer,
                                          Search* search,
                                          // std::unique_ptr<Search>& search,
                                          ThreadData* td,
                                          SearchInfo* si,
                                          MOVE& move, I32& score);
template void DataGen::data_search<BLACK>(Board& board, Timer& timer,
                                          Search* search,
                                          // std::unique_ptr<Search>& search,
                                          ThreadData* td,
                                          SearchInfo* si,
                                          MOVE& move, I32& score);
