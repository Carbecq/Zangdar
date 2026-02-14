#ifndef BITMASK_H
#define BITMASK_H

#include <cassert>

#include "defines.h"
#include "types.h"

constexpr Bitboard FILE_A_BB = 0x0101010101010101ULL;
constexpr Bitboard FILE_B_BB = 0x0202020202020202ULL;
constexpr Bitboard FILE_C_BB = 0x0404040404040404ULL;
constexpr Bitboard FILE_D_BB = 0x0808080808080808ULL;
constexpr Bitboard FILE_E_BB = 0x1010101010101010ULL;
constexpr Bitboard FILE_F_BB = 0x2020202020202020ULL;
constexpr Bitboard FILE_G_BB = 0x4040404040404040ULL;
constexpr Bitboard FILE_H_BB = 0x8080808080808080ULL;

constexpr Bitboard RANK_1_BB = 0x00000000000000ffULL;
constexpr Bitboard RANK_2_BB = 0x000000000000ff00ULL;
constexpr Bitboard RANK_3_BB = 0x0000000000ff0000ULL;
constexpr Bitboard RANK_4_BB = 0x00000000ff000000ULL;
constexpr Bitboard RANK_5_BB = 0x000000ff00000000ULL;
constexpr Bitboard RANK_6_BB = 0x0000ff0000000000ULL;
constexpr Bitboard RANK_7_BB = 0x00ff000000000000ULL;
constexpr Bitboard RANK_8_BB = 0xff00000000000000ULL;

constexpr Bitboard FILE_BB[N_FILES] = {FILE_A_BB, FILE_B_BB, FILE_C_BB, FILE_D_BB, FILE_E_BB, FILE_F_BB, FILE_G_BB, FILE_H_BB};
constexpr Bitboard RANK_BB[N_RANKS] = {RANK_1_BB, RANK_2_BB, RANK_3_BB, RANK_4_BB, RANK_5_BB, RANK_6_BB, RANK_7_BB, RANK_8_BB};

constexpr Bitboard NOT_FILE_A_BB = ~FILE_A_BB;
constexpr Bitboard NOT_FILE_H_BB = ~FILE_H_BB;

constexpr Bitboard NOT_FILE_HG_BB = 4557430888798830399ULL;
constexpr Bitboard NOT_FILE_AB_BB = 18229723555195321596ULL;

constexpr Bitboard DiagMask16[16] = {
    0x8040201008040201ULL, 0x4020100804020100ULL, 0x2010080402010000ULL,
    0x1008040201000000ULL, 0x0804020100000000ULL, 0x0402010000000000ULL,
    0x0201000000000000ULL, 0x0100000000000000ULL,
    0x0000000000000000ULL,
    0x0000000000000080ULL, 0x0000000000008040ULL, 0x0000000000804020ULL,
    0x0000000080402010ULL, 0x0000008040201008ULL, 0x0000804020100804ULL,
    0x0080402010080402ULL
};
constexpr Bitboard ADiagMask16[16] = {
    0x0102040810204080ULL, 0x0001020408102040ULL, 0x0000010204081020ULL,
    0x0000000102040810ULL, 0x0000000001020408ULL, 0x0000000000010204ULL,
    0x0000000000000102ULL, 0x0000000000000001ULL,
    0x0000000000000000ULL,
    0x8000000000000000ULL, 0x4080000000000000ULL, 0x2040800000000000ULL,
    0x1020408000000000ULL, 0x0810204080000000ULL, 0x0408102040800000ULL,
    0x0204081020408000ULL
};




constexpr Bitboard LightSquares  = 0x55aa55aa55aa55aaULL;
constexpr Bitboard DarkSquares   = 0xaa55aa55aa55aa55ULL;
constexpr Bitboard Empty         = 0x0000000000000000ULL;
constexpr Bitboard AllSquares    = 0xffffffffffffffffULL;

constexpr Bitboard PromotionRank[N_COLORS] = {RANK_8_BB, RANK_1_BB};
constexpr Bitboard PromotingRank[N_COLORS] = {RANK_7_BB, RANK_2_BB};
constexpr Bitboard StartingRank[N_COLORS]  = {RANK_2_BB, RANK_7_BB};



//================================================================================

