#ifndef SQUARE_H
#define SQUARE_H

#include <string>
#include "types.h"
#include "bitmask.h"
#include "Bitboard.h"


namespace SQ {

// The constexpr specifier declares that it is possible to evaluate the value of the function or variable at compile time.

[[nodiscard]] inline int square(const int f, const int r) noexcept { return (8*r + f); }

[[nodiscard]] inline int square(const std::string& str) noexcept {
    const int file = str[0] - 'a';
    const int rank = str[1] - '1';
    return(rank * 8 + file);
}

inline std::ostream &operator<<(std::ostream &os, const int square) noexcept {
    os << square_name[square];
    return os;
}

[[nodiscard]] constexpr int rank(int square) noexcept { return (square >> 3);   }   // sq / 8
[[nodiscard]] constexpr int file(int square) noexcept { return (square & 7);    }   // sq % 8

[[nodiscard]] constexpr int north(int square)       noexcept { return (square + NORTH);         }
[[nodiscard]] constexpr int north_west(int square)  noexcept { return (square + NORTH + WEST);  }
[[nodiscard]] constexpr int west(int square)        noexcept { return (square + WEST);          }
[[nodiscard]] constexpr int south_west(int square)  noexcept { return (square + SOUTH + WEST);  }
[[nodiscard]] constexpr int south(int square)       noexcept { return (square + SOUTH);         }
[[nodiscard]] constexpr int south_east(int square)  noexcept { return (square + SOUTH + EAST);  }
[[nodiscard]] constexpr int east(int square)        noexcept { return (square + EAST);          }
[[nodiscard]] constexpr int north_east(int square)  noexcept { return (square + NORTH + EAST);  }
[[nodiscard]] constexpr int south_south(int square) noexcept { return (square + 2*SOUTH);       }
[[nodiscard]] constexpr int north_north(int square) noexcept { return (square + 2*NORTH);       }

// flip vertically
[[nodiscard]] constexpr int flip_square(int sq) noexcept { return sq ^ 56;}
template <Color C>
[[nodiscard]] constexpr int relative_square(int sq) noexcept {
    if constexpr ( C == WHITE)
        return sq;
    else
        return sq ^ 56;
}

//! \brief  crée un bitboard à partir de la colonne d'une case
[[nodiscard]] inline Bitboard square_file_BB(const int sq) noexcept { return FILE_BB[SQ::file(sq)];}

//! \brief  crée un bitboard à partir de la rangée d'une case
[[nodiscard]] inline Bitboard square_rank_BB(const int sq) noexcept { return RANK_BB[SQ::rank(sq)]; }


//! \brief Contrôle si la case "sq" est sur la rangée précédant la promotion (7ème/2ème)
template <Color C>
[[nodiscard]] constexpr bool is_on_seventh_rank(const int sq) {
    return (PromotingRank[C] & BB::square_BB(sq));
}

//! \brief Contrôle si la case "sq" est sur la rangée de promotion (8ème, 1ère)
template <Color C>
[[nodiscard]] constexpr bool is_promotion(const int sq) {
    return (PromotionRank[C] & BB::square_BB(sq));
}

//! \brief Contrôle si la case "sq" est sur la rangée de départ (2ème/7ème)
template <Color C>
[[nodiscard]] constexpr bool is_on_second_rank(const int sq) {
    return(StartingRank[C] & BB::square_BB(sq));
}

//! \brief Convertit une rangée en la rangée relativement à sa couleur
template <Color C>
[[nodiscard]] constexpr int relative_rank8(const int r) { return C == WHITE ? r : RANK_8 - r; }

//! \brief Convertit une rangée en la rangée relativement à sa couleur
template <Color C>
[[nodiscard]] constexpr int relative_rank64(const int sq) { return C == WHITE ? rank(sq) : RANK_8 - rank(sq); }

//! \brief  Retourne le bitboard représentant la colonne de la case 'sq'
[[nodiscard]] constexpr Bitboard fileBB(int square) { return BB::file(SQ::file(square)); }

//! \brief  Retourne le bitboard représentant la rangée de la case 'sq'
[[nodiscard]] constexpr Bitboard rankBB(int square) { return BB::rank(SQ::rank(square)); }



} // namespace


#endif // SQUARE_H
