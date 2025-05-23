#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <algorithm>
#include <iomanip>

#include "defines.h"
#include "Uci.h"
#include "Timer.h"
#include "TranspositionTable.h"
#include "ThreadPool.h"
#include "pyrrhic/tbprobe.h"
#include "Move.h"
#include "bench.h"
#include "Tunable.h"

Board   uci_board;
Timer   uci_timer;

//======================================
//! \brief  Boucle principale UCI
//--------------------------------------
void Uci::run()
{
#if defined DEBUG_LOG
    char message[100];
    sprintf(message, "Uci::run ");
    printlog(message);
#endif

    // Note de Stockfish

    /// UCI::loop() waits for a command from the stdin, parses it and then calls the appropriate
    /// function. It also intercepts an end-of-file (EOF) indication from the stdin to ensure a
    /// graceful exit if the GUI dies unexpectedly.


    // main loop
    std::string token;
    std::string line;
    std::string fen  = START_FEN;
    int         dmax = 6;
    int         tmax = 0;
    int         nmax = 0;
    int         nthreads = 1;

    do
    {
        if (!std::getline(std::cin, line))   // Wait for an input or an end-of-file (EOF) indication
            line = "quit";

        std::istringstream iss(line);
        token.clear();                      // Avoid a stale if getline() returns nothing or a blank line
        iss >> std::skipws >> token;

        //-------------------------------------------- gui -> engine

        if (token == "isready")
        {
            /* "isready" is meant as ping/pong replacement AND feature done with init,
             * and the UCI spec explicitely mentions tablebase initialistion as example
             * that may take some time.
             * The engine only responds with "readyok" after it has processed the setoptions commands.
             * The case where "isready" must be answered immediately is only after init
             * and during search because then there is no reason why the engine should be hanging.
             */

            // synchronize the engine with the GUI
            std::cout << "readyok" << std::endl;
        }

        else if (token == "uci")
        {
            // "uciok" has to appear quickly after the GUI sent "uci",
            // and lengthy init stuff is not allowed.

            // print engine info
            std::cout << "id name Zangdar " << VERSION  << std::endl;
            std::cout << "id author Philippe Chevalier" << std::endl;

            // This command tells the GUI which parameters can be changed in the engine.

            // Quand dans Arena, on fait "Configure", on fait apparaitre une interface
            // contenant les options données ici.
            // Lorsque l'utilisateur va agir sur une de ces options,
            // Arena va envoyer la commande "setoption.." au programme.

            std::cout << "option name Hash type spin default " << HASH_SIZE <<" min " << MIN_HASH_SIZE << " max " << MAX_HASH_SIZE << std::endl;
            std::cout << "option name Clear Hash type button" << std::endl;
            std::cout << "option name Threads type spin default 1 min 1 max " << MAX_THREADS << std::endl;
            std::cout << "option name SyzygyPath type string default " << "<empty>" << std::endl;
            std::cout << "option name MoveOverhead type spin default " << MOVE_OVERHEAD << " min 0 max 10000" << std::endl;

#if defined USE_TUNE
            std::cout << Tunable::paramsToUci();
#endif

            std::cout << "uciok" << std::endl;
        }

        else if (token == "position")
        {
            // set up the position described in fenstring
            uci_board.reset();
            uci_board.parse_position(iss);
        }

        else if (token == "ucinewgame" || token == "new")
        {
            // the next search (started with "position" and "go") will be from
            // a different game.
            transpositionTable.clear();
            threadPool.reset();
        }

        else if (token == "go")
        {
            parse_go(iss);
        }

        else if (token == "setoption")
        {
            parse_options(iss);
        }

        else if (token == "stop")
        {
            // stop calculating as soon as possible
            stop();
        }

        else if (token == "quit")
        {
            // quit the program as soon as possible
            quit();
            break;
        }

        //------------------------------------------------------- commandes non uci

        else if (token == "h")
        {
            std::cout << "benchmark                     : Zangdar bench <depth> <nbr_threads> <hash_size>"     << std::endl;
            std::cout << "datagen                       : Zangdar datagen <nbr_threads> <nbr_fens (millions)> <output>"   << std::endl;
            std::cout << "q(uit) "      << std::endl;
            std::cout << "v(ersion) "   << std::endl;
            std::cout << "s <ref/big> [dmax]            : test suite_perft "                                    << std::endl;
            std::cout << "p <r/k/s/f> [dmax]            : test perft  <Ref/Kiwipete/Silver2/fen> "              << std::endl;
            std::cout << "d <r/k/s/f> [dmax]            : test divide <Ref/Kiwipete/Silver2/fen> "              << std::endl;
            std::cout << "test                          : test de recherche sur un ensemble de positions"       << std::endl;
            std::cout << "eval                          : test evaluation"                                      << std::endl;
            std::cout << "see                           : test see"                                             << std::endl;
            std::cout << "run <s/k/q/f/w/b> [dmax]      : test de recherche <Silver2/Kiwipete/Quies/Fine70/WAC2/BUG/REF>"           << std::endl;
            std::cout << "mirror                        : test mirror"                                          << std::endl;
            std::cout << "fen [str]                     : positionne la chaine fen"                             << std::endl;
            std::cout << "dmax [p]                      : positionne la profondeur de recherche"                << std::endl;
            std::cout << "tmax [ms]                     : positionne le temps de recherche en millisecondes"    << std::endl;
            std::cout << "display                       : affiche la position"                                  << std::endl;
            std::cout << "systeme                       : informe sur les minimums systeme"                     << std::endl;
        }

        else if (token == "v")
        {
            std::cout << "Zangdar " << VERSION << " NNUE " << NETWORK << std::endl;
            transpositionTable.info();
        }
        else if (token == "s")
        {
            std::string str;
            iss >> str;
            iss >> dmax;

            test_suite(str, dmax);
        }

        else if (token == "p")
        {
            std::string str;
            iss >> str;
            iss >> dmax;

            test_perft<false>(str, fen, dmax);
        }

        else if (token == "d")
        {
            std::string str;
            iss >> str;
            iss >> dmax;

            test_perft<true>(str, fen, dmax);
        }

        else if(token == "mirror")
        {
            test_mirror();
        }

        else if(token == "eval")
        {
            test_eval(fen);
        }

        else if(token == "see")
        {
            test_see();
        }

        else if (token == "run")
        {
            std::string str;
            iss >> str;
            iss >> dmax;
            iss >> tmax;
            iss >> nmax;
            iss >> nthreads;

            if (nthreads > 1)
                threadPool.set_threads(nthreads);

            go_run(str, fen, dmax, tmax, nmax);
        }

        else if (token == "test")
        {
            go_test(dmax, tmax);
        }

        else if (token == "fen")
        {
            std::getline(iss, fen);
            uci_board.reset();
            uci_board.set_fen<false>(fen, false);
            std::cout << uci_board.display() << std::endl;;
        }
        else if (token == "display")
        {
            std::cout << uci_board.display() << std::endl;;
        }
        else if (token == "dmax")
        {
            iss >> dmax;
        }
        else if (token == "tmax")
        {
            iss >> tmax;
        }
        else if (token == "nmax")
        {
            iss >> nmax;
        }

        else if (token == "json")
        {
            Tunable::paramsToJSON();
        }

        else if (token == "yyy")
        {
            std::string syzygy_path(SYZYGY);
            std::string command = "name SyzygyPath value ";
            command += syzygy_path;
            std::istringstream stream(command);
            parse_options(stream);
        }

        else if (token == "systeme")
        {
#if __cplusplus >= 201103L
            printf("c++ 11 \n");
#endif
#if __cplusplus >= 201402L
            printf("c++ 14 \n");
#endif
#if __cplusplus >= 201703L
            printf("c++ 17 \n");
#endif
#if __cplusplus >= 202002L
            printf("c++ 20 \n");
#endif
#if __cplusplus > 202002L
            printf("c++ 2x \n");
#endif

            // NOTE : On peut avoir simultanément : _MSC_VER ET __llvm__
            //        Il faut faire attention à l'ordre des tests
            //        pour définir quelle version sera prise (Bitboard.h)

#if defined(__INTEL_COMPILER)
            printf("INTEL_COMPILER \n");
#endif
#if defined(_MSC_VER)
            printf("compilateur Microsoft \n");
#endif
#if defined(__GNUC__)
            printf("compilateur Gnu \n");
#endif
#if defined(__llvm__)
            printf("compilateur Clang \n");
#endif

#if defined(__AVX512F__)
            std::cout << "__AVX512F__ OK " << std::endl;
#endif
#if defined (__AVX512BW__)
            std::cout << "__AVX512BW__ OK " << std::endl;
#endif
#if defined(__AVX2__)
            std::cout << "__AVX2__ OK " << std::endl;
#endif
#if defined(__SSE2__)
            std::cout << "__SSE2__ OK " << std::endl;
#endif
#if defined(__ARM_NEON)
            std::cout << "__ARM_NEON OK " << std::endl;
#endif

        }
    } while (token != "quit");

}