namespace SQ {

// The constexpr specifier declares that it is possible to evaluate the value of the function or variable at compile time.

[[nodiscard]] constexpr inline int square(const int f, const int r) noexcept {
    return (8*r + f);
}

[[nodiscard]] inline int square(const std::string& str) noexcept {
    const int file = str[0] - 'a';
    const int rank = str[1] - '1';
    return (rank * 8 + file);
}

[[nodiscard]] constexpr inline bool is_ok(SQUARE sq) noexcept { return sq >= A1 && sq <= H8; }

[[nodiscard]] inline std::ostream &operator<<(std::ostream &os, const int square) noexcept {
    os << square_name[square];
    return os;
}

[[nodiscard]] constexpr inline U32 rank(SQUARE square) noexcept { return (square >> 3);   }   // sq / 8
[[nodiscard]] constexpr inline U32 file(SQUARE square) noexcept { return (square & 7);    }   // sq % 8

[[nodiscard]] constexpr inline SQUARE north(SQUARE square)       noexcept { assert(is_ok(square + NORTH)) ;         return (square + NORTH);         }
[[nodiscard]] constexpr inline SQUARE north_west(SQUARE square)  noexcept { assert(is_ok(square + NORTH_WEST)) ;    return (square + NORTH_WEST);  }
[[nodiscard]] constexpr inline SQUARE west(SQUARE square)        noexcept { assert(is_ok(square + WEST)) ;          return (square + WEST);          }
[[nodiscard]] constexpr inline SQUARE south_west(SQUARE square)  noexcept { assert(is_ok(square + SOUTH_WEST)) ;    return (square + SOUTH_WEST);  }
[[nodiscard]] constexpr inline SQUARE south(SQUARE square)       noexcept { assert(is_ok(square + SOUTH)) ;         return (square + SOUTH);         }
[[nodiscard]] constexpr inline SQUARE south_east(SQUARE square)  noexcept { assert(is_ok(square + SOUTH_EAST)) ;    return (square + SOUTH_EAST);  }
[[nodiscard]] constexpr inline SQUARE east(SQUARE square)        noexcept { assert(is_ok(square + EAST)) ;          return (square + EAST);          }
[[nodiscard]] constexpr inline SQUARE north_east(SQUARE square)  noexcept { assert(is_ok(square + NORTH_EAST)) ;    return (square + NORTH_EAST);  }
[[nodiscard]] constexpr inline SQUARE south_south(SQUARE square) noexcept { assert(is_ok(square + SOUTH_SOUTH)) ;   return (square + SOUTH_SOUTH);   }
[[nodiscard]] constexpr inline SQUARE north_north(SQUARE square) noexcept { assert(is_ok(square + NORTH_NORTH)) ;   return (square + NORTH_NORTH);   }

// flip vertically
[[nodiscard]] constexpr inline SQUARE mirrorVertically(SQUARE sq) noexcept { return (sq ^ 56); } // 0b111000

// flip horizontally
[[nodiscard]] constexpr inline SQUARE  mirrorHorizontally(SQUARE  sq) noexcept { return (sq ^ 7); } // 0b111

template <Color C>
[[nodiscard]] constexpr inline SQUARE relative_square(SQUARE sq) noexcept {
    if constexpr ( C == WHITE)
        return sq;
    else
        return (sq ^ 56); // 0b111000
}

//---------------------------------------------------

//! \brief  crée un bitboard à partir d'une case
[[nodiscard]] constexpr inline Bitboard square_BB(const SQUARE sq) noexcept { return(1ULL << sq);}

//---------------------------------------------------


//! \brief Contrôle si la case "sq" est sur la rangée précédant la promotion (7ème/2ème)
template <Color C>
[[nodiscard]] constexpr bool is_on_seventh_rank(const SQUARE sq) noexcept {
    return (PromotingRank[C] & square_BB(sq));
}

//! \brief Contrôle si la case "sq" est sur la rangée de promotion (8ème, 1ère)
template <Color C>
[[nodiscard]] constexpr bool is_promotion(const SQUARE sq) noexcept {
    return (PromotionRank[C] & square_BB(sq));
}

//! \brief Contrôle si la case "sq" est sur la rangée de départ (2ème/7ème)
template <Color C>
[[nodiscard]] constexpr bool is_on_second_rank(const SQUARE sq) noexcept {
    return(StartingRank[C] & square_BB(sq));
}


} // namespace

//================================================================================
//  Initialisations utilisant constexpr et lambda; nécessite C++17
//================================================================================

// ATTENTION : std::abs n'est PAS constexpr (jusquà C++23 ??)
template<typename T>
constexpr T myabs(T t) {
    return ( (t) < T(0) ? -(t) : (t));
}

