#include <string>
#include <fstream>
#include <iostream>
#include <chrono>
#include <iomanip>      // std::setw
#include <filesystem>
#include <memory>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <unordered_set>
#include <unordered_map>

#include "defines.h"
#include "Board.h"
#include "Move.h"
#include "Search.h"

bool test_mirror(Board& board, const std::string& line);
template <Color C, bool divide=false>
[[nodiscard]] std::uint64_t perft(Board& board, const int depth) noexcept;

//====================================================
//! \brief Réalisation d'une série de tests "perft"
//!
//! On va faire, pour chaque position, plusieurs tests
//!
//! \param[in]  abc     suffixe du fichier perftsuite_<abc>.epd à charger
//! \param[in]  dmax    profondeur max
//----------------------------------------------------
void test_suite(const std::string& abc, int dmax)
{
    std::cout << "Répertoire courant : " << std::filesystem::current_path() << std::endl;

    // Il y a 2 suites perft
    //  perftsuite_ref  : petite suite permettant de voir s'il y a eu perte de perfo
    //  perftsuite_big  : énorme suite pour contrôler le générateur de coups

    std::string     str_file = "tests/perftsuite_" + abc + ".epd";

    std::ifstream file(str_file);
    if (!file.is_open())
    {
        std::cout << "[test_suite] impossible d'ouvrir le fichier perftsuite.epd " << std::endl;
        return;
    }

    std::string     line;
    std::string     str;
    std::vector<std::string> liste1;
    std::vector<std::string> liste2;
    std::string     fen;

    std::string     aux;
    int             depth;
    U64             expected;
    U64             actual;
    U64             total_expected = 0;
    U64             total_actual   = 0;
    int             total_tests    = 0;
    int             passed_tests   = 0;
    int             failed_tests   = 0;
    int             numero         = 0;
    int             indice;
    char            tag = ';';
    char            tag2 = ' ';

    // Détection de doublons de position (même principe que Uci::go_test) :
    // hash set -> O(1) par position.
    // On mémorise la 1ʳᵉ apparition pour un message utile, et on saute le doublon :
    // un perft en double ne teste rien de plus et coûte cher.
#ifndef NDEBUG
    std::unordered_set<std::string>           seen_positions;
    std::unordered_map<std::string, int>      first_location;
    int             total_dup      = 0;
#endif
    Board CB;
    auto start = TimePoint::now();

    // Boucle sur l'ensemble des positions de test
    while (std::getline(file, line))
    {
        numero++;

        // ligne vide
        if (line.size() < 3)
            continue;

        // Commentaire ou espace au début de la ligne
        aux = line.substr(0,1);
        if (aux == "/" || aux == " ")
            continue;

        // Extraction des éléments de la ligne
        //  0= position
        //  1= D1 20
        //  2= D2 400
        liste1 = split(line, tag);

        fen    = liste1.at(0);                  // position fen

        // Doublon de position ? (les compteurs de coups sont ignorés par la clé)
#ifndef NDEBUG
        const std::string key = position_key(fen);
        if (seen_positions.insert(key).second == false)
        {
            total_dup++;
            std::cout << "  DOUBLON ignoré : position déjà vue ligne "
                      << first_location.at(key) << "  [" << key << "]" << std::endl;
            continue;
        }
        first_location[key] = numero;
#endif

        // int sc = CB->evaluate();
        // printf("%d\n", sc);

        // nombre de profondeurs possibles
        int nbr_prof = liste1.size() - 1;

        // boucle sur les profondeurs de test
        for (int i=1; i<=nbr_prof; i++)
        {
            CB.initialisation();
            CB.set_fen(fen, false);

            aux     = liste1.at(i);                     // "D1 20"
            liste2  = split(aux, tag2);                  // 0= "D1"; 1= "20"

            if (liste2.size() > 1)
            {
                // la profondeur est indiquée : "D1 20"
                indice  = 1;
                aux     = liste2.at(0);                     // "D1"
                depth   = std::stoi(aux.substr(1, aux.size()));   // profondeur = 1
            }
            else
            {
                // la profondeur n'est pas indiquée : "20"
                indice = 0;
                depth  = i;
            }

            if (depth <= dmax)
            {
                aux = liste2.at(indice);                     // "20"
                expected = std::stoull(aux);

                // Exécution du test perft pour cette position et cette profondeur
                if (CB.turn() == WHITE)
                    actual = perft<WHITE, false>(CB, depth);
                else
                    actual = perft<BLACK, false>(CB, depth);

                total_expected += expected;
                total_actual   += actual;
                total_tests++;

                if (expected == actual)
                {
                    passed_tests++;
                }
                else
                {
                    std::cout << "ligne=" << numero << " ; depth=" << depth << "  FAILED : attendus=" << expected << " ; trouves=" << actual << std::endl;
                    std::cout << line << std::endl;
                    Board board;

                    board.set_fen(fen, false);
                    failed_tests++;
                    std::cout << board.display() << std::endl;
                    return;
                }
            }
        } // boucle depth
    } // boucle position

    // Elapsed time in milliseconds
    auto end = TimePoint::now();
    auto sec = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()/1000.0;

    file.close();

    std::cout << "# Passed       " << std::setw(10) << passed_tests << std::endl;
    std::cout << "# Failed       " << std::setw(10) << failed_tests << std::endl;
    std::cout << "# Total        " << std::setw(10) << total_tests << std::endl;
#ifndef NDEBUG
    if (total_dup > 0)
        std::cout << total_dup << " position(s) en double détectée(s) et ignorée(s) ; "
                  << seen_positions.size() << " positions uniques testées." << std::endl;
    else
        std::cout << "Aucune position en double (" << seen_positions.size()
                  << " positions uniques)." << std::endl;
#endif
    std::cout << "Moves Actual   " << std::setw(10) << total_actual << std::endl;
    std::cout << "Moves Expected " << std::setw(10) << total_expected << std::endl;
    std::cout << "Time           " << std::setw(9)  << sec << std::endl;
    if (sec > 0)
        std::cout << "Million Moves/s        " << std::setw(9) << static_cast<double>(total_actual)/static_cast<double>(sec)/1000000.0 << std::endl;

    std::cout << "********************" << std::endl;

}

