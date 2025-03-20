#include <sstream>
#include "zobrist.h"
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
    pieceBoard.fill(Piece::NONE);
    x_king.fill(SquareType::SQUARE_NONE);
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


            if (occupancy_cp<Color::WHITE, PieceType::PAWN>() & bb) {
                ss << " P |";
            } else if (occupancy_cp<Color::WHITE, PieceType::KNIGHT>() & bb) {
                ss << " N |";
            } else if (occupancy_cp<Color::WHITE, PieceType::BISHOP>() & bb) {
                ss << " B |";
            } else if (occupancy_cp<Color::WHITE, PieceType::ROOK>() & bb) {
                ss << " R |";
            } else if (occupancy_cp<Color::WHITE, PieceType::QUEEN>() & bb) {
                ss << " Q |";
            } else if (occupancy_cp<Color::WHITE, PieceType::KING>() & bb) {
                ss << " K |";
            } else if (occupancy_cp<Color::BLACK, PieceType::PAWN>() & bb) {
                ss << " p |";
            } else if (occupancy_cp<Color::BLACK, PieceType::KNIGHT>() & bb) {
                ss << " n |";
            } else if (occupancy_cp<Color::BLACK, PieceType::BISHOP>() & bb) {
                ss << " b |";
            } else if (occupancy_cp<Color::BLACK, PieceType::ROOK>() & bb) {
                ss << " r |";
            } else if (occupancy_cp<Color::BLACK, PieceType::QUEEN>() & bb) {
                ss << " q |";
            } else if (occupancy_cp<Color::BLACK, PieceType::KING>() & bb) {
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
    if (get_ep_square() == SQUARE_NONE) {
        ss << "EP       : -\n";
    } else {
        ss << "EP       : " << square_name[get_ep_square()] << '\n';
    }
    ss << "Turn         : " << side_name[turn()] << '\n';
    ss << "InCheck      : " << bool_name[is_in_check()] << "\n";
    ss << "Key          : " << std::hex << get_key() << "\n";
    ss << "Fen          : " << get_fen() << "\n";
    ss << "50 move      : " << std::dec << get_fiftymove_counter() << "\n";
    ss << "game counter : " << StatusHistory.size() - 1 << "\n";
    ss << "full move    : " << get_fullmove_counter() << "\n";

    // std::cout << std::hex << (get_key())
    //           << "  shift32= " << (get_key() >> 32)
    //           << "  cast32= " << (static_cast<U32>(get_key()))
    //           << "  autre= " << ((get_key()>>32) ^ (get_key()&0xffffffff))
    //           << std::dec << std::endl;

    return(ss.str());
}


//=============================================================
//! \brief  Calcule la valeur du hash
//-------------------------------------------------------------
void Board::calculate_hash(U64& khash) const
{
    khash = 0ULL;
    Bitboard bb;

    // Turn
    if (turn() == Color::BLACK) {
        khash ^= side_key;
    }

    // Pieces Blanches

    bb = occupancy_cp<WHITE, PieceType::PAWN>();
    while (bb) {
        int sq = BB::pop_lsb(bb);
        khash ^= piece_key[static_cast<U32>(Piece::WHITE_PAWN)][sq];
    }
    bb = occupancy_cp<WHITE, PieceType::KNIGHT>();
    while (bb) {
        int sq = BB::pop_lsb(bb);
        khash ^= piece_key[static_cast<U32>(Piece::WHITE_KNIGHT)][sq];
    }
    bb = occupancy_cp<WHITE, PieceType::BISHOP>();
    while (bb) {
        int sq = BB::pop_lsb(bb);
        khash ^= piece_key[static_cast<U32>(Piece::WHITE_BISHOP)][sq];
    }
    bb = occupancy_cp<WHITE, PieceType::ROOK>();
    while (bb) {
        int sq = BB::pop_lsb(bb);
        khash ^= piece_key[static_cast<U32>(Piece::WHITE_ROOK)][sq];
    }
    bb = occupancy_cp<WHITE, PieceType::QUEEN>();
    while (bb) {
        int sq = BB::pop_lsb(bb);
        khash ^= piece_key[static_cast<U32>(Piece::WHITE_QUEEN)][sq];
    }
    bb = occupancy_cp<WHITE, PieceType::KING>();
    while (bb) {
        int sq = BB::pop_lsb(bb);
        khash ^= piece_key[static_cast<U32>(Piece::WHITE_KING)][sq];
    }

    // Pieces Noires

    bb = occupancy_cp<BLACK, PieceType::PAWN>();
    while (bb) {
        int sq = BB::pop_lsb(bb);
        khash ^= piece_key[static_cast<U32>(Piece::BLACK_PAWN)][sq];
    }
    bb = occupancy_cp<BLACK, PieceType::KNIGHT>();
    while (bb) {
        int sq = BB::pop_lsb(bb);
        khash ^= piece_key[static_cast<U32>(Piece::BLACK_KNIGHT)][sq];
    }
    bb = occupancy_cp<BLACK, PieceType::BISHOP>();
    while (bb) {
        int sq = BB::pop_lsb(bb);
        khash ^= piece_key[static_cast<U32>(Piece::BLACK_BISHOP)][sq];
    }
    bb = occupancy_cp<BLACK, PieceType::ROOK>();
    while (bb) {
        int sq = BB::pop_lsb(bb);
        khash ^= piece_key[static_cast<U32>(Piece::BLACK_ROOK)][sq];
    }
    bb = occupancy_cp<BLACK, PieceType::QUEEN>();
    while (bb) {
        int sq = BB::pop_lsb(bb);
        khash ^= piece_key[static_cast<U32>(Piece::BLACK_QUEEN)][sq];
    }
    bb = occupancy_cp<BLACK, PieceType::KING>();
    while (bb) {
        int sq = BB::pop_lsb(bb);
        khash ^= piece_key[static_cast<U32>(Piece::BLACK_KING)][sq];
    }

    // Castling
    khash ^= castle_key[get_status().castling];

    // EP
    if (get_status().ep_square != SQUARE_NONE) {
        khash ^= ep_key[get_status().ep_square];
    }

}

