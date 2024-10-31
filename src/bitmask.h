#ifndef BITMASK_H
#define BITMASK_H

#include "defines.h"
#include "types.h"

constexpr auto FILE_A_BB = 0x0101010101010101ULL;
constexpr auto FILE_B_BB = 0x0202020202020202ULL;
constexpr auto FILE_C_BB = 0x0404040404040404ULL;
constexpr auto FILE_D_BB = 0x0808080808080808ULL;
constexpr auto FILE_E_BB = 0x1010101010101010ULL;
constexpr auto FILE_F_BB = 0x2020202020202020ULL;
constexpr auto FILE_G_BB = 0x4040404040404040ULL;
constexpr auto FILE_H_BB = 0x8080808080808080ULL;

constexpr auto RANK_1_BB = 0x00000000000000ffULL;
constexpr auto RANK_2_BB = 0x000000000000ff00ULL;
constexpr auto RANK_3_BB = 0x0000000000ff0000ULL;
constexpr auto RANK_4_BB = 0x00000000ff000000ULL;
constexpr auto RANK_5_BB = 0x000000ff00000000ULL;
constexpr auto RANK_6_BB = 0x0000ff0000000000ULL;
constexpr auto RANK_7_BB = 0x00ff000000000000ULL;
constexpr auto RANK_8_BB = 0xff00000000000000ULL;

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

//! \brief  donne le bitboard des colonnes adjacentes à la colonne donnée
constexpr Bitboard AdjacentFilesMask8[N_FILES] =
{
 FILE_B_BB,
 FILE_A_BB | FILE_C_BB,
 FILE_B_BB | FILE_D_BB,
 FILE_C_BB | FILE_E_BB,
 FILE_D_BB | FILE_F_BB,
 FILE_E_BB | FILE_G_BB,
 FILE_F_BB | FILE_H_BB,
 FILE_G_BB
};



constexpr Bitboard LightSquares  = 0x55aa55aa55aa55aaULL;
constexpr Bitboard DarkSquares   = 0xaa55aa55aa55aa55ULL;
constexpr Bitboard Empty         = 0x0000000000000000ULL;
constexpr Bitboard AllSquares    = 0xffffffffffffffffULL;
constexpr Bitboard OuterSquares  = 0xff818181818181ffULL;

constexpr Bitboard CenterFiles   = FILE_C_BB | FILE_D_BB | FILE_E_BB | FILE_F_BB;
constexpr Bitboard CenterSquares = (FILE_D_BB | FILE_E_BB) & (RANK_4_BB | RANK_5_BB);   // D4-D5 / E4-E5
constexpr Bitboard QueenSide     = FILE_A_BB | FILE_B_BB | FILE_C_BB | FILE_D_BB;
constexpr Bitboard KingSide      = FILE_E_BB | FILE_F_BB | FILE_G_BB | FILE_H_BB;
constexpr Bitboard LongDiagonals = 0x8142241818244281ULL;   // A1-H8 / A8-H1

constexpr Bitboard PromotionRank[N_COLORS] = {RANK_8_BB, RANK_1_BB};
constexpr Bitboard PromotingRank[N_COLORS] = {RANK_7_BB, RANK_2_BB};
constexpr Bitboard StartingRank[N_COLORS]  = {RANK_2_BB, RANK_7_BB};