//========================================================
//! \brief  lancement d'un test perft sur une position
//!
//! \param[in]  str     code désignant une position de référence
//!                      (r, k, s, f, p21, pos3...pos6) ; sinon m_fen est utilisé
//! \param[in]  m_fen   FEN de la position à tester si str ne correspond
//!                      à aucun code connu
//! \param[in]  depth   profondeur max de recherche
//---------------------------------------------------------
template <bool divide>
void test_perft(const std::string& str, const std::string& m_fen, int depth)
{

    // Le programme JetChess donne les valeurs, ainsi que les divide

    std::string fen;
    std::array<U64, 11> nbr;

    if (str == "r")
    {
        fen = START_FEN;
        std::array<U64, 11> nbrk = {1, 20, 400, 8902,  197281,   4865609,   119060324,   3195901860,  84998978956,   2439530234167,  69352859712417  };
        nbr = nbrk;
    }
    else if (str == "k")
    {
        fen = KIWIPETE;
        std::array<U64, 11> nbrk = {1, 48, 2039, 97862, 4085603, 193690690, 8031647685, 374190009323, 0, 0};
        nbr = nbrk;
    }
    else if (str == "s")
    {
        fen = SILVER2;
        std::array<U64, 11> nbrk = {1, 45, 1927, 82516, 3511858, 146708852, 6179265298, 253624821177, 0, 0, 0};
        nbr = nbrk;
    }
    else if (str == "f")
    {
        fen = FINE_70;
        std::array<U64, 11> nbrk = {1, 3, 15, 90, 396, 2090, 10545, 61641, 301431, 1745898, 8759106};
        nbr = nbrk;
    }
    else if (str == "p21")
    {
        fen = POS_21;
        std::array<U64, 11> nbrk = {1, 28, 1289, 34057, 1559915, 42153037, 1919646125, 53353120381, 0, 0, 0 };
        nbr = nbrk;
    }
    else if (str == "pos3")
    {
        fen = "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1";
        std::array<U64, 11> nbrk = {1, 14, 191, 2812, 43238, 674624, 11030083, 178633661, 3009794393, 0, 0 };
        nbr = nbrk;
    }
    else if (str == "pos4")
    {
        fen = "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1";
        std::array<U64, 11> nbrk = {1, 6, 264, 9467, 422333, 15833292, 706045033, 0, 0, 0, 0 };
        nbr = nbrk;
    }
    else if (str == "pos5")
    {
        fen = "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8";
        std::array<U64, 11> nbrk = {1, 44, 1486, 62379, 2103487, 89941194, 0, 0, 0, 0, 0 };
        nbr = nbrk;
    }
    else if (str == "pos6")
    {
        fen = "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10";
        std::array<U64, 11> nbrk = {1, 46, 2079, 89890, 3894594, 164075551, 6923051137, 287188994746, 11923589843526,  490154852788714, 0 };
        nbr = nbrk;
    }
    else
    {
        fen = m_fen;
    }


    /* https://www.chessprogramming.org/Perft_Results
     * http://www.rocechess.ch/perft.html
     */
    Board CB;
    CB.set_fen(fen, false);
    std::cout << CB.display() << std::endl;
    std::cout << std::endl;

    auto start      = TimePoint::now();
    U64  total;

    if (CB.turn() == WHITE)
        total = perft<WHITE, divide>(CB, depth);
    else
        total = perft<BLACK, divide>(CB, depth);

    auto end        = TimePoint::now();
    auto delta      = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    auto msec       = delta.count();

    std::cout << "Time            : " << msec << " msec" << std::endl;
    std::cout << "Total           : " << std::setw(10) << total << std::endl;
    if (msec > 0)
        std::cout << "Million Moves/s : " << std::fixed << std::setprecision(1) << static_cast<double>(total)/static_cast<double>(msec)/1000.0 << std::endl;
    if (total == nbr[depth])
        std::cout << "resultat        : OK " << std::endl;
    else
        std::cout << "resultat        : >>>>>>>>>>>>>>>>>>>>>>>>>>>> KO : bon = " << nbr[depth] << std::endl;
}

