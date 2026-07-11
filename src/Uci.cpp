#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <algorithm>
#include <iomanip>
#include <unordered_set>
#include <unordered_map>

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

    /// UCI::loop() attend une commande depuis stdin, l'analyse puis appelle la
    /// fonction appropriée. Elle intercepte aussi une indication de fin de
    /// fichier (EOF) sur stdin pour garantir une sortie propre si la GUI
    /// meurt de façon inattendue.


    // boucle principale
    std::string token;
    std::string line;
    std::string fen  = START_FEN;
    int         dmax = 6;       // profondeur
    int         tmax = 0;       // temps
    int         nmax = 0;       // nodes
    int         nthreads = 1;   // threads

    do
    {
        if (!std::getline(std::cin, line))   // attend une entrée, ou détecte une fin de fichier (EOF)
            line = "quit";

        std::istringstream iss(line);
        token.clear();                      // évite un token périmé si getline() ne renvoie rien ou une ligne vide
        iss >> std::skipws >> token;

        //-------------------------------------------- gui -> engine

        if (token == "isready")
        {
            /* "isready" sert à la fois de remplacement ping/pong ET signale que
             * l'init est terminée ; la spec UCI cite explicitement l'init des
             * tablebases comme exemple pouvant prendre du temps.
             * Le moteur ne répond "readyok" qu'après avoir traité les commandes
             * setoption en attente.
             * Le seul cas où "isready" doit être répondu immédiatement, c'est
             * après l'init et pendant la recherche : rien ne justifie alors
             * que le moteur reste bloqué.
             */

            // synchronise le moteur avec la GUI
            std::cout << "readyok" << std::endl;
        }

        else if (token == "uci")
        {
            // "uciok" doit apparaître rapidement après que la GUI ait envoyé "uci",
            // un init long n'est pas autorisé ici.

            // affiche les infos du moteur
            std::cout << "id name Zangdar " << VERSION  << std::endl;
            std::cout << "id author Philippe Chevalier" << std::endl;

            // Cette commande indique à la GUI quels paramètres du moteur peuvent être modifiés.

            // Quand dans Arena, on fait "Configure", on fait apparaitre une interface
            // contenant les options données ici.
            // Lorsque l'utilisateur va agir sur une de ces options,
            // Arena va envoyer la commande "setoption.." au programme.

            std::cout << "option name Hash type spin default " << HASH_SIZE <<" min " << MIN_HASH_SIZE << " max " << MAX_HASH_SIZE << std::endl;
            std::cout << "option name Clear Hash type button" << std::endl;
            std::cout << "option name Threads type spin default 1 min 1 max " << std::max(1U, std::thread::hardware_concurrency()) << std::endl;
            std::cout << "option name SyzygyPath type string default " << "<empty>" << std::endl;
            std::cout << "option name SyzygyProbeLimit type spin default 6 min 0 max 7" << std::endl;
            std::cout << "option name MoveOverhead type spin default " << MOVE_OVERHEAD << " min 0 max 10000" << std::endl;

#if defined USE_TUNING
            std::cout << Tunable::paramsToUci();
#endif

            std::cout << "uciok" << std::endl;
        }

        else if (token == "position")
        {
            // met en place la position décrite par la chaîne fen
            uci_board.initialisation();
            uci_board.parse_position(iss);
        }

        else if (token == "ucinewgame" || token == "new")
        {
            // La prochaine recherche (lancée par "position" puis "go") viendra
            // d'une partie différente.
            // Il faut arrêter une éventuelle recherche en cours AVANT de réinitialiser.
            threadPool.stop();
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
            // arrête le calcul au plus vite
            stop();
        }

        else if (token == "quit")
        {
            // quitte le programme au plus vite
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
            std::cout << "eval [fen]                    : test evaluation sur la position courante ou le fen donné"   << std::endl;
            std::cout << "syzygy [fen]                  : test syzygy sur la position courante ou le fen donné" << std::endl;
            std::cout << "run <fen> [dmax][tmax][nmax][thread]      : test de recherche <Silver2/Kiwipete/Quies/Fine70/WAC2/BUG/REF>"           << std::endl;
            std::cout << "mirror                        : test mirror"                                          << std::endl;
            std::cout << "see                           : test see"                                             << std::endl;
            std::cout << "fen [str]                     : positionne la chaine fen"                             << std::endl;
            std::cout << "dmax [p]                      : positionne la profondeur de recherche"                << std::endl;
            std::cout << "tmax [ms]                     : positionne le temps de recherche en millisecondes"    << std::endl;
            std::cout << "nmax [n]                      : positionne le nombre de nodes"                        << std::endl;
            std::cout << "display                       : affiche la position"                                  << std::endl;
            std::cout << "systeme                       : informe sur les minimums systeme"                     << std::endl;
        }

        else if (token == "v")
        {
            std::cout << "Zangdar " << VERSION << " NNUE " << NET_NAME << std::endl;
            std::cout << transpositionTable.info();
            uci_board.syzygy_info();
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
            std::string rest;
            std::getline(iss, rest);
            test_eval(rest.empty() ? fen : rest);
        }

        else if(token == "see")
        {
            test_see();
        }

        else if(token == "syzygy")
        {
            std::string rest;
            std::getline(iss, rest);
            // Supprime l'espace en tête si présent
            if (!rest.empty() && rest.front() == ' ')
                rest.erase(0, 1);
            test_syzygy(rest.empty() ? fen : rest);
        }

        else if (token == "run")
        {
            std::string str;
            iss >> str;
            iss >> dmax;        // depth
            iss >> tmax;        // time
            iss >> nmax;        // nodes
            iss >> nthreads;    // threads

            if (nthreads > 1)
                threadPool.set_threads(nthreads);

            go_run(str, fen, dmax, tmax, nmax, nthreads);
        }

        else if (token == "test")
        {
            go_test(dmax, tmax);
        }

        else if (token == "fen")
        {
            std::getline(iss, fen);
            uci_board.initialisation();
            uci_board.set_fen(fen, false);
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

        else if (token == "spsa")
        {
            std::cout << Tunable::paramsToSpsa();
        }

        else if (token == "yyy")
        {
            // commande For Your Eyes Only
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

            // Jeux d'instructions, du plus ancien au plus récent
#if defined(__SSE__)
            std::cout << "__SSE__        OK " << std::endl;
#endif
#if defined(__SSE2__)
            std::cout << "__SSE2__       OK " << std::endl;
#endif
#if defined(__SSE3__)
            std::cout << "__SSE3__       OK " << std::endl;
#endif
#if defined(__SSSE3__)
            std::cout << "__SSSE3__      OK " << std::endl;
#endif
#if defined(__SSE4_1__)
            std::cout << "__SSE4_1__     OK " << std::endl;
#endif
#if defined(__SSE4_2__)
            std::cout << "__SSE4_2__     OK " << std::endl;
#endif
#if defined(__POPCNT__)
            std::cout << "__POPCNT__     OK " << std::endl;
#endif
#if defined(__AVX__)
            std::cout << "__AVX__        OK " << std::endl;
#endif
#if defined(__AVX2__)
            std::cout << "__AVX2__       OK " << std::endl;
#endif
#if defined(__FMA__)
            std::cout << "__FMA__        OK " << std::endl;
#endif
#if defined(__BMI2__)
            std::cout << "__BMI2__       OK " << std::endl;
#endif
#if defined(__AVX512F__)
            std::cout << "__AVX512F__    OK " << std::endl;
#endif
#if defined (__AVX512BW__)
            std::cout << "__AVX512BW__   OK " << std::endl;
#endif
#if defined (__AVX512VNNI__)
            std::cout << "__AVX512VNNI__ OK " << std::endl;
#endif
#if defined(__ARM_NEON)
            std::cout << "__ARM_NEON     OK " << std::endl;
#endif
#if defined(USE_PEXT)
            std::cout << "USE_PEXT       OK (attaques glisseurs par PEXT)" << std::endl;
#else
            std::cout << "USE_PEXT       absent (magic bitboards)" << std::endl;
#endif

        }
    } while (token != "quit");

}