//=============================================================
//! \brief  Calcule la valeur du réseau
//-------------------------------------------------------------
void Board::calculate_nnue(int &eval)
{
    eval = 0;

    Bitboard bb;

    NNUE nn{};
    nn.reserve_capacity();
    nn.reset();

    // Pieces Blanches

    bb = occupancy_cp<WHITE, PieceType::PAWN>();
    while (bb) {
        int sq = BB::pop_lsb(bb);
        nn.add(Piece::WHITE_PAWN, sq);
    }
    bb = occupancy_cp<WHITE, PieceType::KNIGHT>();
    while (bb) {
        int sq = BB::pop_lsb(bb);
        nn.add(Piece::WHITE_KNIGHT, sq);
    }
    bb = occupancy_cp<WHITE, PieceType::BISHOP>();
    while (bb) {
        int sq = BB::pop_lsb(bb);
        nn.add(Piece::WHITE_BISHOP, sq);
    }
    bb = occupancy_cp<WHITE, PieceType::ROOK>();
    while (bb) {
        int sq = BB::pop_lsb(bb);
        nn.add(Piece::WHITE_ROOK, sq);
    }
    bb = occupancy_cp<WHITE, PieceType::QUEEN>();
    while (bb) {
        int sq = BB::pop_lsb(bb);
        nn.add(Piece::WHITE_QUEEN, sq);
    }
    bb = occupancy_cp<WHITE, PieceType::KING>();
    while (bb) {
        int sq = BB::pop_lsb(bb);
        nn.add(Piece::WHITE_KING, sq);
    }

    // Pieces Noires

    bb = occupancy_cp<BLACK, PieceType::PAWN>();
    while (bb) {
        int sq = BB::pop_lsb(bb);
        nn.add(Piece::BLACK_PAWN, sq);
    }
    bb = occupancy_cp<BLACK, PieceType::KNIGHT>();
    while (bb) {
        int sq = BB::pop_lsb(bb);
        nn.add(Piece::BLACK_KNIGHT, sq);
    }
    bb = occupancy_cp<BLACK, PieceType::BISHOP>();
    while (bb) {
        int sq = BB::pop_lsb(bb);
        nn.add(Piece::BLACK_BISHOP, sq);
    }
    bb = occupancy_cp<BLACK, PieceType::ROOK>();
    while (bb) {
        int sq = BB::pop_lsb(bb);
        nn.add(Piece::BLACK_ROOK, sq);
    }
    bb = occupancy_cp<BLACK, PieceType::QUEEN>();
    while (bb) {
        int sq = BB::pop_lsb(bb);
        nn.add(Piece::BLACK_QUEEN, sq);
    }
    bb = occupancy_cp<BLACK, PieceType::KING>();
    while (bb) {
        int sq = BB::pop_lsb(bb);
        nn.add(Piece::BLACK_KING, sq);
    }

    eval = nn.evaluate<WHITE>();
}

