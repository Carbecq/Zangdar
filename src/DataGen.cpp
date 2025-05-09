
//  Code inspiré par plusieurs codes puis adapté et commenté
//      Midnight, Illumina, Clover, Clarity, SmallBrain

// #define DEBUG_GEN

/* bulletformat
 *  each line is of the form <FEN> | <score> | <result>
 *  score is white relative and in centipawns
 *  result is white relative and of the form 1.0 for win, 0.5 for draw, 0.0 for loss
*/

#include <string>
#include <cstring>
#include <fstream>
#include <random>

#include "Board.h"
#include "Search.h"
#include "DataGen.h"
#include "defines.h"
#include "TranspositionTable.h"
#include "ThreadPool.h"
#include "pyrrhic/tbprobe.h"

//===========================================================================
//! \brief  Constructeur
//! \param[in]  _nbr_threads    nombre de threads demandé
//! \param[in]  _max_fens       nombre de fens demandé, en millions
//! \param[in]  _output         répertoire de sortie des fichers
//---------------------------------------------------------------------------
DataGen::DataGen(const int _nbr_threads, const int _max_fens, const std::string& _output)
{
    Usize max_fens = std::clamp(_max_fens, 1, 1000) * 1000000;

    printf("DataGen : nbr_threads=%d ; max_fens=%zu millions ; output=%s \n", _nbr_threads, max_fens/1000000, _output.c_str());

    //================================================
    //  Initialisations
    //================================================
    int nbr_threads = set_threads(_nbr_threads);

    // 1) évite d'avoir des positions avec 2 rois seuls
    // 2) évite d'avoir une partie nulle avec R+T/R sans
    //    que le mat soit trouvé
    if (tb_init("/mnt/Datas/Echecs/Syzygy/") == true)
        threadPool.set_useSyzygy(true);

    //================================================
    //  Lancement de la génération par thread
    //================================================
    std::vector<std::thread> threads{};
    std::atomic<Usize> total_fens{0};           // nombre total de fens pour toutes les threads
    std::atomic<bool> run{true};
    auto start_time = TimePoint::now();

    for (auto i = 0; i < nbr_threads; i++)
    {
        std::string str_file(_output);
        str_file += "/data" + std::to_string(i) + ".txt";

        threads.emplace_back(
                    [this, i, str_file, &total_fens, &run] {
            genfens(i, str_file, total_fens, run);
        });
    }

    // thread de contrôle d'arrêt
    // il suffit d'entrer un caractère
    auto check_run_state = [&run] {
        std::cout.setf(std::ios::unitbuf);  // unbuffered output
        std::string input_line{};
        while (std::getline(std::cin, input_line))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10000));
            if (input_line.empty())
                continue;
            run.store(false);
            break;
        }
    };
    threads.emplace_back(check_run_state);

    // boucle d'affichage de l'avancée des résultats
    while (run)
    {
        std::this_thread::sleep_for(std::chrono::seconds(10));
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(TimePoint::now() - start_time).count();
        int fps = 1000 * total_fens / elapsed;
        int r = static_cast<double>(max_fens - total_fens) / static_cast<double>(fps);
        int h = r / 3600;
        int m = (r - h*3600) / 60;
        int s = r - h*3600 - m*60;
        std::cout << "total fens = " << total_fens/1000000.0 << " millions"
                  << " ; fens/s = " << fps
                  << " ; fens/s/t = " << 1000*total_fens/elapsed/nbr_threads
                  << " ; end in " << h << " hours " << m << " min " << s << " sec"
                  << std::endl;
        if (total_fens > max_fens)
        {
            run = false;
            std::cout << "taper un caractère, puis attendre un peu le temps que les threads arrêtent \n";
        }
    }

    //================================================
    //  Fin du job
    //================================================
    for (auto& t : threads)
        t.join();

    std::cout << "total fens generated = " << total_fens << std::endl;
}

