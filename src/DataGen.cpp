
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
#include <memory>
#include <csignal>

#include "Board.h"
#include "Search.h"
#include "DataGen.h"
#include "defines.h"
#include "TranspositionTable.h"
#include "ThreadPool.h"
#include "pyrrhic/tbprobe.h"

// Pointeur global pour permettre au handler de signal d'arrêter proprement le datagen
static std::atomic<bool>* g_datagen_run = nullptr;

static void datagen_signal_handler(int /*sig*/)
{
    if (g_datagen_run)
        g_datagen_run->store(false);
}

//===========================================================================
//! \brief  Constructeur
//! \param[in]  _nbr_threads    nombre de threads demandé
//! \param[in]  _max_fens       nombre de fens demandé, en millions
//! \param[in]  _output         répertoire de sortie des fichiers
//---------------------------------------------------------------------------
DataGen::DataGen(const U32 _nbr_threads, const U32 _max_fens, const std::string& _output)
{
    const U32 max_fens = std::clamp(_max_fens, 1U, 1000U) * 1'000'000U;

    printf("DataGen : nbr_threads=%u ; max_fens=%u millions ; output=%s \n", _nbr_threads, max_fens/1'000'000, _output.c_str());

    //================================================
    //  Initialisations
    //================================================

    // 1) évite d'avoir des positions avec 2 rois seuls
    // 2) évite d'avoir une partie nulle avec R+T/R sans
    //    que le mat soit trouvé
    // 3) SYZYGY est défini dans le Makefile
    tb_init(SYZYGY);

    // only use TB if loading was successful
    if (TB_LARGEST > 0)
    {
        threadPool.set_useSyzygy(true);

        // Sans cet appel, syzygyProbeLimit vaut 0 et le break Syzygy en cours de partie
        // (DataGen.cpp, boucle de jeu) ne se déclencherait jamais (min(TB_LARGEST,0) = 0).
        threadPool.set_syzygyProbeLimit(TB_LARGEST);
    }
    else
    {
        std::cout << "Les tables Syzygy ne sont pas présentes." << std::endl;
        return;
    }

    U32 nbr_threads = set_threads(_nbr_threads);

    //================================================
    //  Lancement de la génération par thread
    //================================================
    std::vector<std::thread> threads{};
    std::atomic<size_t> total_fens{0};           // nombre total de fens pour toutes les threads

    // shared_ptr pour que le thread detached (lecture stdin) ne référence jamais un objet détruit
    auto run = std::make_shared<std::atomic<bool>>(true);
    auto start_time = TimePoint::now();

    // Installer le handler de signal pour arrêt propre via Ctrl-C
    g_datagen_run = run.get();
    auto prev_sigint  = std::signal(SIGINT,  datagen_signal_handler);
    auto prev_sigterm = std::signal(SIGTERM, datagen_signal_handler);

    for (size_t i = 0; i < nbr_threads; i++)
    {
        std::string str_file(_output);
        str_file += "/data" + std::to_string(i) + ".txt";

        threads.emplace_back(
                    [this, i, str_file, &total_fens, run] {
            genfens(i, str_file, total_fens, *run);
        });
    }

    // thread de contrôle d'arrêt (detached : ne bloque pas la fin du programme)
    // il suffit d'entrer un caractère
    std::thread([run] {
        std::string input_line{};
        while (std::getline(std::cin, input_line))
        {
            if (input_line.empty())
                continue;
            run->store(false);
            break;
        }
    }).detach();

    // boucle d'affichage de l'avancée des résultats
    while (run->load())
    {
        std::this_thread::sleep_for(std::chrono::seconds(10));
        auto elapsed = std::max(std::chrono::duration_cast<std::chrono::milliseconds>(TimePoint::now() - start_time).count(), decltype(start_time)::duration::rep(1));
        U64 fps = 1000 * total_fens / elapsed;
        int r   = (fps > 0 && total_fens < max_fens)
                ? static_cast<double>(max_fens - total_fens) / static_cast<double>(fps)
                : 0;
        int h   = r / 3600;
        int m   = (r - h*3600) / 60;
        int s   = r - h*3600 - m*60;
        std::cout << "total fens = " << total_fens/1'000'000.0 << " millions"
                   << " ; fens/s = " << fps
                   << " ; fens/s/t = " << 1000*total_fens/elapsed/nbr_threads
                   << " ; end in " << h << " hours " << m << " min " << s << " sec"
                   << std::endl;
        if (total_fens >= max_fens)
        {
            run->store(false);
        }
    }

    //================================================
    //  Fin du job
    //================================================
    for (auto& t : threads)
        t.join();

    // Restaurer les handlers de signal
    std::signal(SIGINT,  prev_sigint);
    std::signal(SIGTERM, prev_sigterm);
    g_datagen_run = nullptr;

    std::cout << "total fens generated = " << total_fens << std::endl;
}

