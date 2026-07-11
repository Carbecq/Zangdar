#include "Board.h"
#include "Attacks.h"

//===========================================================================
//! \brief  Calcule les pièces donnant échec, et les pièces clouées
//---------------------------------------------------------------------------
template <Color US>
void Board::calculate_checkers_pinned() noexcept
{
    constexpr Color THEM    = ~US;
    const U32       K     = get_king_square<US>();
    const Bitboard enemyBB  = colorPiecesBB[THEM];
    const Bitboard usBB     = colorPiecesBB[US];

    const Bitboard their_diag_sliders = diagonal_sliders<THEM>();
    const Bitboard their_orth_sliders = orthogonal_sliders<THEM>();

    //  Calcul des pièces clouées, et des échecs
    //  algorithme de Surge

    U32 s;
    Bitboard b1;

    //Les pièces donnant échec, pour chaque type de pièce, sont identifiées en :
    //1. Projetant les attaques DEPUIS la case du roi
    //2. Croisant ce bitboard avec le bitboard ennemi de ce type de pièce
    get_status().checkers = (Attacks::knight_moves(K)     & occupancy_cp<THEM, PieceType::KNIGHT>())
            | (Attacks::pawn_attacks<US>(K) & occupancy_cp<THEM, PieceType::PAWN>());

    //Ici, on identifie simultanément les glisseurs donnant échec et les cloueurs ;
    //les candidats à ces deux rôles sont représentés par le bitboard <candidates>
    Bitboard candidates = (Attacks::rook_moves(K, enemyBB)   & their_orth_sliders) |
            (Attacks::bishop_moves(K, enemyBB) & their_diag_sliders);

    get_status().pinned = 0ULL;
    while (candidates)
    {
        s  = BB::pop_lsb(candidates);
        b1 = squares_between(K, s) & usBB;

        //Les cases entre le glisseur ennemi et notre roi contiennent-elles une de nos pièces ?
        //Si non, on ajoute le glisseur au bitboard des pièces donnant échec
        if (b1 == 0)
            get_status().checkers |= SQ::square_BB(s);

        //S'il n'y a qu'une seule de nos pièces entre les deux, on l'ajoute au bitboard des pièces clouées
        else if ((b1 & (b1 - 1)) == 0)
            get_status().pinned |= b1;
    }
}

//===========================================================================
//! \brief  Retourne le Bitboard des cases attaquées par la couleur C
//!
//! \return Bitboard des cases attaquées
//---------------------------------------------------------------------------
template <Color C>
[[nodiscard]] Bitboard Board::squares_attacked() const noexcept
{
    constexpr Color THEM = ~C;

    Bitboard mask = 0ULL;

    const Bitboard friendly = colorPiecesBB[C];
    const Bitboard occupied = colorPiecesBB[THEM] | friendly;

    Bitboard pawns   = friendly &  typePiecesBB[PieceType::PAWN];
    Bitboard knights = friendly &  typePiecesBB[PieceType::KNIGHT];
    Bitboard bishops = friendly & (typePiecesBB[PieceType::BISHOP] | typePiecesBB[PieceType::QUEEN]);
    Bitboard rooks   = friendly & (typePiecesBB[PieceType::ROOK]   | typePiecesBB[PieceType::QUEEN]);

    // Pions
    if constexpr (C == Color::WHITE) {
        mask |= BB::north_east(pawns);
        mask |= BB::north_west(pawns);
    } else {
        mask |= BB::south_east(pawns);
        mask |= BB::south_west(pawns);
    }

    // Cavaliers
    while (knights)
        mask |= Attacks::knight_moves(BB::pop_lsb(knights));

    // Fous et Dames
    while (bishops)
        mask |= Attacks::bishop_moves(BB::pop_lsb(bishops), occupied);

    // Tours et Dames
    while (rooks)
        mask |= Attacks::rook_moves(BB::pop_lsb(rooks), occupied);

    // Roi
    mask |= Attacks::king_moves(get_king_square<C>());

    return mask;
}

// Instanciations explicites.
template Bitboard Board::squares_attacked<WHITE>() const noexcept ;
template Bitboard Board::squares_attacked<BLACK>() const noexcept ;

//===========================================================================
//! \brief  Retourne le Bitboard des cases attaquées par C avec une occupancy explicite
//!         (utilisé pour les mouvements du roi, sans le roi dans l'occupancy)
//!
//! \param[in]  occ     occupancy explicite à utiliser pour les pièces glissantes
//!
//! \return Bitboard des cases attaquées
//---------------------------------------------------------------------------
template <Color C>
[[nodiscard]] Bitboard Board::squares_attacked(Bitboard occ) const noexcept
{
    Bitboard mask = 0ULL;

    const Bitboard friendly = colorPiecesBB[C];

    Bitboard pawns   = friendly &  typePiecesBB[PieceType::PAWN];
    Bitboard knights = friendly &  typePiecesBB[PieceType::KNIGHT];
    Bitboard bishops = friendly & (typePiecesBB[PieceType::BISHOP] | typePiecesBB[PieceType::QUEEN]);
    Bitboard rooks   = friendly & (typePiecesBB[PieceType::ROOK]   | typePiecesBB[PieceType::QUEEN]);

    // Pions
    if constexpr (C == Color::WHITE) {
        mask |= BB::north_east(pawns);
        mask |= BB::north_west(pawns);
    } else {
        mask |= BB::south_east(pawns);
        mask |= BB::south_west(pawns);
    }

    // Cavaliers
    while (knights)
        mask |= Attacks::knight_moves(BB::pop_lsb(knights));

    // Fous et Dames
    while (bishops)
        mask |= Attacks::bishop_moves(BB::pop_lsb(bishops), occ);

    // Tours et Dames
    while (rooks)
        mask |= Attacks::rook_moves(BB::pop_lsb(rooks), occ);

    // Roi
    mask |= Attacks::king_moves(get_king_square<C>());

    return mask;
}

template Bitboard Board::squares_attacked<WHITE>(Bitboard occ) const noexcept ;
template Bitboard Board::squares_attacked<BLACK>(Bitboard occ) const noexcept ;

template void Board::calculate_checkers_pinned<WHITE>() noexcept;
template void Board::calculate_checkers_pinned<BLACK>() noexcept;

