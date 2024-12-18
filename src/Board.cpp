#include <sstream>
#include "Board.h"

//=======================================
//! \brief  Constructeur
//---------------------------------------
Board::Board()
{
    StatusHistory.reserve(MAX_HISTO);
    reset();
}

//=======================================
//! \brief  Constructeur
//---------------------------------------
Board::Board(const std::string& fen)
{
    StatusHistory.reserve(MAX_HISTO);
    reset();

    set_fen<false>(fen, false);
}

//==========================================
//! \brief  Initialisation de l'échiquier
//------------------------------------------
void Board::reset() noexcept
{
    colorPiecesBB.fill(0ULL);
    typePiecesBB.fill(0ULL);
    pieceOn.fill(NO_PIECE);
    x_king.fill(NO_SQUARE);
    side_to_move = Color::WHITE;
    avoid_moves.clear();
    best_moves.clear();
    StatusHistory.clear();
    nnue.reset();
}

//==========================================
//! \brief  Evaluation de la position
//! \return Evaluation statique de la position
//! du point de vue du camp au trait.
//------------------------------------------
[[nodiscard]] int Board::evaluate()
{
    if (side_to_move == WHITE)
        return nnue.evaluate<WHITE>();
    else
        return nnue.evaluate<BLACK>();

    //note : certains codes modifient la valeur retournée par nnue
    //       soit avec une "phase", soit avec une valeur "random".

}

//===================================================
//! \brief  Affichage de l'échiquier
//---------------------------------------------------
std::string Board::display() const noexcept
{
    std::stringstream ss;

    int sq;
    U64 hh = get_hash();
    U64 ph = get_pawn_hash();
    Bitboard bb;

    ss << "\n\n";

    for (int r=7; r>=0; r--)
    {
        ss << "  |---|---|---|---|---|---|---|---|-\n";
        ss << "  |";

        for (int f=0; f<8; f++)
        {
            sq = SQ::square(f, r);
            bb = SQ::square_BB(sq);


            if (occupancy_cp<Color::WHITE, PAWN>() & bb) {
                ss << " P |";
            } else if (occupancy_cp<Color::WHITE, KNIGHT>() & bb) {
                ss << " N |";
            } else if (occupancy_cp<Color::WHITE, BISHOP>() & bb) {
                ss << " B |";
            } else if (occupancy_cp<Color::WHITE, ROOK>() & bb) {
                ss << " R |";
            } else if (occupancy_cp<Color::WHITE, QUEEN>() & bb) {
                ss << " Q |";
            } else if (occupancy_cp<Color::WHITE, KING>() & bb) {
                ss << " K |";
            } else if (occupancy_cp<Color::BLACK, PAWN>() & bb) {
                ss << " p |";
            } else if (occupancy_cp<Color::BLACK, KNIGHT>() & bb) {
                ss << " n |";
            } else if (occupancy_cp<Color::BLACK, BISHOP>() & bb) {
                ss << " b |";
            } else if (occupancy_cp<Color::BLACK, ROOK>() & bb) {
                ss << " r |";
            } else if (occupancy_cp<Color::BLACK, QUEEN>() & bb) {
                ss << " q |";
            } else if (occupancy_cp<Color::BLACK, KING>() & bb) {
                ss << " k |";
            } else {
                ss << "   |";
            }
        }
        ss << " " << r+1 << "\n";
    }
    ss << "  |---|---|---|---|---|---|---|---|-\n";
    ss << "    a   b   c   d   e   f   g   h\n\n";

    ss << "Castling : ";
    ss << (can_castle<WHITE, CastleSide::KING_SIDE>()  ? "K" : "");
    ss << (can_castle<WHITE, CastleSide::QUEEN_SIDE>() ? "Q" : "");
    ss << (can_castle<BLACK, CastleSide::KING_SIDE>()  ? "k" : "");
    ss << (can_castle<BLACK, CastleSide::QUEEN_SIDE>() ? "q" : "");
    ss << '\n';
    if (get_ep_square() == NO_SQUARE) {
        ss << "EP       : -\n";
    } else {
        ss << "EP       : " << square_name[get_ep_square()] << '\n';
    }
    ss <<     "Turn     : " << side_name[turn()] << '\n';
    ss <<     "InCheck  : " << bool_name[is_in_check()] << "\n";
    ss <<     "Hash     : " << hh << "\n";
    ss <<     "PawnHash : " << ph << "\n";
    ss <<     "Fen      : " << get_fen() << "\n";

    return(ss.str());
}


//=============================================================
//! \brief  Calcule la valeur du hash
//-------------------------------------------------------------
void Board::calculate_hash(U64& khash, U64& phash) const
{
    khash = 0ULL;
    phash = 0ULL;
    Bitboard bb;

    // Turn
    if (turn() == Color::BLACK) {
        khash ^= side_key;
    }

    // Pieces
    bb = occupancy_cp<WHITE, PAWN>();
    while (bb) {
        int sq = BB::pop_lsb(bb);
        khash ^= piece_key[WHITE][PAWN][sq];
        phash ^= piece_key[WHITE][PAWN][sq];
    }
    bb = occupancy_cp<WHITE, KNIGHT>();
    while (bb) {
        int sq = BB::pop_lsb(bb);
        khash ^= piece_key[WHITE][KNIGHT][sq];
    }
    bb = occupancy_cp<WHITE, BISHOP>();
    while (bb) {
        int sq = BB::pop_lsb(bb);
        khash ^= piece_key[WHITE][BISHOP][sq];
    }
    bb = occupancy_cp<WHITE, ROOK>();
    while (bb) {
        int sq = BB::pop_lsb(bb);
        khash ^= piece_key[WHITE][ROOK][sq];
    }
    bb = occupancy_cp<WHITE, QUEEN>();
    while (bb) {
        int sq = BB::pop_lsb(bb);
        khash ^= piece_key[WHITE][QUEEN][sq];
    }
    bb = occupancy_cp<WHITE, KING>();
    while (bb) {
        int sq = BB::pop_lsb(bb);
        khash ^= piece_key[WHITE][KING][sq];
    }
    bb = occupancy_cp<BLACK, PAWN>();
    while (bb) {
        int sq = BB::pop_lsb(bb);
        khash ^= piece_key[BLACK][PAWN][sq];
        phash ^= piece_key[BLACK][PAWN][sq];
    }
    bb = occupancy_cp<BLACK, KNIGHT>();
    while (bb) {
        int sq = BB::pop_lsb(bb);
        khash ^= piece_key[BLACK][KNIGHT][sq];
    }
    bb = occupancy_cp<BLACK, BISHOP>();
    while (bb) {
        int sq = BB::pop_lsb(bb);
        khash ^= piece_key[BLACK][BISHOP][sq];
    }
    bb = occupancy_cp<BLACK, ROOK>();
    while (bb) {
        int sq = BB::pop_lsb(bb);
        khash ^= piece_key[BLACK][ROOK][sq];
    }
    bb = occupancy_cp<BLACK, QUEEN>();
    while (bb) {
        int sq = BB::pop_lsb(bb);
        khash ^= piece_key[BLACK][QUEEN][sq];
    }
    bb = occupancy_cp<BLACK, KING>();
    while (bb) {
        int sq = BB::pop_lsb(bb);
        khash ^= piece_key[BLACK][KING][sq];
    }

    // Castling
    khash ^= castle_key[get_status().castling];

    // EP
    if (get_status().ep_square != NO_SQUARE) {
        khash ^= ep_key[get_status().ep_square];
    }

}