//====================================================================
//! \brief  Lancement d'une génération de fens , dans une thread donnée
//! \param[in]  total_fens  somme des fens collectées sur toutes les threads
//! \param[in]  run         true = continuer la génération, false = arrêter
//--------------------------------------------------------------------
void DataGen::genfens(int thread_id, const std::string& str_file,
                      std::atomic<size_t>& total_fens,
                      std::atomic<bool>& run)
{
    printf("thread id = %d : out = %s \n", thread_id, str_file.c_str());

    std::ofstream file;
    file.open(str_file, std::ios::out);     //app = append ; out = write
    if (file.is_open() == false)
    {
        std::cout << "file " << str_file << " not opened" << std::endl;
        return;
    }

    std::array<fenData, 1000> fens{};   // liste des positions conservées, pour une partie donnée
    int nbr_fens = 0;                   // nombre de positions conservées, pour une partie
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

    // Allocation sur le tas : Search et History sont trop gros pour la pile d'un thread
    std::unique_ptr<Search> search = std::make_unique<Search>();
    search->index  = 0;

    // chaque thread a sa propre table, de façon à éviter
    // qu'un thread efface la TT pendant qu'un autre est en pleine recherche.
    // TT réduite (4 MB vs 128 MB) : suffisant pour 5000 nodes, meilleure localité cache
    std::unique_ptr<TranspositionTable> table_ptr = std::make_unique<TranspositionTable>(DATAGEN_HASH_SIZE);
    search->table = table_ptr.get();

    MoveList movelist;
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

    // run est vérifié uniquement ici, entre deux parties :
    // Ctrl-C (ou limite atteinte) → finit la partie en cours → écrit les fens → sort proprement.
    while (run.load())
    {
        // printf("-----nouvelle partie : id=%d  \n", thread_id);

        board.initialisation();
        board.set_fen(START_FEN, false);
        search->table->update_age();            // evite de clear 4 Mo
        search->history.reset();

        //====================================================
        //  Initalisation random de l'échiquier
        //----------------------------------------------------

        // printf("-------------------------------------------init random  \n");

        auto random_ply = [&]<Color C>(size_t& current_ply) -> bool {
            board.legal_moves<C, MoveGenType::ALL>(movelist);
            if (movelist.size() == 0)
            {
                current_ply = 0;
                board.initialisation();
                board.set_fen(START_FEN, false);
                return false; // recommencer
            }
            std::uniform_int_distribution<> distribution{0, int(movelist.size() - 1)};
            const int index = distribution(generator);
            search->make_move<C, false>(board, movelist.mlmoves[index].move);

            // Prevent the last ply being checkmate/stalemate
            if (++current_ply == MAX_RANDOM_PLIES)
            {
                board.legal_moves<~C, MoveGenType::ALL>(movelist);
                if (movelist.size() == 0)
                {
                    current_ply = 0;
                    board.initialisation();
                    board.set_fen(START_FEN, false);
                }
            }
            return true;
        };

        size_t current_ply = 0;
        while (current_ply < MAX_RANDOM_PLIES)
        {
            bool ok = board.turn() == WHITE
                ? random_ply.template operator()<WHITE>(current_ply)
                : random_ply.template operator()<BLACK>(current_ply);
            if (!ok)
                continue;
        }

        // printf("-------------------------------------------generated OK  \n");

        // Evaluation de la position, en fin des MAX_RANDOM_PLIES moves
        // Si elle est trop déséquilibrée, on passe à une nouvelle partie

        // Initialisation NNUE une seule fois par partie (au lieu de à chaque data_search)
        search->nnue.start_search(board);

        move  = Move::MOVE_NONE;
        score = -INFINITE;
        timer.setup(Color::WHITE);

        if (board.turn() == WHITE)
            data_search<WHITE>(board, timer, *search, move, score);
        else
            data_search<BLACK>(board, timer, *search, move, score);
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
            search->nodes   = 0;
            search->stopped = false;
            search->table->update_age();

            // Partie nulle ?
            if (board.is_draw(0))
            {
                result = COLOR_DRAW;
                break;
            }

            if (board.turn() == WHITE)
                board.legal_moves<WHITE, MoveGenType::ALL>(movelist);
            else
                board.legal_moves<BLACK, MoveGenType::ALL>(movelist);

            // Aucun coup possible
            if (movelist.size() == 0)
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

            // Rebase le stack NNUE avant chaque recherche pour éviter l'overflow
            // (head_idx monte de 1 à chaque make_move de la boucle de jeu)
            search->nnue.rebase(board);

            if (board.turn() == WHITE)
                data_search<WHITE>(board, timer, *search, move, score);
            else
                data_search<BLACK>(board, timer, *search, move, score);

            // printf("----------------------------ajout (%d) \n", filtered);

            //  Filtrage de la position
            //  On cherche une position tranquille
            if (    board.is_in_check() == false
                && !Move::is_capturing(move)
                && abs(score) < HIGH_SCORE
                && nbr_fens < static_cast<int>(fens.size()))
            {
                fens[nbr_fens++] = { board.get_fen() , score * (1 - 2*board.turn()) };
            }
            // printf("----------------------------ajout fin (%d) \n", ok_tb);

            // Si on utilise les tables Syzygy, inutile d'aller plus loin
            if (BB::count_bit(board.occupancy_all()) <= TB_LARGEST)
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
                search->make_move<WHITE, true>(board, move);
            else
                search->make_move<BLACK, true>(board, move);
        }
        // printf("------------------fin de la partie \n");

        // Si on utilise les tables Syzygy, le résultat est exact.
        if (use_syzygy)
        {
            unsigned wdl = tb_probe_wdl(
                board.occupancy_c<WHITE>(), board.occupancy_c<BLACK>(),
                board.occupancy_p<PieceType::KING>(),   board.occupancy_p<PieceType::QUEEN>(),
                board.occupancy_p<PieceType::ROOK>(),   board.occupancy_p<PieceType::BISHOP>(),
                board.occupancy_p<PieceType::KNIGHT>(), board.occupancy_p<PieceType::PAWN>(),
                board.get_status().ep_square == SQUARE_NONE ? 0 : board.get_status().ep_square,
                board.turn() == WHITE ? 1 : 0);

/*            Les tables Syzygy retournent le WDL du point de vue du camp qui doit jouer (board.turn()).

              - TB_WIN → le camp qui joue gagne → COLOR_WIN[board.turn()]
              - TB_LOSS → le camp qui joue perd → c'est l'adversaire qui gagne → COLOR_WIN[~board.turn()]
              - sinon → nulle

              L'opérateur ~ inverse la couleur (WHITE→BLACK, BLACK→WHITE).

              Exemple concret : si c'est aux Noirs de jouer et que Syzygy dit TB_WIN :
              - board.turn() = BLACK
              - result = COLOR_WIN[BLACK] = "0.0" ✓
*/
            if (wdl != TB_RESULT_FAILED)
            {
                result = (wdl == TB_WIN)  ? COLOR_WIN[board.turn()] :
                         (wdl == TB_LOSS) ? COLOR_WIN[~board.turn()] :
                                            COLOR_DRAW;
            }
            else
            {
                // fallback : score de la dernière recherche
                score *= (board.turn() == WHITE ? 1 : -1);
                result = score < 0 ? COLOR_WIN[BLACK] :
                         score > 0 ? COLOR_WIN[WHITE] :
                                     COLOR_DRAW;
            }
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

        total_fens.fetch_add(nbr_fens);   // total des fens de tous les threads
        file.flush();   // force l'écriture sur disque : protège contre Ctrl-C / crash

        // file << "\n";
        // file << "\n";
        // file << "---- nbr_fens  " << nbr_fens << " \n";
        // file << "\n";
        // file << "\n";

    } // fin des parties

    file.close();
}

