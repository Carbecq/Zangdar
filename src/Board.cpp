#include <sstream>
#include "zobrist.h"
#include "Board.h"
#include "Cuckoo.h"

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
    square_king.fill(SquareType::SQUARE_NONE);
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
        return nnue.evaluate<WHITE>(BB::count_bit(occupancy_all()));
    else
        return nnue.evaluate<BLACK>(BB::count_bit(occupancy_all()));

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
void Board::calculate_hash(U64& key, U64& pawn_key, U64 mat_key[N_COLORS]) const
{
    key            = 0ULL;
    pawn_key       = 0ULL;
    mat_key[WHITE] = 0ULL;
    mat_key[BLACK] = 0ULL;

    Bitboard bb;

    // Turn
    if (turn() == Color::BLACK) {
        key ^= side_key;
    }

    // Pieces Blanches

    bb = occupancy_cp<WHITE, PieceType::PAWN>();
    while (bb) {
        int sq = BB::pop_lsb(bb);
        key      ^= piece_key[static_cast<U32>(Piece::WHITE_PAWN)][sq];
        pawn_key ^= piece_key[static_cast<U32>(Piece::WHITE_PAWN)][sq];
    }
    bb = occupancy_cp<WHITE, PieceType::KNIGHT>();
    while (bb) {
        int sq = BB::pop_lsb(bb);
        key            ^= piece_key[static_cast<U32>(Piece::WHITE_KNIGHT)][sq];
        mat_key[WHITE] ^= piece_key[static_cast<U32>(Piece::WHITE_KNIGHT)][sq];
    }
    bb = occupancy_cp<WHITE, PieceType::BISHOP>();
    while (bb) {
        int sq = BB::pop_lsb(bb);
        key            ^= piece_key[static_cast<U32>(Piece::WHITE_BISHOP)][sq];
        mat_key[WHITE] ^= piece_key[static_cast<U32>(Piece::WHITE_BISHOP)][sq];
    }
    bb = occupancy_cp<WHITE, PieceType::ROOK>();
    while (bb) {
        int sq = BB::pop_lsb(bb);
        key            ^= piece_key[static_cast<U32>(Piece::WHITE_ROOK)][sq];
        mat_key[WHITE] ^= piece_key[static_cast<U32>(Piece::WHITE_ROOK)][sq];
    }
    bb = occupancy_cp<WHITE, PieceType::QUEEN>();
    while (bb) {
        int sq = BB::pop_lsb(bb);
        key            ^= piece_key[static_cast<U32>(Piece::WHITE_QUEEN)][sq];
        mat_key[WHITE] ^= piece_key[static_cast<U32>(Piece::WHITE_QUEEN)][sq];
    }
    bb = occupancy_cp<WHITE, PieceType::KING>();
    while (bb) {
        int sq = BB::pop_lsb(bb);
        key            ^= piece_key[static_cast<U32>(Piece::WHITE_KING)][sq];
        mat_key[WHITE] ^= piece_key[static_cast<U32>(Piece::WHITE_KING)][sq];
    }

    // Pieces Noires

    bb = occupancy_cp<BLACK, PieceType::PAWN>();
    while (bb) {
        int sq = BB::pop_lsb(bb);
        key      ^= piece_key[static_cast<U32>(Piece::BLACK_PAWN)][sq];
        pawn_key ^= piece_key[static_cast<U32>(Piece::BLACK_PAWN)][sq];
    }
    bb = occupancy_cp<BLACK, PieceType::KNIGHT>();
    while (bb) {
        int sq = BB::pop_lsb(bb);
        key            ^= piece_key[static_cast<U32>(Piece::BLACK_KNIGHT)][sq];
        mat_key[BLACK] ^= piece_key[static_cast<U32>(Piece::BLACK_KNIGHT)][sq];
    }
    bb = occupancy_cp<BLACK, PieceType::BISHOP>();
    while (bb) {
        int sq = BB::pop_lsb(bb);
        key            ^= piece_key[static_cast<U32>(Piece::BLACK_BISHOP)][sq];
        mat_key[BLACK] ^= piece_key[static_cast<U32>(Piece::BLACK_BISHOP)][sq];
    }
    bb = occupancy_cp<BLACK, PieceType::ROOK>();
    while (bb) {
        int sq = BB::pop_lsb(bb);
        key            ^= piece_key[static_cast<U32>(Piece::BLACK_ROOK)][sq];
        mat_key[BLACK] ^= piece_key[static_cast<U32>(Piece::BLACK_ROOK)][sq];
    }
    bb = occupancy_cp<BLACK, PieceType::QUEEN>();
    while (bb) {
        int sq = BB::pop_lsb(bb);
        key            ^= piece_key[static_cast<U32>(Piece::BLACK_QUEEN)][sq];
        mat_key[BLACK] ^= piece_key[static_cast<U32>(Piece::BLACK_QUEEN)][sq];
    }
    bb = occupancy_cp<BLACK, PieceType::KING>();
    while (bb) {
        int sq = BB::pop_lsb(bb);
        key            ^= piece_key[static_cast<U32>(Piece::BLACK_KING)][sq];
        mat_key[BLACK] ^= piece_key[static_cast<U32>(Piece::BLACK_KING)][sq];
    }

    // Castling
    key ^= castle_key[get_status().castling];

    // EP
    if (get_status().ep_square != SQUARE_NONE) {
        key ^= ep_key[get_status().ep_square];
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

    bb = occupancy_cp<WHITE, PieceType::KING>();
    int wking = BB::pop_lsb(bb);

    bb = occupancy_cp<BLACK, PieceType::KING>();
    int bking = BB::pop_lsb(bb);

    nn.add(Piece::WHITE_KING, wking, wking, bking);
    nn.add(Piece::BLACK_KING, bking, wking, bking);

    // Pieces Blanches

    bb = occupancy_cp<WHITE, PieceType::PAWN>();
    while (bb) {
        int sq = BB::pop_lsb(bb);
        nn.add(Piece::WHITE_PAWN, sq, wking, bking);
    }
    bb = occupancy_cp<WHITE, PieceType::KNIGHT>();
    while (bb) {
        int sq = BB::pop_lsb(bb);
        nn.add(Piece::WHITE_KNIGHT, sq, wking, bking);
    }
    bb = occupancy_cp<WHITE, PieceType::BISHOP>();
    while (bb) {
        int sq = BB::pop_lsb(bb);
        nn.add(Piece::WHITE_BISHOP, sq, wking, bking);
    }
    bb = occupancy_cp<WHITE, PieceType::ROOK>();
    while (bb) {
        int sq = BB::pop_lsb(bb);
        nn.add(Piece::WHITE_ROOK, sq, wking, bking);
    }
    bb = occupancy_cp<WHITE, PieceType::QUEEN>();
    while (bb) {
        int sq = BB::pop_lsb(bb);
        nn.add(Piece::WHITE_QUEEN, sq, wking, bking);
    }

    // Pieces Noires

    bb = occupancy_cp<BLACK, PieceType::PAWN>();
    while (bb) {
        int sq = BB::pop_lsb(bb);
        nn.add(Piece::BLACK_PAWN, sq, wking, bking);
    }
    bb = occupancy_cp<BLACK, PieceType::KNIGHT>();
    while (bb) {
        int sq = BB::pop_lsb(bb);
        nn.add(Piece::BLACK_KNIGHT, sq, wking, bking);
    }
    bb = occupancy_cp<BLACK, PieceType::BISHOP>();
    while (bb) {
        int sq = BB::pop_lsb(bb);
        nn.add(Piece::BLACK_BISHOP, sq, wking, bking);
    }
    bb = occupancy_cp<BLACK, PieceType::ROOK>();
    while (bb) {
        int sq = BB::pop_lsb(bb);
        nn.add(Piece::BLACK_ROOK, sq, wking, bking);
    }
    bb = occupancy_cp<BLACK, PieceType::QUEEN>();
    while (bb) {
        int sq = BB::pop_lsb(bb);
        nn.add(Piece::BLACK_QUEEN, sq, wking, bking);
    }

    eval = nn.evaluate<WHITE>(BB::count_bit(occupancy_all()));
}

// Upcoming repetition detection
// http://www.open-chess.org/viewtopic.php?f=5&t=2300
// Implemented originally in SF
// Paper accessible @ http://marcelk.net/2013-04-06/paper/upcoming-rep-v2.pdf

bool Board::upcoming_repetition(int ply) const
{
    // Adapted from Stockfish, Stormphrax

    const auto distance = std::min(get_fiftymove_counter(), static_cast<I32>(StatusHistory.size()));

    // Enough reversible moves played
    if (distance < 3)
        return false;

    U32 mk;
    int index = StatusHistory.size() - 1;

    const Bitboard occupied = occupancy_all();
    const U64 originalKey   = get_key();      // StatusHistory[StatusHistory.size() - 1].key

    /* StatusHistory : array of board status, for all moves, including the last move played
     */
    assert(originalKey == StatusHistory[index].key);

    for (I32 d = 3; d <= distance; d += 2)
    {
        U64 diff = originalKey ^ StatusHistory[index - d].key;

        if (   (mk = Cuckoo::h1(diff), Cuckoo::keys[mk] == diff)
               || (mk = Cuckoo::h2(diff), Cuckoo::keys[mk] == diff))
        {
            const MOVE move = Cuckoo::moves[mk];
            const int  from = Move::from(move);
            const int  dest = Move::dest(move);

            // Test if the squares between a and b are all empty (a and b themselves excluded)
            if (BB::empty(occupied & squares_between(from, dest)))
            {
                // repetition is after root, done
                if (ply > d)
                    return true;

                // For nodes before or at the root, check that the move is a
                // repetition rather than a move to the current position.
                // In the cuckoo table, both moves Rc1c5 and Rc5c1 are stored in
                // the same location, so we have to select which square to check.
                if (Move::color(piece_at( empty(from) ? dest : from)) != turn() )
                    continue;
            }
        }
    }

    return false;
}