//==============================================================
//! \brief commande uci : stop
//! Arrête la recherche en cours.
//! Cela affiche en général une dernière info string et le meilleur coup.
//--------------------------------------------------------------
void Uci::stop()
{
    threadPool.stop();
}

//==============================================================
//! \brief commande uci : quit
//! Quitte le programme.
//--------------------------------------------------------------
void Uci::quit()
{
    threadPool.quit();
}

//==============================================================
//! \brief commande uci : go
//! Lance la recherche
//!
//! \param[in]  iss  flux contenant les paramètres de la commande "go"
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
    U64 nodes       = 0;
    int movetime    = 0;

    // Arrête toute recherche en cours
    Uci::stop();

    std::string token;
    token.clear();

    // On a reçu une commande de lancement. On extrait tous les paramètres de
    // la commande puis on démarre la recherche.

    while (iss >> token)
    {
        if (token == "infinite")
        {
            // recherche jusqu'à la commande "stop". Ne pas sortir de la recherche sans y être invité dans ce mode !
            infinite = true;
        }
        else if (token == "wtime")
        {
            // il reste x msec sur la pendule des Blancs
            iss >> wtime;
        }
        else if (token == "btime")
        {
            // il reste x msec sur la pendule des Noirs
            iss >> btime;
        }
        else if (token == "winc")
        {
            // incrément des Blancs par coup, en ms
            iss >> winc;
        }
        else if (token == "binc")
        {
            // incrément des Noirs par coup, en ms
            iss >> binc;
        }
        else if (token == "movestogo")
        {
            // il reste x coups avant la prochaine cadence
            iss >> movestogo;
        }
        else if (token == "depth")
        {
            // recherche x plies seulement.
            iss >> depth;
            depth = std::min(depth, MAX_PLY);
        }
        else if (token == "nodes")
        {
            // recherche x nodes maximum.
            iss >> nodes;
        }
        else if (token == "movetime")
        {
            // recherche exactement x millisecondes
            uint64_t searchTime;
            iss >> searchTime;
            movetime = searchTime;
        }
    }

    // Initialise le gestionnaire de temps
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

    // démarre la recherche
    threadPool.start_thinking(uci_board, uci_timer);
}

