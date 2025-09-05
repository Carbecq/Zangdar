#include <cassert>
#include <algorithm>
#include <sstream>
#include <string>
#include <iomanip>
#include "Board.h"

//    rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1
//                                                1 2    3 4 5
//
//      1) w       white to move
//      2) KQkq    roques possibles, ou '-'
//      3) -       case en passant
//      4) 0       demi-coups pour la règle des 50 coups   : halfmove_counter
//      5) 1       nombre de coups de la partie            : fullmove_counter

// The halfmove clock specifies a decimal number of half moves with respect to the 50 move draw rule.
// It is reset to zero after a capture or a pawn move and incremented otherwise.

// The number of the full moves in a game.
// It starts at 1, and is incremented after each Black's move

// EPD notation : extension de FEN

//    rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - <opération>;
//                                                1 2    3
//
//      1) w       white to move
//      2) KQkq    roques possibles, ou '-'
//      3) -       case en passant
//      4) ...     opération, par exemple : bm Re6; id WAC10";

//-----------------------------------------------------
//! \brief Initialisation depuis une position FEN
//-----------------------------------------------------
template <bool Update_NNUE>
void Board::set_fen(const std::string &fen, bool logTactics) noexcept
{
    assert(fen.length() > 0);

    if (fen == "startpos")
    {
        set_fen<Update_NNUE>(START_FEN, logTactics);
        return;
    }

    //-------------------------------------
    // Reset the board
    // >>> doit être fait en dehors de set_fen

    //-------------------------------------

    // ajout d'un élément, il en faut au moins 1
    // il ne faut pas re-initialiser StatusHistory

    /* position fen r2qk1nr/ppp2ppp/2nb4/4p3/2PpP3/5N1P/PPP2PP1/RNBQK2R b KQkq - 0 8 moves g8f6 b1d2 e8g8 a2a3 a7a5 b2b3 f6d7 e1g1 d7c5 d1e2 f8e8
        i=0  move=a1-a1  50=0
        i=1  move=Ng8-f6 50=1
        i=2  move=Nb1-d2 50=2
        i=3  move=Ke8-g8 50=3
        i=4  move=Pa2-a3 50=0
        i=5  move=Pa7-a5 50=0
        i=6  move=Pb2-b3 50=0
        i=7  move=Nf6-d7 50=1
        i=8  move=Ke1-g1 50=2
        i=9  move=Nd7-c5 50=3
        i=10 move=Qd1-e2 50=4
        i=11 move=Rf8-e8 50=5
     */

    StatusHistory.push_back(Status{});

    // est-ce une notation FEN ou EPD ?
    bool epd = false;
    int count = std::count(fen.begin(), fen.end(), ';');
    if (count > 0)
        epd = true;

    //-------------------------------------
    std::stringstream ss{fen};
    std::string word;

    ss >> word;
    int i = 56;
    for (const auto &c : word) {
        switch (c) {
        case 'P':
            add_piece(i, Color::WHITE, Piece::WHITE_PAWN);
            i++;
            break;
        case 'p':
            add_piece(i, Color::BLACK, Piece::BLACK_PAWN);
            i++;
            break;
        case 'N':
            add_piece(i, Color::WHITE, Piece::WHITE_KNIGHT);
            i++;
            break;
        case 'n':
            add_piece(i, Color::BLACK, Piece::BLACK_KNIGHT);
            i++;
            break;
        case 'B':
            add_piece(i, Color::WHITE, Piece::WHITE_BISHOP);
            i++;
            break;
        case 'b':
            add_piece(i, Color::BLACK, Piece::BLACK_BISHOP);
            i++;
            break;
        case 'R':
            add_piece(i, Color::WHITE, Piece::WHITE_ROOK);
            i++;
            break;
        case 'r':
            add_piece(i, Color::BLACK, Piece::BLACK_ROOK);
            i++;
            break;
        case 'Q':
            add_piece(i, Color::WHITE, Piece::WHITE_QUEEN);
            i++;
            break;
        case 'q':
            add_piece(i, Color::BLACK, Piece::BLACK_QUEEN);
            i++;
            break;
        case 'K':
            add_piece(i, Color::WHITE, Piece::WHITE_KING);
            king_square[Color::WHITE] = i;
            i++;
            break;
        case 'k':
            add_piece(i, Color::BLACK, Piece::BLACK_KING);
            king_square[Color::BLACK] = i;
            i++;
            break;
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
            i += c - '1' + 1;
            break;
        case '/':
            i -= 16;
            break;
        default:
            break;
        }
    }

    //----------------------------------------------------------
    // Side to move
    ss >> word;
    if (word == "w") {
        side_to_move = Color::WHITE;
    } else {
        side_to_move = Color::BLACK;
    }

    //----------------------------------------------------------
    // Droit au roque
    ss >> word;

    for (auto it = word.begin(); it != word.end(); ++it)
    {
        switch(*it)
        {
        case 'K':
            get_status().castling |= CASTLE_WK;
            break;
        case 'Q':
            get_status().castling |= CASTLE_WQ;
            break;
        case 'k':
            get_status().castling |= CASTLE_BK;
            break;
        case 'q':
            get_status().castling |= CASTLE_BQ;
            break;
        case '-':
            break;
        default:
            break;
        }
    }

    //----------------------------------------------------------
    // prise en passant

    std::string ep;
    ss >> ep;
    if (ep != "-")
    {
        char file = ep.at(0);
        char rank = ep.at(1);
        int s = (rank - '1') * 8 + file - 'a';
        assert(s>=A1 && s<=H8);
        get_status().ep_square = s;
    }

    //-----------------------------------------
    // la lecture dépend si on a une notation EPD ou FEN

    if (epd == true)
    {
        // iss : bm Rd6; id "WAC12";
        //       am Rd6; bm Rb6 Rg5+; id "WAC.274";
        //       bm Bg4 Re2; c0 "Bg4 wins, but Re2 is far better."; id "WAC.252";

        // best move
        std::string op, auxi;
        size_t p;

        for (int n=0; n<count; n++)
        {
            ss >> op;

            if (op == "bm") // Best Move
            {
                while(true)
                {
                    ss >> auxi;
                    p = auxi.find(';');             // indique la fin du champ "bm"
                    if (p == std::string::npos)     // pas trouvé
                    {
                        best_moves.push_back(auxi);
                    }
                    else
                    {
                        std::string bm = auxi.substr(0, p);
                        best_moves.push_back(bm);
                        break;
                    }
                }
            }
            else if (op == "am")    // Avoid Move
            {
                while(true)
                {
                    ss >> auxi;
                    p = auxi.find(';');             // indique la fin du champ "am"
                    if (p == std::string::npos)     // pas trouvé
                    {
                        avoid_moves.push_back(auxi);
                    }
                    else
                    {
                        std::string am = auxi.substr(0, p);
                        avoid_moves.push_back(am);
                        break;
                    }
                }
            }
            else if (op == "id")    // identifiant
            {
                std::string total;
                while(true)
                {
                    ss >> auxi;
                    p = auxi.find(';');             // indique la fin du champ "am"
                    if (p == std::string::npos)     // pas trouvé
                    {
                        total += auxi;
                        total += " ";
                    }
                    else
                    {
                        total += auxi.substr(0, p);;
                        std::string ident = total; //.substr(0, p);
                        if (logTactics)
                            std::cout << std::setw(30) << ident << " : ";
                        break;
                    }
                }
            }
            else if (op == "c0")    // commentaire
            {
                while(true)
                {
                    ss >> auxi;
                    p = auxi.find(';');             // indique la fin du champ "am"
                    if (p == std::string::npos)     // pas trouvé
                    {
                    }
                    else
                    {
                        break;
                    }
                }
            }
        }
    }
    else
    {
        // Halfmove clock
        // The halfmove clock specifies a decimal number of half moves with respect to the 50 move draw rule.
        // It is reset to zero after a capture or a pawn move and incremented otherwise.
        ss >> get_status().fiftymove_counter;

        // Fullmove clock
        // The number of the full moves in a game. It starts at 1, and is incremented after each Black's move.
        ss >> get_status().fullmove_counter;

        // Move count: ignore and use zero, as we count since root
    }

    //-----------------------------------------

    // pièces attaquant le roi
    (side_to_move == WHITE) ? calculate_checkers_pinned<WHITE>() : calculate_checkers_pinned<BLACK>();

    // Calculate hash
    U64 key;
    U64 pawn_key;
    U64 mat_key[2];
    calculate_hash(key, pawn_key, mat_key);
    get_status().key      = key;
    get_status().pawn_key = pawn_key;
    get_status().mat_key[WHITE] = mat_key[WHITE];
    get_status().mat_key[BLACK] = mat_key[BLACK];

    //--------------------------------------------
    if constexpr (Update_NNUE)
            set_network();

    //   std::cout << display() << std::endl;
}