//============================================================================
//! \brief  Recherche dans la position actuelle
//! Lance un iterative deepening avec aspiration windows
//! \param[out] move    meilleur coup trouvé
//! \param[out] score   score du meilleur coup
//----------------------------------------------------------------------------
template <Color C>
void DataGen::data_search(Board& board, Timer& timer, Search& search,
                          MOVE &move, I32 &score)
{
    move  = Move::MOVE_NONE;
    score = -INFINITE;

    search.iter_score     = -INFINITE;
    search.stopped   = false;
    search.nodes     = 0;

    search.iter_best_depth = 0;
    search.iter_best_move  = Move::MOVE_NONE;
    search.iter_best_score = -INFINITE;


#if defined DEBUG_GEN
    timer.debug(WHITE);
    timer.start();
#endif

    //==================================================
    // iterative deepening
    //==================================================

    // start_search() est appelé une seule fois par partie, dans genfens()
    // L'accumulateur NNUE est maintenu incrémentalement par les make_move de la boucle de jeu.

    std::array<SearchInfo, STACK_SIZE> _info{};
    SearchInfo* si  = &(_info[STACK_OFFSET]);
    for (int i = 0; i < MAX_PLY+STACK_OFFSET; i++)
    {
        (si + i)->ply = i;
        (si + i)->cont_hist = &search.history.continuation_history[0][0];
    }

    for (search.iter_depth = 1; search.iter_depth <= std::max(1, timer.getSearchDepth()); search.iter_depth++)
    {
        // Search position, using aspiration windows for higher depths
        search.iter_score = search.aspiration_window<C>(board, timer, si);

        if (search.stopped)
            break;

        // L'itération s'est terminée sans problème
        // On peut mettre à jour les infos UCI
        search.iter_best_depth = search.iter_depth;
        search.iter_best_move  = si->pv.line[0];
        search.iter_best_score = search.iter_score;

#if defined DEBUG_GEN
        I64 elapsed = timer.elapsedTime();

        std::cout << "time " << elapsed
                  << "  depth " << search.td_depth
                  << "  nodes " << search.td_nodes
                  << "  score " << search.td_score
                  << "  move " << Move::name(search.td_best_move)
                  << std::endl;
#else
        I64 elapsed = 0;
#endif

        // If an iteration finishes after optimal time usage, stop the search
        if (timer.finishOnThisDepth(elapsed, search.iter_depth, search.iter_best_move, search.nodes))
            break;

        search.seldepth = 0;
    }

    move  = search.iter_best_move;
    score = search.iter_best_score;
}

//===================================================
//! \brief  Calcule le nombre de threads à utiliser
//! Limité au nombre de processeurs disponibles
//---------------------------------------------------
U32 DataGen::set_threads(const U32 nbr)
{
    U32 nbr_threads = nbr;

    U32 processorCount = static_cast<U32>(std::thread::hardware_concurrency());
    // Check if the number of processors can be determined
    if (processorCount == 0)
        processorCount = MAX_THREADS;

    // Clamp the number of threads to the number of processors
    nbr_threads     = std::min(nbr, processorCount);
    nbr_threads     = std::max(nbr_threads, 1U);
    nbr_threads     = std::min(nbr_threads, MAX_THREADS);

    return nbr_threads;
}

template void DataGen::data_search<WHITE>(Board& board, Timer& timer, Search& search, MOVE& move, I32& score);
template void DataGen::data_search<BLACK>(Board& board, Timer& timer, Search& search, MOVE& move, I32& score);