//==============================================================
//! \brief uci command: stop
//! stops the current search.
//! This will usually print a last info string and the best move.
//--------------------------------------------------------------
void Uci::stop()
{
    threadPool.stop();
}

void Uci::quit()
{
    threadPool.quit();
}

//==============================================================
//! \brief uci command: go
//! Lance la recherche
//--------------------------------------------------------------
void Uci::parse_go(std::istringstream& iss)
{
    bool infinite   = false;
    int wtime       = 0;
    int btime       = 0;
    int winc        = 0;
    int binc        = 0;
    int movestogo   = 0;
    int depth       = 0;
    int nodes       = 0;
    int movetime    = 0;

    // Stop any running search
    Uci::stop();

    std::string token;
    token.clear();

    // We received a start command. Extract all parameters from the
    // command and start the search.

    while (iss >> token)
    {
        if (token == "infinite")
        {
            // search until the "stop" command. Do not exit the search without being told so in this mode!
            infinite = true;
        }
        else if (token == "wtime")
        {
            // white has x msec left on the clock
            iss >> wtime;
        }
        else if (token == "btime")
        {
            // black has x msec left on the clock
            iss >> btime;
        }
        else if (token == "winc")
        {
            // white increment per move in mseconds
            iss >> winc;
        }
        else if (token == "binc")
        {
            // black increment per move in mseconds
            iss >> binc;
        }
        else if (token == "movestogo")
        {
            // there are x moves to the next time control
            iss >> movestogo;
        }
        else if (token == "depth")
        {
            // search x plies only.
            iss >> depth;
            depth = std::min(depth, MAX_PLY);
        }
        else if (token == "nodes")
        {
            // search x nodes max.
            iss >> nodes;
        }
        else if (token == "movetime")
        {
            // search exactly x mseconds
            uint64_t searchTime;
            iss >> searchTime;
            movetime = searchTime;
        }
    }

    // Init the time manager
    uci_timer = Timer(infinite, wtime, btime, winc, binc, movestogo, depth, nodes, movetime);
    uci_timer.start();
    uci_timer.setup(uci_board.side_to_move);


    // La recherche est lancée dans une ou plusieurs threads séparées
    // Le programme principal contine dans la thread courante
    // de façon à continuer à recevoir les commandes
    // de UCI. Pax exemple : stop, quit.
#if defined DEBUG_LOG
    char message[100];
    sprintf(message, "UCI::start_thinking ");
    printlog(message);
#endif

    // start the search
    threadPool.start_thinking(uci_board, uci_timer);
}

