#include "Board.h"
#include "Attacks.h"

//===========================================================================
//! \brief  Calcule les pièces donnant échec, et les pièces clouées
//---------------------------------------------------------------------------
template <Color US>
void Board::calculate_checkers_pinned() noexcept
{
    constexpr Color THEM    = ~US;
    const int      K        = king_square<US>();
    const Bitboard enemyBB  = colorPiecesBB[THEM];
    const Bitboard usBB     = colorPiecesBB[US];

    const Bitboard their_diag_sliders = diagonal_sliders<THEM>();
    const Bitboard their_orth_sliders = orthogonal_sliders<THEM>();

    //  Calcul des pièces clouées, et des échecs
    //  algorithme de Surge

    int s;
    Bitboard b1;

    //Checkers of each piece type are identified by:
    //1. Projecting attacks FROM the king square
    //2. Intersecting this bitboard with the enemy bitboard of that piece type
    get_status().checkers = (Attacks::knight_moves(K)     & occupancy_cp<THEM, KNIGHT>())
             | (Attacks::pawn_attacks<US>(K) & occupancy_cp<THEM, PAWN>());

    //Here, we identify slider checkers and pinners simultaneously, and candidates for such pinners
    //and checkers are represented by the bitboard <candidates>
    Bitboard candidates = (Attacks::rook_moves(K, enemyBB)   & their_orth_sliders) |
                          (Attacks::bishop_moves(K, enemyBB) & their_diag_sliders);

    get_status().pinned = 0ULL;
    while (candidates)
    {
        s  = BB::pop_lsb(candidates);
        b1 = squares_between(K, s) & usBB;

        //Do the squares in between the enemy slider and our king contain any of our pieces?
        //If not, add the slider to the checker bitboard
        if (b1 == 0)
            get_status().checkers |= SQ::square_BB(s);

        //If there is only one of our pieces between them, add our piece to the pinned bitboard
        else if ((b1 & (b1 - 1)) == 0)
            get_status().pinned |= b1;
    }
}

//! \brief Retourne le Bitboard des cases attaquées
template <Color C>
[[nodiscard]] Bitboard Board::squares_attacked() const noexcept {
    Bitboard mask = 0ULL;

    // Pawns
    if constexpr (C == Color::WHITE) {
        const auto pawns = occupancy_cp<C, PAWN>();
        mask |= BB::north_east(pawns);
        mask |= BB::north_west(pawns);
    } else {
        const auto pawns = occupancy_cp<C, PAWN>();
        mask |= BB::south_east(pawns);
        mask |= BB::south_west(pawns);
    }

    // Knights
    Bitboard bb = occupancy_cp<C, KNIGHT>();
    while (bb) {
        int fr = BB::pop_lsb(bb);
        mask |= Attacks::knight_moves(fr);
    }

    // Bishops
    bb = occupancy_cp<C, BISHOP>();
    while (bb) {
        int fr = BB::pop_lsb(bb);
        mask |= Attacks::bishop_moves(fr, ~occupancy_none());
    }

    // Rooks
    bb = occupancy_cp<C, ROOK>();
    while (bb) {
        int fr = BB::pop_lsb(bb);
        mask |= Attacks::rook_moves(fr, ~occupancy_none());
    }

    // Queens
    bb = occupancy_cp<C, QUEEN>();
    while (bb) {
        int fr = BB::pop_lsb(bb);
        mask |= Attacks::queen_moves(fr, ~occupancy_none());
    }

    // King
    mask |= Attacks::king_moves(king_square<C>());

    return mask;
}

//===================================================================
//! \brief  Retourne le bitboard des attaques à la découverte
//! sur la case donnée
//-------------------------------------------------------------------
template <Color C>
[[nodiscard]] Bitboard Board::discoveredAttacks(const int sq) const noexcept
{
    Bitboard enemy      = colorPiecesBB[~C];
    Bitboard occupiedBB = occupancy_all();

    Bitboard rAttacks = Attacks::rook_moves(sq, occupiedBB);
    Bitboard bAttacks = Attacks::bishop_moves(sq, occupiedBB);

    Bitboard rooks   = (enemy & typePiecesBB[ROOK]) & ~rAttacks;
    Bitboard bishops = (enemy & typePiecesBB[BISHOP]) & ~bAttacks;

    return (  rooks &   Attacks::rook_moves(sq, occupiedBB & ~rAttacks))
           | (bishops & Attacks::bishop_moves(sq, occupiedBB & ~bAttacks));
}

// Explicit instantiations.
template Bitboard Board::squares_attacked<WHITE>() const noexcept ;
template Bitboard Board::squares_attacked<BLACK>() const noexcept ;

template void Board::calculate_checkers_pinned<WHITE>() noexcept;
template void Board::calculate_checkers_pinned<BLACK>() noexcept;

template Bitboard Board::discoveredAttacks<WHITE>(const int sq) const noexcept;
template Bitboard Board::discoveredAttacks<BLACK>(const int sq) const noexcept;