constexpr Bitboard PassedPawnMask[N_COLORS][N_SQUARES] = {
    {
     0x0303030303030300L, 0x0707070707070700L, 0x0e0e0e0e0e0e0e00L, 0x1c1c1c1c1c1c1c00L, 0x3838383838383800L,
     0x7070707070707000L, 0xe0e0e0e0e0e0e000L, 0xc0c0c0c0c0c0c000L, 0x0303030303030000L, 0x0707070707070000L,
     0x0e0e0e0e0e0e0000L, 0x1c1c1c1c1c1c0000L, 0x3838383838380000L, 0x7070707070700000L, 0xe0e0e0e0e0e00000L,
     0xc0c0c0c0c0c00000L, 0x0303030303000000L, 0x0707070707000000L, 0x0e0e0e0e0e000000L, 0x1c1c1c1c1c000000L,
     0x3838383838000000L, 0x7070707070000000L, 0xe0e0e0e0e0000000L, 0xc0c0c0c0c0000000L, 0x0303030300000000L,
     0x0707070700000000L, 0x0e0e0e0e00000000L, 0x1c1c1c1c00000000L, 0x3838383800000000L, 0x7070707000000000L,
     0xe0e0e0e000000000L, 0xc0c0c0c000000000L, 0x0303030000000000L, 0x0707070000000000L, 0x0e0e0e0000000000L,
     0x1c1c1c0000000000L, 0x3838380000000000L, 0x7070700000000000L, 0xe0e0e00000000000L, 0xc0c0c00000000000L,
     0x0303000000000000L, 0x0707000000000000L, 0x0e0e000000000000L, 0x1c1c000000000000L, 0x3838000000000000L,
     0x7070000000000000L, 0xe0e0000000000000L, 0xc0c0000000000000L, 0x0300000000000000L, 0x0700000000000000L,
     0x0e00000000000000L, 0x1c00000000000000L, 0x3800000000000000L, 0x7000000000000000L, 0xe000000000000000L,
     0xc000000000000000L, 0x0000000000000000L, 0x0000000000000000L, 0x0000000000000000L, 0x0000000000000000L,
     0x0000000000000000L, 0x0000000000000000L, 0x0000000000000000L, 0x0000000000000000L
    },
    {
     0x0000000000000000L, 0x0000000000000000L, 0x0000000000000000L, 0x0000000000000000L, 0x0000000000000000L,
     0x0000000000000000L, 0x0000000000000000L, 0x0000000000000000L, 0x0000000000000003L, 0x0000000000000007L,
     0x000000000000000eL, 0x000000000000001cL, 0x0000000000000038L, 0x0000000000000070L, 0x00000000000000e0L,
     0x00000000000000c0L, 0x0000000000000303L, 0x0000000000000707L, 0x0000000000000e0eL, 0x0000000000001c1cL,
     0x0000000000003838L, 0x0000000000007070L, 0x000000000000e0e0L, 0x000000000000c0c0L, 0x0000000000030303L,
     0x0000000000070707L, 0x00000000000e0e0eL, 0x00000000001c1c1cL, 0x0000000000383838L, 0x0000000000707070L,
     0x0000000000e0e0e0L, 0x0000000000c0c0c0L, 0x0000000003030303L, 0x0000000007070707L, 0x000000000e0e0e0eL,
     0x000000001c1c1c1cL, 0x0000000038383838L, 0x0000000070707070L, 0x00000000e0e0e0e0L, 0x00000000c0c0c0c0L,
     0x0000000303030303L, 0x0000000707070707L, 0x0000000e0e0e0e0eL, 0x0000001c1c1c1c1cL, 0x0000003838383838L,
     0x0000007070707070L, 0x000000e0e0e0e0e0L, 0x000000c0c0c0c0c0L, 0x0000030303030303L, 0x0000070707070707L,
     0x00000e0e0e0e0e0eL, 0x00001c1c1c1c1c1cL, 0x0000383838383838L, 0x0000707070707070L, 0x0000e0e0e0e0e0e0L,
     0x0000c0c0c0c0c0c0L, 0x0003030303030303L, 0x0007070707070707L, 0x000e0e0e0e0e0e0eL, 0x001c1c1c1c1c1c1cL,
     0x0038383838383838L, 0x0070707070707070L, 0x00e0e0e0e0e0e0e0L, 0x00c0c0c0c0c0c0c0L,
    }
};

constexpr Bitboard OutpostRanksMasks[N_COLORS] = { RANK_4_BB | RANK_5_BB | RANK_6_BB, RANK_3_BB | RANK_4_BB | RANK_5_BB };

//================================================================================

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

//---------------------------------------------------

//! \brief  crée un bitboard à partir d'une case
[[nodiscard]] inline Bitboard square_BB(const int sq) noexcept { return(1ULL << sq);}

//-------------

//! \brief  Retourne le bitboard représentant toutes les cases appartenant à la colonne 'file'
[[nodiscard]] inline Bitboard file_mask8(int file) { return FILE_BB[file]; }

//! \brief  Retourne le bitboard représentant toutes les cases appartenant à la colonne de la case 'square'
[[nodiscard]] inline Bitboard file_mask64(const int square) noexcept { return FILE_BB[SQ::file(square)];}
// [[nodiscard]] constexpr Bitboard fileBB(int square) { return BB::file(SQ::file(square)); }

//-------------

//! \brief  Retourne le bitboard représentant toutes les cases appartenant à la rangée 'rank'
[[nodiscard]] inline Bitboard rank_mask8(int rank) { return RANK_BB[rank]; }