//=========================================================
//! \brief  Parse les commandes UCI relatives au changement
//! d'option.
//---------------------------------------------------------
void Uci::parse_options(std::istringstream& iss)
{

    /*
setoption name <id> [value <x>]
    this is sent to the engine when the user wants to change the internal parameters
    of the engine. For the "button" type no value is needed.
    One string will be sent for each parameter and this will only be sent when the engine is waiting.
    The name and value of the option in <id> should not be case sensitive and can inlude spaces.
    The substrings "value" and "name" should be avoided in <id> and <x> to allow unambiguous parsing,
    for example do not use <name> = "draw value".
    Here are some strings for the example below:
       "setoption name Nullmove value true\n"
       "setoption name Selectivity value 3\n"
       "setoption name Style value Risky\n"
       "setoption name Clear Hash\n"
       "setoption name NalimovPath value c:\chess\tb\4;c:\chess\tb\5\n" *
     */

    //  "setoption name Hash value 16\n"
    //  "setoption name Nullmove value true\n"
    //  "setoption name Selectivity value 3\n"
    //  "setoption name Style value Risky\n"
    //  "setoption name Clear Hash\n"
    //  "setoption name NalimovPath value c:\chess\tb\4;c:\chess\tb\5\n"


    std::string name;
    std::string value;
    std::string auxi;
#if defined DEBUG_LOG
    char message[100];
#endif

    iss >> name;   // "name"
    if (name == "name")
    {
        std::string option_name;    // nom de l'option
        iss >> option_name;

        if (option_name == "Hash")
        {
            iss >> value;      // "value"
            int mb;
            iss >> mb;
            mb = std::min(mb, MAX_HASH_SIZE);
            mb = std::max(mb, MIN_HASH_SIZE);

#if defined DEBUG_LOG
            sprintf(message, "Uci::parse_options : Set Hash to %d MB", mb);
            printlog(message);
#endif
            transpositionTable.set_hash_size(mb);
        }

        else if (option_name == "Clear")
        {
            iss >> auxi;
            if (auxi == "Hash")
            {
#if defined DEBUG_LOG
                sprintf(message, "Uci::parse_options : Hash Clear");
                printlog(message);
#endif
                transpositionTable.clear();
            }
        }

        if (option_name == "Threads")
        {
            iss >> value;      // "value"
            int nbr;
            iss >> nbr;

            threadPool.set_threads(nbr);
        }

        else if (option_name == "SyzygyPath")
        {
            iss >> value;      // "value"

            std::string path;
            iss >> path;

            if (path != "<empty>" && !path.empty())
            {
#if defined DEBUG_LOG
                sprintf(message, "Uci::parse_options : SyzygyPath (%s) ", path.c_str());
                printlog(message);
#endif
                tb_init(path.data());

                // only use TB if loading was successful
                threadPool.set_useSyzygy(TB_LARGEST > 0);
            }
        }

        else if (option_name == "MoveOverhead")
        {
            int MoveOverhead;       // Overhead on time allocation to avoid time losses

            iss >> value;      // "value"
            iss >> MoveOverhead;
            uci_timer.setMoveOverhead(MoveOverhead);
        }

        //------------------------------------------------------------
        // Tuning
        //------------------------------------------------------------
#if defined USE_TUNE
        else
        {
            int param;
            iss >> value;      // "value"
            iss >> param;
            Tunable::setParam(option_name, param);
        }
#endif

    }
    else
    {
        std::cout << "info string Invalid option format." << std::endl;
    }
}