//======================================================
//! \brief  Affiche tous les coups possibles ainsi que leur évaluation NNUE
//!         L'affichage est trié par évaluation décroissante
//!
//! \param[in]  fen     position de départ au format FEN
//------------------------------------------------------
void test_eval(const std::string& fen)
{
    Board board(fen);
    std::cout << board.display() << std::endl;

    auto search = std::make_unique<Search>();
    search->nnue.start_search(board);

    int eval = search->evaluate(board);
    printf("side = %s : evaluation = %d \n\n", side_name[board.turn()].c_str(), eval);

    MoveList ml;

    if (board.turn() == WHITE)
        board.legal_moves<WHITE, MoveGenType::ALL>(ml);
    else
        board.legal_moves<BLACK, MoveGenType::ALL>(ml);

    printf("%zu coups legaux\n\n", ml.count);

    // Jouer chaque coup et évaluer la position résultante
    for (size_t index = 0; index < ml.count; index++)
    {
        MOVE move = ml.mlmoves[index].move;

        if (board.turn() == WHITE)
        {
            search->make_move<WHITE, true>(board, move);
            ml.mlmoves[index].value = -search->evaluate(board);
            search->undo_move<WHITE, true>(board);
        }
        else
        {
            search->make_move<BLACK, true>(board, move);
            ml.mlmoves[index].value = -search->evaluate(board);
            search->undo_move<BLACK, true>(board);
        }
    }

    // Tri par évaluation décroissante
    for (size_t i = 0; i < ml.count; i++)
    {
        for (size_t j = i + 1; j < ml.count; j++)
        {
            if (ml.mlmoves[j].value > ml.mlmoves[i].value)
                std::swap(ml.mlmoves[i], ml.mlmoves[j]);
        }
    }

    // Affichage
    printf("%-6s %6s  %s\n", "coup", "eval", "flags");
    printf("------ ------  -----\n");

    for (size_t index = 0; index < ml.count; index++)
    {
        MOVE move = ml.mlmoves[index].move;
        int  val  = ml.mlmoves[index].value;

        std::string flags;
        if (Move::is_capturing(move))  flags += "cap ";
        if (Move::is_enpassant(move))  flags += "ep ";
        if (Move::is_promoting(move))  flags += "prom ";
        if (Move::is_castling(move))   flags += "castle ";

        // Vérifier si le coup donne échec
        if (board.turn() == WHITE)
        {
            board.make_move<WHITE, false>(search->nnue.get_accumulator(), move);
            if (board.is_in_check()) flags += "check ";
            board.undo_move<WHITE>();
        }
        else
        {
            board.make_move<BLACK, false>(search->nnue.get_accumulator(), move);
            if (board.is_in_check()) flags += "check ";
            board.undo_move<BLACK>();
        }

        printf("%-6s %6d  %s\n", Move::name(move).c_str(), val, flags.c_str());
    }
}