//! \brief  Retourne le bitboard représentant toutes les cases appartenant à la rangée de la case 'square'
// [[nodiscard]] constexpr Bitboard rankBB(int square) { return BB::rank(SQ::rank(square)); }
[[nodiscard]] inline Bitboard rank_mask64(const int square) noexcept { return RANK_BB[SQ::rank(square)]; }

//---------------------------------------------------


//! \brief Contrôle si la case "sq" est sur la rangée précédant la promotion (7ème/2ème)
template <Color C>
[[nodiscard]] constexpr bool is_on_seventh_rank(const int sq) {
    return (PromotingRank[C] & square_BB(sq));
}

//! \brief Contrôle si la case "sq" est sur la rangée de promotion (8ème, 1ère)
template <Color C>
[[nodiscard]] constexpr bool is_promotion(const int sq) {
    return (PromotionRank[C] & square_BB(sq));
}

//! \brief Contrôle si la case "sq" est sur la rangée de départ (2ème/7ème)
template <Color C>
[[nodiscard]] constexpr bool is_on_second_rank(const int sq) {
    return(StartingRank[C] & square_BB(sq));
}

//! \brief Convertit une rangée en la rangée relativement à sa couleur
template <Color C>
[[nodiscard]] constexpr int relative_rank8(const int r) { return C == WHITE ? r : RANK_8 - r; }

//! \brief Convertit une rangée en la rangée relativement à sa couleur
template <Color C>
[[nodiscard]] constexpr int relative_rank64(const int sq) { return C == WHITE ? rank(sq) : RANK_8 - rank(sq); }


} // namespace

//================================================================================

extern Bitboard RankMask64[N_SQUARES];
extern Bitboard FileMask64[N_SQUARES];              // toutes les cases de la colonne contenant la case spécifiée, moins cette dernière
extern Bitboard DiagonalMask64[N_SQUARES];
extern Bitboard AntiDiagonalMask64[N_SQUARES];
extern Bitboard AdjacentFilesMask64[N_SQUARES];

extern int      MANHATTAN_DISTANCE[N_SQUARES][N_SQUARES];
extern int      CHEBYSHEV_DISTANCE[N_SQUARES][N_SQUARES];
extern Bitboard SQUARES_BETWEEN_MASK[N_SQUARES][N_SQUARES];

extern Bitboard RearSpanMask[N_COLORS][N_SQUARES];
extern Bitboard BackwardMask[N_COLORS][N_SQUARES];
extern Bitboard OutpostSquareMasks[N_COLORS][N_SQUARES];

extern Bitboard ForwardRanksMasks[N_COLORS][N_RANKS];
extern Bitboard ForwardFileMasks[N_COLORS][N_SQUARES];

// Init a table for the King Areas. Use the King's square, the King's target
// squares, and the squares within the pawn shield. When on the A/H files, extend
// the King Area to include an additional file, namely the C and F file respectively
extern Bitboard KingAreaMasks[N_COLORS][N_SQUARES];
extern int      KingPawnFileDistance[N_FILES][1 << N_FILES];

struct Mask
{
    int direction[N_SQUARES];
};
extern std::array<Mask, N_SQUARES> allmask;




//------------------------------------------------------------------------
//  Fonctions
//------------------------------------------------------------------------

[[nodiscard]] inline Bitboard squares_between(const int sq1, const int sq2) noexcept {
    return SQUARES_BETWEEN_MASK[sq1][sq2];
}

[[nodiscard]] inline int manhattan_distance(const int sq1, const int sq2) noexcept {
    return MANHATTAN_DISTANCE[sq1][sq2];
}

[[nodiscard]] inline int chebyshev_distance(const int sq1, const int sq2) noexcept {
    return CHEBYSHEV_DISTANCE[sq1][sq2];
}

template <Color C>
[[nodiscard]] constexpr Bitboard outpostRanksMasks() noexcept {
    return OutpostRanksMasks[C];
}

template <Color C>
[[nodiscard]] inline Bitboard outpostSquareMasks(int sq) noexcept {
    return OutpostSquareMasks[C][sq];
}

template <Color C>
[[nodiscard]] inline Bitboard forwardRanksMasks(int rank) {
    return ForwardRanksMasks[C][rank];
}

template <Color C>
[[nodiscard]] inline Bitboard forwardFileMasks(int sq) {
    return ForwardFileMasks[C][sq];
}

#endif // BITMASK_H