//=================================================================
//! \brief  Lancement d'une recherche sur une position
//-----------------------------------------------------------------
void Uci::go_run(const std::string& abc, const std::string& fen, int dmax, int tmax, int nmax)
{
    std::string auxi;
    std::string bug = "";
    printf("go_run : abc=%s fen=%s dmax=%d tmax=%d nmax=%d \n", abc.c_str(), fen.c_str(), dmax, tmax, nmax);
    transpositionTable.clear();
    threadPool.reset();

    // utiliser setoption name Clear Hash
    // permet de controler l'utilisation de la table

    if (abc == "s")
        auxi = SILVER2;
    else if (abc == "k")
        auxi = KIWIPETE;
    else if (abc == "q")
        auxi = QUIESC;
    else if (abc == "f")
        auxi = FINE_70;
    else if (abc == "w")
        auxi = WAC_2;
    else if (abc == "r")
        auxi = START_FEN;
    else if (abc == "b")
        auxi = bug;
    else if (abc == "p21")
        auxi = POS_21;
    else
        auxi = fen;

    //    Options.log_uci = true;
    if (abc != "n")
    {
        uci_board.reset();
        uci_board.set_fen<true>(auxi, false);
    }
    std::cout << uci_board.display() << std::endl;
    std::string strgo;

    if (dmax != 0)
        strgo = "go depth " + std::to_string(dmax);
    else if (tmax != 0)
        strgo = "go movetime " + std::to_string(tmax);
    else if (nmax != 0)
        strgo = "go nodes " + std::to_string(nmax);

    std::istringstream issgo(strgo);
    parse_go(issgo);
}