// une façon de faire
constexpr std::array<Bitboard, N_SQUARES> RankMask64 = []() -> std::array<Bitboard, N_SQUARES> {
    std::array<Bitboard, N_SQUARES> b{};
    for (SQUARE sq = A1; sq < N_SQUARES; ++sq) {
        b[sq] = RANK_BB[SQ::rank(sq)];
    }
    return b;
}();

// une autrefaçon
constexpr std::array<Bitboard, N_SQUARES> FileMask64 = [] {
    auto b = decltype(FileMask64){};
    for (SQUARE sq = A1; sq < N_SQUARES; ++sq) {
        b[sq] = FILE_BB[SQ::file(sq)];
    }
    return b;
}();

constexpr std::array<Bitboard, N_SQUARES> DiagonalMask64 = [] {
    auto b = decltype(DiagonalMask64){};
    for (SQUARE sq = A1; sq < N_SQUARES; ++sq) {
        b[sq] = DiagMask16[((sq >> 3) - (sq & 7)) & 15];
    }
    return b;
}();

constexpr std::array<Bitboard, N_SQUARES> AntiDiagonalMask64 = [] {
    auto b = decltype(AntiDiagonalMask64){};
    for (SQUARE sq = A1; sq < N_SQUARES; ++sq) {
        b[sq] = ADiagMask16[((sq >> 3) + (sq & 7)) ^ 7];
    }
    return b;
}();



//------------------------------------------------------------------
// initialise un bitboard constitué des cases entre 2 cases
// alignées en diagonale, ou droite
// ne contient pas les cases from et dest
constexpr std::array<std::array<Bitboard, N_SQUARES>, N_SQUARES> SQUARES_BETWEEN_MASK = [] {
    auto b = decltype(SQUARES_BETWEEN_MASK){};

    for (SQUARE i = A1; i < N_SQUARES; ++i) {
        for (SQUARE j = A1; j < N_SQUARES; ++j) {
            SQUARE sq1 = i;
            SQUARE sq2 = j;

            const int dx = (SQ::file(sq2) - SQ::file(sq1));
            const int dy = (SQ::rank(sq2) - SQ::rank(sq1));
            const int adx = dx > 0 ? dx : -dx;
            const int ady = dy > 0 ? dy : -dy;

            if (dx == 0 || dy == 0 || adx == ady)
            {
                Bitboard mask = 0ULL;
                while (sq1 != sq2)
                {
                    if (dx > 0) {
                        sq1 += EAST;
                    } else if (dx < 0) {
                        sq1 += WEST;
                    }
                    if (dy > 0) {
                        sq1 += NORTH;
                    } else if (dy < 0) {
                        sq1 += SOUTH;
                    }
                    mask |= SQ::square_BB(sq1);
                }
                b[i][j] = mask & ~SQ::square_BB(sq2);
            }
        }
    }

    return b;
}();

//------------------------------------------------------------------
//  table utilisée pour les générateur de coups

struct Mask
{
    int direction[N_SQUARES];
};


/* Make a square from file & rank if inside the board */
[[nodiscard]] constexpr int square_safe(const int f, const int r) {
    if (0 <= f && f < 8 && 0 <= r && r < 8)
        return SQ::square(f, r);
    else
        return SQUARE_NONE;
}

constexpr std::array<Mask, N_SQUARES> DirectionMask = [] {
    auto b = decltype(DirectionMask){};
    int d[N_SQUARES][N_SQUARES];
    constexpr int king_dir[8][2] = {{-1, -1}, {-1, 0}, {-1, 1}, {0, -1}, {0, 1}, {1, -1}, {1, 0}, {1, 1}};

    for (SQUARE sq = A1; sq < N_SQUARES; ++sq)
    {
        auto f = SQ::file(sq);
        auto r = SQ::rank(sq);

        for (size_t y = A1; y < N_SQUARES; ++y)
            d[sq][y] = 0;

        // directions & between
        for (int i = 0; i < 8; ++i)
        {
            for (int j = 1; j < 8; ++j)
            {
                int y = square_safe(f + king_dir[i][0] * j, r + king_dir[i][1] * j);
                if (y != SQUARE_NONE)
                {
                    d[sq][y] = king_dir[i][0] + 8 * king_dir[i][1];
                    b[sq].direction[y] = myabs(d[sq][y]);
                }
            }
        }

    }

    return b;
}();


//------------------------------------------------------------------------
//  Fonctions
//------------------------------------------------------------------------

[[nodiscard]] constexpr Bitboard squares_between(size_t sq1, size_t sq2) {
    return SQUARES_BETWEEN_MASK[sq1][sq2];
}


#endif // BITMASK_H