//=========================================================================
//  Lecture d'une position fen, mais on va inverser l'Ã©chiquier
//  Ceci permet de tester l'Ã©valuation, qui doit Ãªtre symÃ©trique.
//-------------------------------------------------------------------------
void Board::mirror_fen(const std::string& fen, bool logTactics)
{
    assert(fen.length() > 0);

    if (fen == "startpos")
    {
        mirror_fen(START_FEN, logTactics);
        return;
    }

    // nnue.reset();

    // est-ce une notation FEN ou EPD ?
    bool epd = false;
    std::size_t found = fen.find(';');
    if (found != std::string::npos)
        epd = true;

    //-------------------------------------

    // Reset the board
    avoid_moves.clear();
    best_moves.clear();

    // Il faut avoir au moins un élément
    StatusHistory.push_back(Status{});

    //-------------------------------------

    std::stringstream ss(fen);
    std::string word;

    // Parse board positions
    ss >> word;
    int i = 56;
    for (const auto &c : word) {
        switch (c) {
        case 'P':
            add_piece(SQ::mirrorVertically(i), ~Color::WHITE, Piece::WHITE_PAWN);
            i++;
            break;
        case 'p':
            add_piece(SQ::mirrorVertically(i), ~Color::BLACK, Piece::WHITE_PAWN);
            i++;
            break;
        case 'N':
            add_piece(SQ::mirrorVertically(i), ~Color::WHITE, Piece::WHITE_KNIGHT);
            i++;
            break;
        case 'n':
            add_piece(SQ::mirrorVertically(i), ~Color::BLACK, Piece::WHITE_KNIGHT);
            i++;
            break;
        case 'B':
            add_piece(SQ::mirrorVertically(i), ~Color::WHITE, Piece::WHITE_BISHOP);
            i++;
            break;
        case 'b':
            add_piece(SQ::mirrorVertically(i), ~Color::BLACK, Piece::WHITE_BISHOP);
            i++;
            break;
        case 'R':
            add_piece(SQ::mirrorVertically(i), ~Color::WHITE, Piece::WHITE_ROOK);
            i++;
            break;
        case 'r':
            add_piece(SQ::mirrorVertically(i), ~Color::BLACK, Piece::WHITE_ROOK);
            i++;
            break;
        case 'Q':
            add_piece(SQ::mirrorVertically(i), ~Color::WHITE, Piece::WHITE_QUEEN);
            i++;
            break;
        case 'q':
            add_piece(SQ::mirrorVertically(i), ~Color::BLACK, Piece::WHITE_QUEEN);
            i++;
            break;
        case 'K':
            add_piece(SQ::mirrorVertically(i), ~Color::WHITE, Piece::WHITE_KING);
            king_square[~Color::WHITE] = SQ::mirrorVertically(i);
            i++;
            break;
        case 'k':
            add_piece(SQ::mirrorVertically(i), ~Color::BLACK, Piece::WHITE_KING);
            king_square[~Color::BLACK] = SQ::mirrorVertically(i);
            i++;
            break;
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
            i += c - '1' + 1;
            break;
        case '/':
            i -= 16;
            break;
        default:
            break;
        }
    }

    //----------------------------------------------------------
    // Set the side to move
    ss >> word;
    if (word == "w") {
        side_to_move = ~Color::WHITE;
    } else {
        side_to_move = ~Color::BLACK;
    }


    //----------------------------------------------------------
    // droit au roque
    ss >> word;

    for (auto it = word.begin(); it != word.end(); ++it)
    {
        switch(*it)
        {
        case 'K':
            get_status().castling |= CASTLE_BK;
            break;
        case 'Q':
            get_status().castling |= CASTLE_BQ;
            break;
        case 'k':
            get_status().castling |= CASTLE_WK;
            break;
        case 'q':
            get_status().castling |= CASTLE_WQ;
            break;
        case '-':
            break;
        default:
            break;
        }
    }


    //----------------------------------------------------------
    // prise en passant
    std::string ep;
    ss >> ep;
    if (ep != "-")
    {
        char file = ep.at(0);
        char rank = ep.at(1);
        int s = (rank - '1') * 8 + file - 'a';
        assert(s>=A1 && s<=H8);
        get_status().ep_square = SQ::mirrorVertically(s);
    }

    //-----------------------------------------
    // la lecture dépend si on a une notation EPD ou FEN

    if (epd == true)
    {
        // iss : bm Rd6; id "WAC12";

        // best move
        std::string op, auxi;
        size_t p;

        ss >> op;
        if (op == "bm") // Best Move
        {
            // on peut avoir plusieurs meilleurs coups !
            best_moves.clear();
            while(true)
            {
                ss >> auxi;
                p = auxi.find(';');     // indique la fin du champ "bm"
                if (p == std::string::npos)  // pas trouvé
                {
                    best_moves.push_back(auxi);
                }
                else
                {
                    std::string bm = auxi.substr(0, p);
                    best_moves.push_back(bm);
                    break;
                }
            }
        }
        else if (op == "am")    // Avoid Move
        {
            avoid_moves.clear();
            while(true)
            {
                ss >> auxi;
                p = auxi.find(';');     // indique la fin du champ "am"
                if (p == std::string::npos)  // pas trouvé
                {
                    avoid_moves.push_back(auxi);
                }
                else
                {
                    std::string am = auxi.substr(0, p);
                    avoid_moves.push_back(am);
                    break;
                }
            }
        }

        // identifiant
        ss >> op;
        if (op == "id")
        {
            ss >> auxi;
            p = auxi.find(';');
            std::string ident = auxi.substr(0, p);

            if (logTactics)
                std::cout << ident << " : ";
        }
    }
    else
    {
        // Halfmove clock
        // The halfmove clock specifies a decimal number of half moves with respect to the 50 move draw rule.
        // It is reset to zero after a capture or a pawn move and incremented otherwise.
        ss >> get_status().fiftymove_counter;

        // Fullmove clock
        // The number of the full moves in a game. It starts at 1, and is incremented after each Black's move.
        ss >> get_status().fullmove_counter;

        // Move count: ignore and use zero, as we count since root
        // get_status().gamemove_counter = 0;
    }

    //-----------------------------------------

    // pièces attaquant le roi
    (side_to_move == WHITE) ? calculate_checkers_pinned<WHITE>() : calculate_checkers_pinned<BLACK>();

    // Calculate hash
    U64 key;
    U64 pawn_key;
    U64 mat_key[2];
    calculate_hash(key, pawn_key, mat_key);
    get_status().key      = key;
    get_status().pawn_key = pawn_key;
    get_status().mat_key[WHITE] = mat_key[WHITE];
    get_status().mat_key[BLACK] = mat_key[BLACK];

    //   std::cout << display() << std::endl;
}