//=================================================================
//! \brief  Lancement d'une recherche sur un ensemble de positions
//-----------------------------------------------------------------
void Uci::go_test(int dmax, int tmax)
{
    // le fichier tests/0000.txt contient la liste des fichiers de test
    // les noms non commentés seront utilisés.

    std::string     str_0000("0000.txt");
    std::string     str_path(HOME);
    str_path += "tests/" + str_0000;

    std::string     line;
    std::string     aux;
    std::ifstream   ifs;
    int             numero      = 0;
    int             total_bm    = 0;
    int             total_am    = 0;
    int             total_ko    = 0;
    U64             total_nodes = 0;
    U64             total_time  = 0;
    int             total_depths = 0;

    std::ifstream   f(str_path);
    if (!f.is_open())
    {
        std::cout << "[Uci::go_test] impossible d'ouvrir le fichier (" << str_path << ")" << std::endl;
        return;
    }

    threadPool.set_logUci(false);

    //-------------------------------------------------
    std::string     str_file;
    std::string     str_line;

    while (std::getline(f, str_line))
    {
        // ATTENTION : fin de ligne différentre entre Unix (LF) et Windows (CRLF) !!

        // ligne vide
        if (str_line.size() < 3)
            continue;

        // Commentaire ou espace au début de la ligne
        aux = str_line.substr(0,1);
        if (aux == "#" || aux == " " )
            continue;

        std::cout << "test du fichier : [" << str_line << "]" << std::endl;

        str_file = HOME;
        str_file += "tests/" + str_line;

        ifs.open(str_file, std::ifstream::in);
        if (!ifs.is_open())
        {
            std::cout << "[Uci::go_test] impossible d'ouvrir le fichier [" << str_file << "]" << std::endl;
            continue;
        }

        //---------------------------------------------------
        numero      = 0;
        total_bm    = 0;
        total_am    = 0;
        total_ko    = 0;
        total_nodes = 0;
        total_time  = 0;
        total_depths = 0;

        // Boucle sur l'ensemble des positions de test
        while (std::getline(ifs, line))
        {
            // ligne vide
            if (line.size() < 3)
                continue;

            // Commentaire ou espace au début de la ligne
            aux = line.substr(0,1);
            if (aux == "#" || aux == "/" || aux == " ")
                continue;

            numero++;
            printf("test %3d : ", numero);

            // Extraction des éléments de la ligne
            //  0= position
            //  1= D1 20
            //  2= D2 400
            //            liste1 = split(line, tag);

            // Extraction de la position
            //            aa = liste1[0];

            // Vérification d'unicité de la position
            //            if (std::find(poslist.begin(), poslist.end(), aa) ==  poslist.end())
            //                poslist.push_back(aa);
            //            else
            //            {
            //                std::cout << "--------------------position en double : "      << aa << std::endl;
            //            }

            // Exécution du test
            go_tactics(line, dmax, tmax, total_nodes, total_time, total_depths, total_bm, total_am, total_ko);

        } // boucle position

        ifs.close();

        std::cout << "===============================================" << std::endl;
        std::cout << " Fichier " << str_line << std::endl;
        std::cout << "===============================================" << std::endl;
        std::cout << "total bm    = " << total_bm << std::endl;
        std::cout << "total am    = " << total_am << std::endl;
        std::cout << "total ko    = " << total_ko << std::endl;
        std::cout << "total nodes = " << total_nodes << std::endl;
        std::cout << "time        = " << std::fixed << std::setprecision(3) << static_cast<double>(total_time)/1000.0 << " s" << std::endl;
        std::cout << "nps         = " << std::fixed << std::setprecision(3) << static_cast<double>(total_nodes)/1000.0/static_cast<double>(total_time) << " Mnode/s" << std::endl;
        std::cout << "depth moy   = " << std::fixed << std::setprecision(3) << static_cast<double>(total_depths)/static_cast<double>(total_bm+total_am+total_ko) << std::endl;
        std::cout << "nbr threads = " << threadPool.get_nbrThreads() << std::endl;
        if (dmax !=0)
            std::cout << "depth max   = " << dmax << std::endl;
        if (tmax !=0)
            std::cout << "time max    = " << tmax << std::endl;
        std::cout << "===============================================" << std::endl;

    } // boucle fichiers epd

    f.close();
    threadPool.set_logUci(true);
}