//====================================================================
//! \brief  Lancement d'une génération de fens , dans une thread donnée
//! \param[in]  total_fens  somme des fens collectées sur toutes les threads
//! \param[in]  run         indique que l'on veut arrêter
//--------------------------------------------------------------------
void DataGen::genfens(int thread_id, const std::string& str_file,
                      std::atomic<Usize>& total_fens,
                      std::atomic<bool>& run)
{
    printf("thread id = %d : out = %s \n", thread_id, str_file.c_str());

    std::ofstream file;
    file.open(str_file, std::ios::out);     //app = append ; out = write
    if (file.is_open() == false)
    {
        std::cout << "file " << str_file << "not opened" << std::endl;
        return;
    }

    std::array<fenData, 1000> fens{};   // liste des positions conservées, pour une partie donnée
    int nbr_fens;                       // nombre de positions conservées, pour une partie
    std::random_device rd;
    std::mt19937_64 generator(rd());

    // Set the time manager
    // On va utiliser une limite par "node",
    // sauf dans "random_game" où on va utiliser une limite par "depth".
    Timer timer(false, 0, 0, 0, 0, 0, MAX_RANDOM_DEPTH, 0, 0);

    //================================================
    //  Boucle infinie
    //  On simule l'exécution de N parties
    //================================================

    std::unique_ptr<Search> search = std::make_unique<Search>();
    ThreadData* td = new ThreadData;
    td->index  = 0;

    MoveList movelist;
    Usize    nbr_moves;
    Board    board;
    int      drawCount = 0;
    int      winCount = 0;
    bool     use_syzygy;

    std::string  result;  // 0.5 for draw, 1.0 for white win, 0.0 for black win. = ~Color = 1 - Color
    MOVE    move;
    I32     score;

    //====================================================
    // Boucle infinie sur une simulation de parties
    //----------------------------------------------------

    while (run.load())
    {
        // printf("-----nouvelle partie : id=%d  \n", thread_id);

        board.reset();
        board.set_fen<true>(START_FEN, false);
        transpositionTable.clear();
        td->history.reset();

        //====================================================
        //  Initalisation random de l'échiquier
        //----------------------------------------------------

        // printf("-------------------------------------------init random  \n");

        Usize current_ply = 0;
        while (current_ply < MAX_RANDOM_PLIES)
        {
            if (board.turn() == WHITE)
            {
                board.legal_moves<WHITE, MoveGenType::ALL>(movelist);
                // On exécute la boucle jusqu'à trouver un coup
                if (movelist.size() == 0)
                {
                    current_ply = 0;
                    board.reset();
                    board.set_fen<true>(START_FEN, false);
                    continue;
                }
                std::uniform_int_distribution<> distribution{0, int(movelist.size() - 1)};
                const auto index = distribution(generator);
                board.make_move<WHITE, true>(movelist.mlmoves[index].move);

                // Prevent the last ply being checkmate/stalemate
                if (++current_ply == MAX_RANDOM_PLIES)
                {
                    board.legal_moves<WHITE, MoveGenType::ALL>(movelist);
                    if (movelist.size() == 0)
                    {
                        current_ply = 0;
                        board.reset();
                        board.set_fen<true>(START_FEN, false);
                    }
                }
            }
            else
            {
                board.legal_moves<BLACK, MoveGenType::ALL>(movelist);
                if (movelist.size() == 0)
                {
                    current_ply = 0;
                    board.reset();
                    board.set_fen<true>(START_FEN, false);
                    continue;
                }
                std::uniform_int_distribution<> distribution{0, int(movelist.size() - 1)};
                const auto index = distribution(generator);
                board.make_move<BLACK, true>(movelist.mlmoves[index].move);

                if (++current_ply == MAX_RANDOM_PLIES)
                {
                    board.legal_moves<BLACK, MoveGenType::ALL>(movelist);
                    if (movelist.size() == 0)
                    {
                        current_ply = 0;
                        board.reset();
                        board.set_fen<true>(START_FEN, false);
                    }
                }
            }
        }

        // printf("-------------------------------------------generated OK  \n");

        // Evaluation de la position, en fin des MAX_RANDOM_PLIES moves
        // Si elle est trop déséquilibrée, on passe à une nouvelle partie

        move  = Move::MOVE_NONE;
        score = -INFINITE;
        timer.setup(Color::WHITE);

        if (board.turn() == WHITE)
            data_search<WHITE>(board, timer, search, td, move, score);
        else
            data_search<BLACK>(board, timer, search, td, move, score);
        if (std::abs(score) > MAX_RANDOM_SCORE)
        {
            // printf("-------------------------------------------score false  %d\n", score);
            continue;
        }

        // printf("-------------------------------------------score OK  %d\n", score);

        //====================================================
        //  Boucle de jeu
        //----------------------------------------------------

        drawCount   = 0;
        winCount    = 0;
        nbr_fens    = 0;
        use_syzygy  = false;
        timer.setup(SOFT_NODE_LIMIT, HARD_NODE_LIMIT);

        while (true)
        {
            // printf("----------------------------nouveau coup \n");
            td->nodes   = 0;
            td->stopped = false;
            transpositionTable.update_age();

            // Partie nulle ?
            if (board.is_draw(0))   //TODO vérifier ce 0 (voir Integral V4, Clover)
            {
                result = COLOR_DRAW;
                break;
            }

            if (board.turn() == WHITE)
                board.legal_moves<WHITE, MoveGenType::ALL>(movelist);
            else
                board.legal_moves<BLACK, MoveGenType::ALL>(movelist);
            nbr_moves = movelist.size();

            // Aucun coup possible
            if (!nbr_moves)
            {
                if (board.is_in_check())    // en échec : mat
                {
                    result = COLOR_WIN[~board.turn()];
                }
                else                        // pas en échec ; pat
                {
                    result = COLOR_DRAW;
                }
                break;
            }

            if (board.turn() == WHITE)
                data_search<WHITE>(board, timer, search, td, move, score);
            else
                data_search<BLACK>(board, timer, search, td, move, score);

            // printf("----------------------------ajout (%d) \n", filtered);

            //  Filtrage de la position
            //  On cherche une position tranquille
            if (    board.is_in_check() == false
                && !Move::is_capturing(move)
                && abs(score) < HIGH_SCORE)
            {
                fens[nbr_fens++] = { board.get_fen() , score * (1 - 2*board.turn()) };
            }
            // printf("----------------------------ajout fin (%d) \n", ok_tb);

            // Si on utilise les tables Syzygy, inutile d'aller plus loin
            if (    threadPool.get_useSyzygy()
                 && BB::count_bit(board.occupancy_all()) <= 6)
            {
                use_syzygy = true;
                break;
            }

            // Adjudication de la partie
            const int absScore = std::abs(score);

            //  Presque mat ?
            if (absScore >= MATE_IN_X)
            {
                score *= (board.turn() == WHITE ? 1 : -1);
                result = (score < 0 ? COLOR_WIN[BLACK] : COLOR_WIN[WHITE]);
                break;
            }

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

            // Gain ou Nulle quasiment assuré, inutile d'aller plus loin
            if (winCount >= MIN_WIN_COUNT) {            // on est presque certain de gagner
                score *= (board.turn() == WHITE ? 1 : -1);
                result = (score < 0 ? COLOR_WIN[BLACK] : COLOR_WIN[WHITE]);
                break;
            }
            else if (drawCount >= MIN_DRAW_COUNT) {     // on est presque certain de faire nul
                result = COLOR_DRAW;
                break;
            }

            // Exécution du coup trouvé lors de la recherche
            if (board.turn() == WHITE)
                board.make_move<WHITE, true>(move);
            else
                board.make_move<BLACK, true>(move);
        }
        // printf("------------------fin de la partie \n");

        // Si on utilise les tables Syzygy, le résultat est exact.
        if (use_syzygy)
        {
            score *= (board.turn() == WHITE ? 1 : -1);
            result = score < 0 ? COLOR_WIN[BLACK] :
                     score > 0 ? COLOR_WIN[WHITE] :
                                 COLOR_DRAW;
        }

        // Ecriture dans le fichier des fens collectées dans cette partie
        for (int i = 0; i < nbr_fens; i++)
        {
            file << fens[i].fen   << " | "
                 << fens[i].score << " | "
                 << result
                    // <<  " | "
                    // << "nbr_games=" << nbr_games <<  " | "
                    // << "Noir gagne : 0.0 ; Blanc gagne : 1.0 "
                 << "\n";
        }

        total_fens.fetch_add(nbr_fens); // total des tous les threads

        // file << "\n";
        // file << "\n";
        // file << "---- nbr_fens  " << nbr_fens << " \n";
        // file << "\n";
        // file << "\n";

    } // fin des parties

    // dernière étape : on ferme le fichier
    file.close();
}