//============================================================
//! \brief  Retourne la chaine fen correspondant
//! à la position
//------------------------------------------------------------
[[nodiscard]] std::string Board::get_fen() const noexcept
{
    std::string fen;

    for (int y = 7; y >= 0; --y)
    {
        int num_empty = 0;

        for (int x = 0; x < 8; ++x)
        {
            const auto sq = SQ::square(x, y);
            const Piece piece = piece_at(sq);
            if (piece == Piece::NONE) {
                num_empty++;
            } else {
                // Add the number of empty squares so far
                if (num_empty > 0) {
                    fen += std::to_string(num_empty);
                }
                num_empty = 0;

                fen += pieceToChar(piece);
                // piece_symbol[color_on(sq)][piece];
            }
        }

        // Add the number of empty squares when we reach the end of the rank
        if (num_empty > 0) {
            fen += std::to_string(num_empty);
        }

        if (y > 0) {
            fen += "/";
        }
    }

    //------------------------------------------------------- side

    fen += (turn() == Color::WHITE) ? " w" : " b";

    //------------------------------------------------------- castling

    std::string part;
    if (can_castle<WHITE, CastleSide::KING_SIDE>())
        part += "K";
    if (can_castle<WHITE, CastleSide::QUEEN_SIDE>())
        part += "Q";
    if (can_castle<BLACK, CastleSide::KING_SIDE>())
        part += "k";
    if (can_castle<BLACK, CastleSide::QUEEN_SIDE>())
        part += "q";
    if (part.empty())
        part = "-";

    fen += " ";
    fen += part;

    //------------------------------------------------------- en-passant

    fen += " ";
    if (get_status().ep_square == SQUARE_NONE)
        fen += "-";
    else
        fen += square_name[get_status().ep_square];

    //------------------------------------------------------- fifty-move

    fen += " ";
    fen += std::to_string(get_fiftymove_counter());

    //------------------------------------------------------- full-move

    fen += " ";
    fen += std::to_string(get_fullmove_counter());

    return fen;
}