//=============================================================
//! \brief Réalisaion d'un test tactique
//! et comparaison avec le résultat
//-------------------------------------------------------------
void Uci::go_tactics(const std::string& line, int dmax, int tmax, U64& total_nodes, U64& total_time, int& total_depths, int& total_bm, int& total_am, int& total_ko)
{
    transpositionTable.clear();
    threadPool.reset();

    uci_board.reset();
    uci_board.set_fen<true>(line, true);

    const auto start = TimePoint::now();

    //============================================== Lance le calcul
    std::string strgo;
    if (dmax != 0)
        strgo = "go depth " + std::to_string(dmax);
    else if (tmax != 0)
        strgo = "go movetime " + std::to_string(tmax);
    std::istringstream issgo(strgo);
    parse_go(issgo);

    //================================================= Attente des threads
    threadPool.wait(0);

    const auto end = TimePoint::now();
    const auto ms  = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    total_time   += ms;
    total_nodes  += threadPool.get_all_nodes();
    total_depths += threadPool.get_all_depths();
    MOVE best     = threadPool.get_best_move();

    std::string str1 = Move::show(best, 1);
    std::string str2 = Move::show(best, 2);
    std::string str3 = Move::show(best, 3);
    std::stringstream ss;
    std::string found[2] = { "   : ", "OK : "};

    // NOTE : il est possible que dans certains cas, il faut donner
    // à la fois la case de départ et celle d'arrivée pour déterminer
    // réellement le coup.
    bool found_bm = false;
    bool first = true;
    for (auto & e : uci_board.best_moves)
    {
        // On vire tous les caractères inutiles à la comparaison
        for (char c : std::string("+#!?"))
        {
            e.erase(std::remove(e.begin(), e.end(), c), e.end());
        }

        if(str1==e || str2 == e || str3 == e) // attention au format de sortie de Move::show()
        {
            ss << "coup trouvé : " << e;
            found_bm = true;
            break;
        }
        else
        {
            if (first == false)
                ss << " ; ";
            ss << "coup à trouver : " << e;
            ss << ", coup trouvé = " << str1;
        }
        first = false;
    }
    if (!uci_board.best_moves.empty())
        std::cout << found[found_bm] << ss.str() << std::endl;


    bool avoid_am = true;
    first = true;
    for (auto & e : uci_board.avoid_moves)
    {
        // On vire tous les caractères inutiles à la comparaison
        for (char c : std::string("+#!?"))
        {
            e.erase(std::remove(e.begin(), e.end(), c), e.end());
        }

        if(str1 == e || str2 == e || str3 == e) // attention au format de sortie de Move::show()
        {
            avoid_am = false;
            ss << "coup non évité : " << e ;
            break;
        }
        else
        {
            if (first == false)
                ss << " ; ";
            ss << "coup à éviter : " << e;
            ss << ", coup trouvé = " << str1;
        }
        first = false;
    }
    if (!uci_board.avoid_moves.empty())
        std::cout << found[avoid_am] << ss.str() << std::endl;

    if (found_bm)
    {
        total_bm++;
    }
    else if (avoid_am && !uci_board.avoid_moves.empty())
    {
        total_am++;
    }
    else
    {
        total_ko++;
    }
}

