#ifndef LIBCHESS_BITBOARD_HPP
#define LIBCHESS_BITBOARD_HPP

#include <bit>
#include <cassert>
#include <ostream>
#include "defines.h"
#include <sstream>
#include <iostream>
#include "types.h"
#include "bitmask.h"

namespace BB {

//! \brief  récupère le bit à la position donnée
[[nodiscard]] constexpr bool get_bit(Bitboard b, const SQUARE sq)  noexcept { return (b & (1ULL << sq)) ; }

//! \brief  met le bit à 1
constexpr void set_bit(Bitboard& b, const SQUARE sq) noexcept      { b |= (1ULL << sq);      }

//! \brief met le bit à 0
constexpr void clear_bit(Bitboard& b, const SQUARE sq) noexcept    { b &= ~(1ULL << sq);     }

//! \brief teste si le bit est mis
[[nodiscard]] constexpr bool test_bit(const Bitboard b, const SQUARE sq) noexcept    { return ( b & (1ULL << sq));     }

//! \brief change le bit en son opposé
constexpr void toggle_bit(Bitboard& b, const SQUARE sq) noexcept     { b ^= (1ULL << sq);    }

//! \brief change les deux bits en leur opposé
constexpr void toggle_bit2(Bitboard& b, const SQUARE sq1, const SQUARE sq2) noexcept { b ^= (1ULL << sq1) ^ (1ULL << sq2);    }

//! \brief met le LSB à zéro
constexpr void unset_lsb(Bitboard& b) noexcept { b &= b-1; }

//! \brief met le LSB à zéro, modifie le bitboard, retourne la position du LSB
//! aussi appelée poplsb
[[nodiscard]] constexpr U32 pop_lsb(Bitboard& b) noexcept {
    U32 index = std::countr_zero(b);       // position du LSB
    b &= b - 1;                             // met le LSB à 0
    return index;
}

//! \brief  met le LSB à zéro, modifie le bitboard pointé, retourne la position du LSB (variante pointeur pour Pyrrhic)
constexpr U32 PYRRHIC_pop_lsb(Bitboard* b) noexcept {
    U32 index = std::countr_zero(*b);
    *b &= *b - 1;
    return index;
}

//! \brief retourne la position du LSB
//! Returns the number of consecutive 0 bits in the value of x, starting from the least significant bit (“right”).
//!
//!     countr_zero( 00000000 ) = 8
//!     countr_zero( 11111111 ) = 0
//!     countr_zero( 00011100 ) = 2
//!     countr_zero( 00011101 ) = 0
//!
//!     Fonction aussi nommée : bitScanForward, getlsb
//!     Remplace __builtin_ctzll à partir de C++20
[[nodiscard]] constexpr U32 get_lsb(Bitboard b) noexcept {
    return (std::countr_zero(b));
}

//! \brief bitScanReverse : trouve la position du MSB
//! countl_zero(0) = 64
//! donc attention à : 63 - countl_zero(0) = -1 !!!
[[nodiscard]] constexpr int get_msb(Bitboard b)  noexcept {
    b |= 1;
    return 63 - std::countl_zero(b);
}

//! \brief  Donne le nombre de bits à 1 du bitboard
[[nodiscard]] constexpr int  count_bit(Bitboard b) noexcept { return std::popcount(b);}


//! \brief  Teste si le bitboard est vide
[[nodiscard]] constexpr bool empty(Bitboard b) noexcept { return b == 0ULL; }


//! \brief Teste si le bitboard a plus d'un set à 1
[[nodiscard]] constexpr bool multiple(Bitboard b) noexcept { return (b & (b - 1)); }

//! \brief Teste si le bitboard n'a qu'un seul bit à 1
[[nodiscard]] constexpr bool single(Bitboard b) noexcept { return (b && !multiple(b)); }




//! \brief  Byte swap (= symétrie verticale du bitboard)
[[nodiscard]] constexpr Bitboard byte_swap(Bitboard b) noexcept
{
#if defined(__GNUC__)
    return __builtin_bswap64(b);
#elif defined(__llvm__)
    return __builtin_bswap64(b);
#elif defined(_MSC_VER)
    return _byteswap_uint64(b);
#else
    b = ((b >> 8)  & 0x00FF00FF00FF00FFULL) | ((b & 0x00FF00FF00FF00FFULL) << 8);
    b = ((b >> 16) & 0x0000FFFF0000FFFFULL) | ((b & 0x0000FFFF0000FFFFULL) << 16);
    return (b >> 32) | (b << 32);
#endif
}

//! \brief  Retourne le bitboard des cases de la même couleur (claire/sombre) que "sq"
[[nodiscard]] constexpr Bitboard squaresOfMatchingColour(SQUARE sq) noexcept {
    assert(A1 <= sq && sq < N_SQUARES);
    return BB::test_bit(LightSquares, sq) ? LightSquares : DarkSquares;
}



//! \brief  Décale le bitboard d'une case vers le nord
[[nodiscard]] constexpr Bitboard north(Bitboard b)      noexcept { return (b << 8); }
//! \brief  Décale le bitboard d'une case vers le sud
[[nodiscard]] constexpr Bitboard south(Bitboard b)      noexcept { return (b >> 8); }
//! \brief  Décale le bitboard d'une case vers l'est (sans wrap sur la colonne A)
[[nodiscard]] constexpr Bitboard east(Bitboard b)       noexcept { return ((b << 1) & NOT_FILE_A_BB); }
//! \brief  Décale le bitboard d'une case vers l'ouest (sans wrap sur la colonne H)
[[nodiscard]] constexpr Bitboard west(Bitboard b)       noexcept { return ((b >> 1) & NOT_FILE_H_BB); }
//! \brief  Décale le bitboard d'une case vers le nord-est (sans wrap sur la colonne A)
[[nodiscard]] constexpr Bitboard north_east(Bitboard b) noexcept { return ((b << 9) & NOT_FILE_A_BB); }
//! \brief  Décale le bitboard d'une case vers le sud-est (sans wrap sur la colonne A)
[[nodiscard]] constexpr Bitboard south_east(Bitboard b) noexcept { return ((b >> 7) & NOT_FILE_A_BB); }
//! \brief  Décale le bitboard d'une case vers le sud-ouest (sans wrap sur la colonne H)
[[nodiscard]] constexpr Bitboard south_west(Bitboard b) noexcept { return ((b >> 9) & NOT_FILE_H_BB); }
//! \brief  Décale le bitboard d'une case vers le nord-ouest (sans wrap sur la colonne H)
[[nodiscard]] constexpr Bitboard north_west(Bitboard b) noexcept { return ((b << 7) & NOT_FILE_H_BB); }

//! \brief  Décale le bitboard de deux cases vers le nord
[[nodiscard]] constexpr Bitboard north_north(Bitboard b)  noexcept { return (b << 16); }
//! \brief  Décale le bitboard de deux cases vers le sud
[[nodiscard]] constexpr Bitboard south_south(Bitboard b)  noexcept { return (b >> 16); }

//! \brief  Décale le bitboard façon "saut de cavalier" nord-nord-est (sans wrap colonne A)
[[nodiscard]] constexpr Bitboard north_north_east(Bitboard b) noexcept { return ((b << 17) & NOT_FILE_A_BB); }
//! \brief  Décale le bitboard façon "saut de cavalier" nord-nord-ouest (sans wrap colonne H)
[[nodiscard]] constexpr Bitboard north_north_west(Bitboard b) noexcept { return ((b << 15) & NOT_FILE_H_BB); }

//! \brief  Décale le bitboard façon "saut de cavalier" sud-sud-est (sans wrap colonne A)
[[nodiscard]] constexpr Bitboard south_south_east(Bitboard b) noexcept { return ((b >> 15) & NOT_FILE_A_BB); }
//! \brief  Décale le bitboard façon "saut de cavalier" sud-sud-ouest (sans wrap colonne H)
[[nodiscard]] constexpr Bitboard south_south_west(Bitboard b) noexcept { return ((b >> 17) & NOT_FILE_H_BB); }

//! \brief  Décale le bitboard façon "saut de cavalier" est-est-nord (sans wrap colonnes A/B)
[[nodiscard]] constexpr Bitboard east_east_north(Bitboard b)  noexcept { return ((b << 10) & NOT_FILE_AB_BB); }
//! \brief  Décale le bitboard façon "saut de cavalier" est-est-sud (sans wrap colonnes A/B)
[[nodiscard]] constexpr Bitboard east_east_south(Bitboard b)  noexcept { return ((b >> 6)  & NOT_FILE_AB_BB); }

//! \brief  Décale le bitboard façon "saut de cavalier" ouest-ouest-nord (sans wrap colonnes H/G)
[[nodiscard]] constexpr Bitboard west_west_north(Bitboard b)  noexcept { return ((b << 6)  & NOT_FILE_HG_BB); }
//! \brief  Décale le bitboard façon "saut de cavalier" ouest-ouest-sud (sans wrap colonnes H/G)
[[nodiscard]] constexpr Bitboard west_west_south(Bitboard b)  noexcept { return ((b >> 10) & NOT_FILE_HG_BB); }

//! \brief Décale d'une case dans la direction donnée
template <Direction D>
[[nodiscard]] constexpr Bitboard shift(const Bitboard b) noexcept
{
    switch (D)
    {
    case NORTH:
        return b << 8;
        break;
    case NORTH_EAST:
        return (b << 9) & NOT_FILE_A_BB;
        break;
    case EAST:
        return (b << 1) & NOT_FILE_A_BB;
        break;
    case SOUTH_EAST:
        return (b >> 7) & NOT_FILE_A_BB;
        break;
    case SOUTH:
        return b >> 8;
        break;
    case SOUTH_WEST:
        return (b >> 9) & NOT_FILE_H_BB;
        break;
    case WEST:
        return (b >> 1) & NOT_FILE_H_BB;
        break;
    case NORTH_WEST:
        return (b << 7) & NOT_FILE_H_BB;
        break;
    case NORTH_NORTH:
        return b << 16;
        break;
    case SOUTH_SOUTH:
        return b >> 16;
        break;
    default:
        return b;
    }
}

//======================================
//! \brief  Impression d'un Bitboard
//--------------------------------------
inline void PrintBB(const Bitboard bb, const std::string& message)
{
    std::cout << "--------------------------- " << message << std::endl;
    std::stringstream ss;
    int i = 56;

    ss << std::endl;
    ss << "  8 " ;

    while (i >= 0) {
        const SQUARE sq = (i);
        if (bb & SQ::square_BB(sq)) {
            ss << "1 ";
        } else {
            ss << ". ";
        }

        if (i % 8 == 7)
        {
            if (i/8 != 0)
                ss << "\n  " << i/8 << ' ';
            i -= 16;
        }

        i++;
    }
    ss << "\n    a b c d e f g h\n\n";

    std::cout << ss.str() << std::endl;
}

} // namespace

#endif