//============================================================================
//  Recherche dans la position actuelle
//----------------------------------------------------------------------------
template <Color C>
void DataGen::data_search(Board& board, Timer& timer,
                          std::unique_ptr<Search>& search,
                          ThreadData *td,
                          MOVE &move, I32 &score)
{
    move  = Move::MOVE_NONE;
    score = -INFINITE;

    td->score     = -INFINITE;
    td->stopped   = false;
    td->nodes     = 0;

    td->best_depth = 0;
    td->best_move  = Move::MOVE_NONE;
    td->best_score = -INFINITE;


#if defined DEBUG_GEN
    timer.debug(WHITE);
    timer.start();
#endif

    //==================================================
    // iterative deepening
    //==================================================

    std::array<SearchInfo, STACK_SIZE> _info{};
    SearchInfo* si  = &(_info[STACK_OFFSET]);
    for (int i = 0; i < MAX_PLY+STACK_OFFSET; i++)
    {
        (si + i)->ply = i;
        (si + i)->cont_hist = &td->history.continuation_history[0][0];
    }

    for (td->depth = 1; td->depth <= std::max(1, timer.getSearchDepth()); td->depth++)
    {
        // Search position, using aspiration windows for higher depths
        td->score = search->aspiration_window<C>(board, timer, td, si);

        if (td->stopped)
            break;

        // L'itération s'est terminée sans problème
        // On peut mettre à jour les infos UCI
        td->best_depth = td->depth;
        td->best_move  = si->pv.line[0];
        td->best_score = td->score;

#if defined DEBUG_GEN
        auto elapsed = timer.elapsedTime();

        std::cout << "time " << elapsed
                  << "  depth " << td->depth
                  << "  nodes " << td->nodes
                  << "  score " << td->score
                  << "  move " << Move::name(td->best_move)
                  << std::endl;
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
// Search* search,
std::unique_ptr<Search>& search,
ThreadData* td,
MOVE& move, I32& score);
template void DataGen::data_search<BLACK>(Board& board, Timer& timer,
// Search* search,
std::unique_ptr<Search>& search,
ThreadData* td,
MOVE& move, I32& score);