//====================================================
//! \brief Test Syzygy : sonde les tables pour la position
//!        donnée en FEN et affiche le résultat détaillé.
//!
//! \param[in]  fen     position à sonder au format FEN
//----------------------------------------------------
void test_syzygy(const std::string& fen)
{
    Board board;
    board.initialisation();
    board.set_fen(fen, false);
    board.probe_root_test();
}

//====================================================
//! \brief Réalisation d'un test contrôlant si
//! l'évaluation est symétrique.
//!
//----------------------------------------------------
void test_mirror(void)
{
    // std::string     str_file = "tests/mirror.epd";
    std::string     str_file = "tests/1000.epd";

    std::cout << "[test_mirror]  " << str_file << std::endl;

    std::ifstream file(str_file);
    if (!file.is_open())
    {
        std::cout << "[test_mirror] impossible d'ouvrir le fichier " << str_file << std::endl;
        return;
    }

    std::string     line;
    int numero = 0;
    Board board;

    // Boucle sur l'ensemble des positions de test
    while (std::getline(file, line))
    {
        // ligne vide
        if (line.size() < 3)
            continue;

        numero++;

        if (test_mirror(board, line) == false)
        {
            std::cout << "Mirror Fail : " << line << std::endl;
        }
        else
        {
            //            std::cout << "Mirror OK : " << line << std::endl;
        }

        if((numero % 100) == 0)
            std::cout << "position " << numero << std::endl;
    }

    std::cout << "[test_mirror]  fini " << std::endl;

    file.close();
}

//===========================================================
//! \brief  Test vérifiant la symétrie de l'évaluation
//! L'évaluation de la position originale doit être identique
//! à celle de la position miroir (couleurs inversées).
//!
//! \param[in,out]  board   échiquier de travail, réinitialisé
//!                          puis positionné successivement sur
//!                          la position et sa position miroir
//! \param[in]      line    ligne du fichier de test contenant la FEN
//!
//! \return true si les deux évaluations sont identiques, false sinon
//-----------------------------------------------------------
bool test_mirror(Board& board, const std::string& line)
{
    // Evaluation de la position originale
    board.initialisation();
    board.set_fen(line, false);

    auto search = std::make_unique<Search>();

    search->nnue.start_search(board);
    int ev1 = search->evaluate(board);

    // Evaluation de la position miroir
    board.initialisation();
    board.mirror_fen(line, true);

    search->nnue.start_search(board);
    int ev2 = search->evaluate(board);

    if (ev1 != ev2)
    {
        std::cout << ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> " << std::endl;
        std::cout << "Mirror Fail : ev1 = " << ev1 << " ; ev2 = " << ev2 << std::endl;
        board.initialisation();
        board.set_fen(line, true);
        std::cout << board.display() << std::endl;
        board.initialisation();
        board.mirror_fen(line, true);
        std::cout << board.display() << std::endl;
        return false;
    }

    return true;
}

#include "MovePicker.h"

//========================================================
//! \brief  Test de la Static Exchange Evaluation
//--------------------------------------------------------
//=========================================================================
//! \brief  Évaluateur des attentes de tests/see.epd
//!
//! Les attentes sont écrites symboliquement (« B - P », « max(0, R - N) »)
//! et non en dur, pour qu'un changement du barème SEE ne rende pas la suite
//! fausse. Grammaire :
//!
//!     expr    = terme (('+' | '-') terme)*
//!     terme   = facteur ('*' facteur)*
//!     facteur = '-' facteur | '(' expr ')' | ('max'|'min') '(' expr ',' expr ')'
//!             | entier | lettre de pièce (P N B R Q K)
//=========================================================================
namespace {

class SeeExpr
{
public:
    explicit SeeExpr(const std::string& source) : s(source) {}

    //! \brief  Évalue l'expression. Rend false si elle est mal formée.
    bool parse(int& result)
    {
        result = expr();
        skip();
        return ok && pos == s.size();
    }

private:
    const std::string& s;
    size_t pos = 0;
    bool   ok  = true;

    void skip() { while (pos < s.size() && std::isspace(static_cast<unsigned char>(s[pos]))) pos++; }

    bool accept(char c)
    {
        skip();
        if (pos < s.size() && s[pos] == c) { pos++; return true; }
        return false;
    }

    bool keyword(const char* kw)
    {
        skip();
        const size_t n = std::strlen(kw);
        if (s.compare(pos, n, kw) != 0)
            return false;
        pos += n;
        return true;
    }