//===========================================================
//! \brief  met à jour le réseau à partir de la position
//-----------------------------------------------------------
void Board::set_network()
{
    nnue.reset();

    int wking = king_square[WHITE];
    int bking = king_square[BLACK];
    int square;

    Bitboard occupied = occupancy_all();
    while (occupied)
    {
        square = BB::pop_lsb(occupied);
        nnue.add(piece_square[square], square, wking, bking);
    }
}

//===========================================================
//! \brief  met à jour le réseau courant à partir de la position
//!         de plus, on ne s'intéresse qu'à l'accumulateur "US"
//! \param[in]  king    position du roi de couleur "US"
//-----------------------------------------------------------
template <Color US>
void Board::set_current_network(int king)
{
    nnue.reset_current<US>();

    // Bitboard bb;
    int square;

    Bitboard occupied = occupancy_all();
    while (occupied)
    {
        square = BB::pop_lsb(occupied);
        nnue.add<US>(piece_square[square], square, king);
    }
}

//-----------------------------------------------------
//! \brief Commande UCI : position
//!         Entrée d'une position
//!         et d'une série de coups
//-----------------------------------------------------
void Board::parse_position(std::istringstream &is)
{
    std::string token;

    token.clear();
    is >> token;

    bool logTactics = false;

    if (token == "startpos")
    {
        set_fen<true>(START_FEN, logTactics);
    }
    else
    {
        std::string fen;

        while (is >> token && token != "moves")
        {
            fen += token + " ";
        }
        set_fen<true>(fen, logTactics);
    }

    while (is >> token)
    {
        if (token == "moves")
            continue;
        if (side_to_move == WHITE)
            apply_token<WHITE>(token);
        else
            apply_token<BLACK>(token);
    }
}


//==========================================================================
//! \brief  recherche si le coup "token" est un coup légal,
//!         puis l'exécute
//--------------------------------------------------------------------------
template <Color C>
void Board::apply_token(const std::string& token) noexcept
{
    MoveList ml;
    legal_moves<C, MoveGenType::ALL>(ml);
#ifndef NDEBUG
    int nbr = 0;
#endif

    for (const auto &mlmove : ml.mlmoves)
    {
        if (Move::name(mlmove.move) == token)
        {
#ifndef NDEBUG
            nbr++;
#endif
            make_move<C, true>(mlmove.move);
            break;
        }
    }

#ifndef NDEBUG
    if (nbr == 0)
        printlog("---------------------------nbr 0\n");
    else if (nbr == 1)
        printlog("ok \n") ;
    else
        printlog("---------------------------nbr > 1 \n");
#endif

}

template void Board::set_fen<true>(const std::string &fen, bool logTactics) noexcept;
template void Board::set_fen<false>(const std::string &fen, bool logTactics) noexcept;

template void Board::set_current_network<WHITE>(int king);
template void Board::set_current_network<BLACK>(int king);