//=========================================================
//! \brief  Parse les commandes UCI relatives au changement
//! d'option.
//!
//! \param[in]  iss  flux contenant le nom et la valeur de l'option
//---------------------------------------------------------
void Uci::parse_options(std::istringstream& iss)
{

    /*
setoption name <id> [value <x>]
    Cette commande est envoyée au moteur quand l'utilisateur veut changer un paramètre
    interne du moteur. Pour le type "button", aucune valeur n'est nécessaire.
    Une chaîne est envoyée par paramètre, et uniquement quand le moteur est en attente.
    Le nom et la valeur de l'option dans <id> ne devraient pas être sensibles à la casse
    et peuvent contenir des espaces.
    Les sous-chaînes "value" et "name" devraient être évitées dans <id> et <x> pour permettre
    une analyse non ambiguë, par exemple ne pas utiliser <name> = "draw value".
    Voici quelques exemples de chaînes :
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
            // Il faut arrêter la recherche avant de réallouer.
            threadPool.stop();
            transpositionTable.init_size(mb);
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
                // Il faut arrêter la recherche avant de clear la TT.
                threadPool.stop();
                transpositionTable.clear();
            }
        }

        else if (option_name == "Threads")
        {
            iss >> value;      // "value"
            int nbr;
            iss >> nbr;

            threadPool.set_threads(nbr);
        }

        else if (option_name == "SyzygyPath")
        {
            // Pour mettre plusieurs chemins
            // setoption name SyzygyPath value /mnt/Datas/Echecs/Syzygy/345:/mnt/Datas/Echecs/Syzygy/6

            iss >> value;      // "value"

            std::string path;
            iss >> path;

            if (path != "<empty>" && !path.empty())
            {
#if defined DEBUG_LOG
                sprintf(message, "Uci::parse_options : SyzygyPath (%s) ", path.c_str());
                printlog(message);
#endif
                tb_init(path);

                // n'utilise les TB que si le chargement a réussi
                if (TB_LARGEST > 0)
                {
                    threadPool.set_useSyzygy(true);
                    // Initialise probeLimit à TB_LARGEST si non défini explicitement
                    if (threadPool.get_syzygyProbeLimit() == 0)
                        threadPool.set_syzygyProbeLimit(TB_LARGEST);
                }
            }
        }

        else if (option_name == "SyzygyProbeLimit")
        {
            int limit;
            iss >> value;      // "value"
            iss >> limit;
            threadPool.set_syzygyProbeLimit(limit);
        }

        else if (option_name == "MoveOverhead")
        {
            int MoveOverhead;       // marge de sécurité sur le temps alloué, pour éviter de perdre au temps

            iss >> value;      // "value"
            iss >> MoveOverhead;
            uci_timer.setMoveOverhead(MoveOverhead);
        }

        //------------------------------------------------------------
        // Tuning
        //------------------------------------------------------------
#if defined USE_TUNING
        else
        {
            int param;
            iss >> value;      // "value"
            iss >> param;
            Tunable::setParam(option_name, param);
            threadPool.reinit_reductions();
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
//! \param[in]  abc      = code de la position à tester (s/k/q/f/w/r/p21/b1/b2) ou "n" pour garder la position courante
//! \param[in]  fen      = fen à utiliser si abc ne correspond à aucun code connu
//! \param[in]  dmax     = depth max
//! \param[in]  tmax     = time max
//! \param[in]  nmax     = nodes max
//! \param[in]  nthreads = nombre de threads
//!
//-----------------------------------------------------------------
void Uci::go_run(const std::string& abc, const std::string& fen, int dmax, int tmax, int nmax, int nthreads)
{
    std::string auxi;
    printf("go_run : abc=%s fen=%s depth_max=%d time_max=%d node_max=%d threads=%d \n", abc.c_str(), fen.c_str(), dmax, tmax, nmax, nthreads);
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
    else if (abc == "p21")
        auxi = POS_21;
    else if (abc == "b1")
        auxi = BUG_1;
    else if (abc == "b2")
        auxi = BUG_2;
    else
        auxi = fen;

    if (abc != "n")
    {
        uci_board.initialisation();
        uci_board.set_fen(auxi, false);
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
//! \brief  Clé de position d'une ligne EPD/FEN : les 4 premiers champs
//!         (placement, trait, roque, e.p.), sans les annotations (bm/am/id...)
//!         ni les éventuels compteurs demi-coups/coups. Sert à détecter les
//!         positions en double dans une suite de tests.
//!
//! \param[in]  epd_line  ligne EPD/FEN à analyser
//!
//! \return Clé (4 premiers champs concaténés) identifiant la position
//-----------------------------------------------------------------
static std::string position_key(const std::string& epd_line)
{
    std::istringstream iss(epd_line);
    std::string tok, key;
    for (int i = 0; i < 4 && (iss >> tok); ++i)
        key += (i ? " " : "") + tok;
    return key;
}

//=================================================================
//! \brief  Lancement d'une recherche sur un ensemble de positions
//!
//! \param[in]  dmax  profondeur max de recherche
//! \param[in]  tmax  temps max de recherche
//-----------------------------------------------------------------
void Uci::go_test(int dmax, int tmax)
{
    // Détection de doublons de position sur TOUTE la suite (tous fichiers confondus) :
    // hash set -> O(1) par position (au lieu de l'ancien std::find linéaire, O(n²)).
    // On mémorise aussi la 1ʳᵉ apparition pour un message utile, et on saute le doublon.
    std::unordered_set<std::string>              seen_positions;
    std::unordered_map<std::string, std::string> first_location;
    int total_dup = 0;

    // le fichier tests/0000.txt contient la liste des fichiers de test
    // les noms non commentés seront utilisés.

    std::string     str_0000("0000.txt");
    std::string     str_path(MAISON);
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

        str_file = MAISON;
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

            // Doublon de position ? (suite entière, tous fichiers confondus)
            const std::string key = position_key(line);
            if (seen_positions.insert(key).second == false)
            {
                total_dup++;
                std::cout << "  DOUBLON ignoré : position déjà vue en "
                          << first_location.at(key) << "  [" << key << "]" << std::endl;
                continue;   // on ne re-teste pas la même position
            }
            first_location[key] = str_line + " test " + std::to_string(numero + 1);

            numero++;
            printf("test %3d : ", numero);

            // Exécution du test (la ligne EPD est parsée directement dans go_tactics)
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

    //-------------------------------------------------
    // Bilan des doublons de position sur toute la suite
    std::cout << "===============================================" << std::endl;
    if (total_dup > 0)
        std::cout << total_dup << " position(s) en double détectée(s) et ignorée(s) ; "
                  << seen_positions.size() << " positions uniques testées." << std::endl;
    else
        std::cout << "Aucune position en double (" << seen_positions.size()
                  << " positions uniques)." << std::endl;
    std::cout << "===============================================" << std::endl;

    threadPool.set_logUci(true);
}


//=============================================================
//! \brief Réalisaion d'un test tactique
//! et comparaison avec le résultat
//!
//! \param[in]      line          ligne EPD/FEN décrivant la position et le(s) coup(s) attendu(s)
//! \param[in]      dmax          profondeur max de recherche
//! \param[in]      tmax          temps max de recherche
//! \param[in,out]  total_nodes   cumul du nombre de nodes calculés
//! \param[in,out]  total_time    cumul du temps de calcul (ms)
//! \param[in,out]  total_depths  cumul des profondeurs atteintes
//! \param[in,out]  total_bm      cumul du nombre de "best move" trouvés
//! \param[in,out]  total_am      cumul du nombre de "avoid move" évités
//! \param[in,out]  total_ko      cumul du nombre d'échecs
//-------------------------------------------------------------
void Uci::go_tactics(const std::string& line, int dmax, int tmax, U64& total_nodes, U64& total_time, int& total_depths, int& total_bm, int& total_am, int& total_ko)
{
    transpositionTable.clear();
    threadPool.reset();

    uci_board.initialisation();
    uci_board.set_fen(line, true);

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

    // NOTE : il est possible que dans certains cas, il faille donner
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
//! commande : bench depth threads hash
//!
//! \param[in]  argCount  nombre d'arguments de la ligne de commande
//! \param[in]  argValue  arguments de la ligne de commande (depth, threads, hash)
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

    int depth       = argCount > 2 ? atoi(argValue[2]) : 20;
    depth           = std::min(depth, MAX_PLY-1);
    int nbr_threads = argCount > 3 ? atoi(argValue[3]) : 1;
    int hash_size   = argCount > 4 ? atoi(argValue[4]) : HASH_SIZE;

    if (nbr_threads > 1)
        threadPool.set_threads(nbr_threads);
    if (hash_size != HASH_SIZE)
        transpositionTable.init_size(hash_size);

    // Boucle sur l'ensemble des positions de test
    for (const auto& line : bench_pos)
    {
        // Exécution du test
        transpositionTable.clear();
        threadPool.reset();

        uci_board.initialisation();
        uci_board.set_fen(line, true);
        std::cout << "[# " << total+1 << "] " << line << std::endl;

        const auto start = TimePoint::now();

        //============================================== Lance le calcul
        // Initialise une recherche "go depth <x>"
        Uci::stop();

        uci_timer = Timer(false, 0, 0, 0, 0, 0, depth, 0, 0);
        uci_timer.start();
        uci_timer.setup(uci_board.side_to_move);

        threadPool.start_thinking(uci_board, uci_timer);

        //================================================= Fin du calcul
        threadPool.wait(0);     // Attente des threads

        const auto end = TimePoint::now();
        const auto ms  = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        int bt = threadPool.get_best_thread();
        scores[total] = threadPool.search[bt].pv_scores[threadPool.search[bt].best_depth];
        moves[total]  = threadPool.search[bt].pv_moves[threadPool.search[bt].best_depth];
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
    std::cout << "nps         = " << static_cast<U64>(1000.0 * total_nodes / (total_time + 1)) << std::endl;
    std::cout << "depth       = " << depth << std::endl;
    std::cout << "nbr threads = " << threadPool.get_nbrThreads() << std::endl;
    std::cout << "hash size   = " << transpositionTable.get_hash_size() << std::endl;
    std::cout << "===============================================" << std::endl;
}