    int expr()
    {
        int v = term();
        while (ok)
        {
            if      (accept('+')) v += term();
            else if (accept('-')) v -= term();
            else break;
        }
        return v;
    }

    int term()
    {
        int v = factor();
        while (ok && accept('*'))
            v *= factor();
        return v;
    }

    int factor()
    {
        skip();
        if (pos >= s.size()) { ok = false; return 0; }

        if (accept('-')) return -factor();
        if (accept('+')) return  factor();

        if (accept('('))
        {
            const int v = expr();
            if (!accept(')')) ok = false;
            return v;
        }

        const bool is_max = keyword("max");
        const bool is_min = is_max ? false : keyword("min");
        if (is_max || is_min)
        {
            if (!accept('(')) { ok = false; return 0; }
            const int a = expr();
            if (!accept(',')) { ok = false; return 0; }
            const int b = expr();
            if (!accept(')')) { ok = false; return 0; }
            return is_max ? std::max(a, b) : std::min(a, b);
        }

        if (std::isdigit(static_cast<unsigned char>(s[pos])))
        {
            int v = 0;
            while (pos < s.size() && std::isdigit(static_cast<unsigned char>(s[pos])))
                v = v * 10 + (s[pos++] - '0');
            return v;
        }

        switch (s[pos++])
        {
        case 'P': return see_value(PieceType::PAWN);
        case 'N': return see_value(PieceType::KNIGHT);
        case 'B': return see_value(PieceType::BISHOP);
        case 'R': return see_value(PieceType::ROOK);
        case 'Q': return see_value(PieceType::QUEEN);
        case 'K': return see_value(PieceType::KING);
        default:  ok = false; return 0;
        }
    }
};

} // namespace

