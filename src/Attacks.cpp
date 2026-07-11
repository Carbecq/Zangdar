#include "Attacks.h"
#include "Bitboard.h"

namespace Attacks {

Bitboard ROOK_ATTACKS  [N_SQUARES][4096]{};
Bitboard BISHOP_ATTACKS[N_SQUARES][ 512]{};


//======================================================
//! \brief  Définit les occupancies
//!
//! \param[in]  index           index de la variation d'occupancy à générer
//! \param[in]  bits_in_mask    nombre de bits pertinents dans le masque
//! \param[in]  attack_mask     masque des cases pertinentes (hors bord)
//!
//! \return Bitboard d'occupancy correspondant à l'index donné
//------------------------------------------------------
Bitboard set_occupancy(int index, int bits_in_mask, Bitboard attack_mask)
{
    Bitboard occupancy = 0;

    for (int count = 0; count < bits_in_mask; count++)
    {
        SQUARE sq = BB::pop_lsb(attack_mask);

        if (index & (1 << count))
            occupancy |= (1ULL << sq);
    }

    return occupancy;
}

//======================================================
//! \brief  Génère les attaques du fou à la volée
//!
//! \param[in]  sq      case de départ du fou
//! \param[in]  block   bitboard des cases occupées (bloquantes)
//!
//! \return Bitboard des cases attaquées par le fou
//------------------------------------------------------
Bitboard bishop_attacks_on_the_fly(SQUARE sq, Bitboard block)
{
    Bitboard attacks = 0;

    int sr = SQ::rank(sq);
    int sf = SQ::file(sq);

    for (int r = sr + 1, f = sf + 1; r <= 7 && f <= 7; r++, f++)
    {
        attacks |= (1ULL << (r * 8 + f));
        if (BB::get_bit(block, SQ::square(f, r)))
            break;
    }

    for (int r = sr - 1, f = sf + 1; r >= 0 && f <= 7; r--, f++) {
        attacks |= (1ULL << (r * 8 + f));
        if (BB::get_bit(block, SQ::square(f, r)))
            break;
    }

    for (int r = sr + 1, f = sf - 1; r <= 7 && f >= 0; r++, f--) {
        attacks |= (1ULL << (r * 8 + f));
        if (BB::get_bit(block, SQ::square(f, r)))
            break;
    }

    for (int r = sr - 1, f = sf - 1; r >= 0 && f >= 0; r--, f--) {
        attacks |= (1ULL << (r * 8 + f));
        if (BB::get_bit(block, SQ::square(f, r)))
            break;
    }

    return attacks;
}

//======================================================
//! \brief  Initialise la table d'attaques des fous
//!         en utilisant les magic bitboards ou PEXT
//------------------------------------------------------
void init_bishop_attacks()
{
    for (SQUARE sq = A1; sq < N_SQUARES; ++sq)
    {
        Bitboard mask = bishop_masks[sq];
        int bits      = bishop_relevant_bits[sq];
        int indicies  = (1 << bits);

        for (int index = 0; index < indicies; index++)
        {
            Bitboard occupancy = set_occupancy(index, bits, mask);

#ifndef USE_PEXT
            int idx                 = (occupancy * bishop_magics[sq]) >> (64 - bits);
            BISHOP_ATTACKS[sq][idx] = bishop_attacks_on_the_fly(sq, occupancy);
#else
            BISHOP_ATTACKS[sq][_pext_u64(occupancy, mask)] = bishop_attacks_on_the_fly(sq, occupancy);
#endif
        }
    }
}

//======================================================
//! \brief  Génère les attaques de la tour à la volée
//!
//! \param[in]  sq      case de départ de la tour
//! \param[in]  block   bitboard des cases occupées (bloquantes)
//!
//! \return Bitboard des cases attaquées par la tour
//------------------------------------------------------
Bitboard rook_attacks_on_the_fly(SQUARE sq, Bitboard block)
{
    Bitboard attacks = 0;

    int sr = SQ::rank(sq);
    int sf = SQ::file(sq);

    for (int r = sr + 1; r <= 7; r++) {
        attacks |= (1ULL << (r * 8 + sf));
        if (BB::get_bit(block, SQ::square(sf, r)))
            break;
    }

    for (int r = sr - 1; r >= 0; r--) {
        attacks |= (1ULL << (r * 8 + sf));
        if (BB::get_bit(block, SQ::square(sf, r)))
            break;
    }

    for (int f = sf + 1; f <= 7; f++) {
        attacks |= (1ULL << (sr * 8 + f));
        if (BB::get_bit(block, SQ::square(f, sr)))
            break;
    }

    for (int f = sf - 1; f >= 0; f--) {
        attacks |= (1ULL << (sr * 8 + f));
        if (BB::get_bit(block, SQ::square(f, sr)))
            break;
    }

    return attacks;
}

//======================================================
//! \brief  Initialise la table d'attaques des tours
//!         en utilisant les magic bitboards ou PEXT
//------------------------------------------------------
void init_rook_attacks()
{
    for (SQUARE sq = A1; sq < N_SQUARES; ++sq)
    {
        Bitboard mask = rook_masks[sq];
        int bits      = rook_relevant_bits[sq];
        int indicies  = (1 << bits);

        for (int index = 0; index < indicies; index++)
        {
            Bitboard occupancy = set_occupancy(index, bits, mask);

#ifndef USE_PEXT
            int idx               = (occupancy * rook_magics[sq]) >> (64 - bits);
            ROOK_ATTACKS[sq][idx] = rook_attacks_on_the_fly(sq, occupancy);
#else
            ROOK_ATTACKS[sq][_pext_u64(occupancy, mask)]   = rook_attacks_on_the_fly(sq, occupancy);
#endif
        }
    }
}

//================================================
//! \brief  Initialisation des attaques
//------------------------------------------------
void init_masks()
{
    init_bishop_attacks();
    init_rook_attacks();
}

} // namespace