//=================================================================
//! \brief  Benchmark
//!         Les tests du fichier bench.csv proviennent d'Ethereal
//-----------------------------------------------------------------
void Uci::bench(int argCount, char* argValue[])
{
    int     scores[256];
    double  times[256];
    U64     nodes[256];
    MOVE    moves[256];

    int     total       = 0;
    U64     total_nodes = 0;
    U64     total_time  = 0;

    int depth       = argCount > 2 ? atoi(argValue[2]) : 16;
    depth           = std::min(depth, MAX_PLY);
    int nbr_threads = argCount > 3 ? atoi(argValue[3]) : 1;
    int hash_size   = argCount > 4 ? atoi(argValue[4]) : HASH_SIZE;

    if (nbr_threads > 1)
        threadPool.set_threads(nbr_threads);
    if (hash_size != HASH_SIZE)
        transpositionTable.set_hash_size(hash_size);

    // Boucle sur l'ensemble des positions de test
    for (const auto& line : bench_pos)
    {
        // Exécution du test
        transpositionTable.clear();
        threadPool.reset();

        uci_board.reset();
        uci_board.set_fen<true>(line, true);
        std::cout << "[# " << total+1 << "] " << line << std::endl;

        const auto start = TimePoint::now();

        //============================================== Lance le calcul
        // Initialize a "go depth <x>" search
        Uci::stop();

        uci_timer = Timer(false, 0, 0, 0, 0, 0, depth, 0, 0);
        uci_timer.start();
        uci_timer.setup(uci_board.side_to_move);

        threadPool.start_thinking(uci_board, uci_timer);

        //================================================= Fin du calcul
        threadPool.wait(0);     // Attente des threads

        const auto end = TimePoint::now();
        const auto ms  = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        scores[total] = threadPool.threadData[0].best_score;
        moves[total]  = threadPool.threadData[0].best_move;
        nodes[total]  = threadPool.get_all_nodes();
        times[total]  = ms;

        total++;
    } // boucle position

    printf("\n===============================================================================\n");

    for (int i=0; i<total; i++)
    {
        printf("position %2d : best_move %5s score %5d nodes %9ld nps %6d \n",
               i+1, Move::name(moves[i]).c_str(), scores[i], nodes[i], static_cast<int>(1000.0*nodes[i]/(times[i]+1.0)) );

        total_nodes  += nodes[i];
        total_time   += times[i];
    }

    printf("===============================================================================\n");

    std::cout << "===============================================" << std::endl;
    std::cout << "total nodes = " << total_nodes << std::endl;
    std::cout << "time        = " << std::fixed << std::setprecision(3) << static_cast<double>(total_time)/1000.0 << " s" << std::endl;
    std::cout << "nps         = " << std::fixed << std::setprecision(3) << static_cast<double>(total_nodes)/1000.0/static_cast<double>(total_time) << " Mnode/s" << std::endl;
    std::cout << "depth       = " << depth << std::endl;
    std::cout << "nbr threads = " << threadPool.get_nbrThreads() << std::endl;
    std::cout << "hash size   = " << transpositionTable.get_hash_size() << std::endl;
    std::cout << "===============================================" << std::endl;
}