void test_see()
{
    std::string   str_file = "tests/see.epd";

    std::ifstream file(str_file);
    if (!file.is_open())
    {
        std::cout << "[test_see] impossible d'ouvrir le fichier see.epd " << std::endl;
        return;
    }

    std::string     line;
    std::string     strm;
    MOVE            move;
    std::vector<std::string> liste1;
    std::vector<std::string> liste2;
    std::string     fen;
    int             score;

    std::string     aux;
    int             total_tests    = 0;

    // Test du signe : fast_see(move, 0) doit avoir le même signe que "score"
    int             passed_tests_sign = 0;
    int             failed_tests_sign = 0;

    // Test de la valeur exacte : fast_see(move, score) doit passer,
    // fast_see(move, score+1) doit échouer
    int             passed_tests_exact = 0;
    int             failed_tests_exact = 0;

    int             numero         = 0;
    char            tag = ';';

    // Détection de doublons. L'unité de test est le couple (position, coup) et non
    // la position seule : plusieurs lignes testent volontairement le même échiquier
    // avec des coups différents (les 4 promotions). La clé porte sur le coup une
    // fois RÉSOLU, donc deux notations du même coup (« Nxe5 » et « d3e5 ») sont bien
    // vues comme un doublon.
    std::unordered_set<std::string> seen_tests;
    int             total_dup      = 0;

    Board board;
    MoveList ml;

    // Boucle sur l'ensemble des positions de test
    while (std::getline(file, line))
    {
        // ATTENTION : fin de ligne différente entre Unix (LF) et Windows (CRLF) !!
        // Sans cela le \r reste collé au dernier champ et l'attente devient illisible.
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        // ligne vide
        if (line.size() < 3)
            continue;

        // Commentaire ou espace au début de la ligne
        aux = line.substr(0,1);
        if (aux == "/" || aux == " " || aux == "#")
            continue;

        // Extraction des éléments de la ligne
        //  0= position
        //  1= move
        //  2= score
        liste1 = split(line, tag);

        // Ligne malformée : sans ce garde-fou, liste1[2] déborde et segfault
        if (liste1.size() < 3)
        {
            printf("ligne mal formée (%zu champs au lieu de 3) : %s\n",
                   liste1.size(), line.c_str());
            continue;
        }

        fen   = liste1[0];                  // position fen
        aux   = liste1[1];

        strm  = aux.substr(1, aux.size());

        // Attente symbolique : « B - P », « max(0, R - N) », ou un simple entier
        if (SeeExpr(liste1[2]).parse(score) == false)
        {
            printf("attente illisible : « %s » [%s]\n", liste1[2].c_str(), fen.c_str());
            total_tests++;
            failed_tests_sign++;
            failed_tests_exact++;
            continue;
        }

        board.initialisation();
        board.set_fen(fen, false);

        if (board.turn() == WHITE)
            board.legal_moves<WHITE, MoveGenType::ALL>(ml);
        else
            board.legal_moves<BLACK, MoveGenType::ALL>(ml);

        move = 0;

        for (size_t i=0; i<ml.count; i++)
        {
            MOVE m = ml.mlmoves[i].move;

            std::string str1 = Move::show(m, 1);
            std::string str2 = Move::show(m, 2);
            std::string str3 = Move::show(m, 3);
            std::string str4 = Move::show(m, 4);

            if (strm==str1 || strm==str2 || strm==str3 || strm==str4)
            {
                move = m;
                break;
            }
        }

        // Doublon ? La clé est (position, coup résolu) : deux notations du même
        // coup sur la même position donnent la même clé.
        if (move && seen_tests.insert(fen + " | " + std::to_string(move)).second == false)
        {
            total_dup++;
            printf("DOUBLON ignoré : (%s) déjà teste sur [%s]\n", strm.c_str(), fen.c_str());
            continue;
        }

        numero++;
        printf("%2d : ", numero);

        if (move)
        {
            bool v = board.fast_see(move, 0);
            //    int  s = board.see(move);

            //    int  s = board.see(move);

            //        printf("%2d : %s : (%s) see=%d sees=%d score=%d \n", numero, fen.c_str(), strm.c_str(), v, s, score);

            if (Move::is_capturing(move))
                printf("C");
            if (Move::is_promoting(move))
                printf("P");
            if (Move::is_enpassant(move))
                printf("E");
            if (Move::is_castling(move))
                printf("K");

            if ((v==true && score>=0) || (v==false && score<0))
            {
                printf(" : OK ");
                passed_tests_sign++;
            }
            else
            {
                printf(" : %s : (%s) seeB=%d score=%d ", fen.c_str(), strm.c_str(), v, score);
                failed_tests_sign++;
            }

            // Test de la valeur exacte : le seuil "score" doit passer, "score+1" échouer.
            // Le test de signe ci-dessus n'appelle fast_see qu'avec un seuil nul,
            // ce qui laisse promotion et prise en passant hors d'atteinte.
            bool ok_inf = board.fast_see(move, score);
            bool ok_sup = board.fast_see(move, score+1);

            if (ok_inf == true && ok_sup == false)
            {
                printf(" : OK ");
                passed_tests_exact++;
            }
            else
            {
                printf(" : EXACT : (%s) see>=%d:%d see>=%d:%d ", strm.c_str(), score, ok_inf, score+1, ok_sup);
                failed_tests_exact++;
            }
            printf("\n");

            total_tests++;
        }
        else
        {
            // Coup introuvable : compté comme un échec des deux tests, sinon la ligne
            // disparaît du bilan et le seul indice est ce message noyé dans la sortie.
            total_tests++;
            failed_tests_sign++;
            failed_tests_exact++;

            printf("coup non trouvé %s \n", strm.c_str());
            printf("%s \n", fen.c_str());
            for (size_t i=0; i<ml.count; i++)
            {
                MOVE m = ml.mlmoves[i].move;

                std::string str1 = Move::show(m, 1);
                std::string str2 = Move::show(m, 2);
                std::string str3 = Move::show(m, 3);

                printf("(%s) : (%s) (%s) (%s) \n", strm.c_str(), str1.c_str(), str2.c_str(), str3.c_str());
            }
        }
    } // boucle position

    file.close();

    printf("# Passed sign  %10d\n",     passed_tests_sign);
    printf("# Passed exact %10d\n",     passed_tests_exact);
    printf("# Failed sign  %10d\n",     failed_tests_sign);
    printf("# Failed exact %10d\n",     failed_tests_exact);
    printf("# Total        %10d\n",     total_tests);

    if (total_dup > 0)
        std::cout << total_dup << " doublon(s) (position, coup) détecté(s) et ignoré(s) ; "
                  << seen_tests.size() << " couples uniques testés." << std::endl;
    else
        std::cout << "Aucun doublon (" << seen_tests.size()
                  << " couples (position, coup) uniques)." << std::endl;

    std::cout << "********************" << std::endl;

}

template void test_perft<true>(const std::string& str, const std::string& m_fen, int depth);
template void test_perft<false>(const std::string& str, const std::string& m_fen, int depth);

