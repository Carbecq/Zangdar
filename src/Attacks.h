#ifndef ATTACKS_H
#define ATTACKS_H

#include "bitmask.h"
#include <array>
#include <bit>
#include <cassert>
#if defined USE_PEXT
#include "immintrin.h"
#endif

#include "types.h"

//! \brief  Namespace pour les attaques des différentes pièces
//!
namespace  Attacks {

constexpr Bitboard KING_ATTACKS[64] = {
    0x302, 0x705, 0xe0a, 0x1c14,
    0x3828, 0x7050, 0xe0a0, 0xc040,
    0x30203, 0x70507, 0xe0a0e, 0x1c141c,
    0x382838, 0x705070, 0xe0a0e0, 0xc040c0,
    0x3020300, 0x7050700, 0xe0a0e00, 0x1c141c00,
    0x38283800, 0x70507000, 0xe0a0e000, 0xc040c000,
    0x302030000, 0x705070000, 0xe0a0e0000, 0x1c141c0000,
    0x3828380000, 0x7050700000, 0xe0a0e00000, 0xc040c00000,
    0x30203000000, 0x70507000000, 0xe0a0e000000, 0x1c141c000000,
    0x382838000000, 0x705070000000, 0xe0a0e0000000, 0xc040c0000000,
    0x3020300000000, 0x7050700000000, 0xe0a0e00000000, 0x1c141c00000000,
    0x38283800000000, 0x70507000000000, 0xe0a0e000000000, 0xc040c000000000,
    0x302030000000000, 0x705070000000000, 0xe0a0e0000000000, 0x1c141c0000000000,
    0x3828380000000000, 0x7050700000000000, 0xe0a0e00000000000, 0xc040c00000000000,
    0x203000000000000, 0x507000000000000, 0xa0e000000000000, 0x141c000000000000,
    0x2838000000000000, 0x5070000000000000, 0xa0e0000000000000, 0x40c0000000000000,
};

// Table précalculée des bitboards de coups de cavalier
constexpr Bitboard KNIGHT_ATTACKS[64] = {
    0x20400, 0x50800, 0xa1100, 0x142200,
    0x284400, 0x508800, 0xa01000, 0x402000,
    0x2040004, 0x5080008, 0xa110011, 0x14220022,
    0x28440044, 0x50880088, 0xa0100010, 0x40200020,
    0x204000402, 0x508000805, 0xa1100110a, 0x1422002214,
    0x2844004428, 0x5088008850, 0xa0100010a0, 0x4020002040,
    0x20400040200, 0x50800080500, 0xa1100110a00, 0x142200221400,
    0x284400442800, 0x508800885000, 0xa0100010a000, 0x402000204000,
    0x2040004020000, 0x5080008050000, 0xa1100110a0000, 0x14220022140000,
    0x28440044280000, 0x50880088500000, 0xa0100010a00000, 0x40200020400000,
    0x204000402000000, 0x508000805000000, 0xa1100110a000000, 0x1422002214000000,
    0x2844004428000000, 0x5088008850000000, 0xa0100010a0000000, 0x4020002040000000,
    0x400040200000000, 0x800080500000000, 0x1100110a00000000, 0x2200221400000000,
    0x4400442800000000, 0x8800885000000000, 0x100010a000000000, 0x2000204000000000,
    0x4020000000000, 0x8050000000000, 0x110a0000000000, 0x22140000000000,
    0x44280000000000, 0x0088500000000000, 0x0010a00000000000, 0x20400000000000
};

// Table précalculée des bitboards de coups de pion
constexpr Bitboard PAWN_ATTACKS[2][64] = {
    {
        0x200, 0x500, 0xa00, 0x1400,
        0x2800, 0x5000, 0xa000, 0x4000,
        0x20000, 0x50000, 0xa0000, 0x140000,
        0x280000, 0x500000, 0xa00000, 0x400000,
        0x2000000, 0x5000000, 0xa000000, 0x14000000,
        0x28000000, 0x50000000, 0xa0000000, 0x40000000,
        0x200000000, 0x500000000, 0xa00000000, 0x1400000000,
        0x2800000000, 0x5000000000, 0xa000000000, 0x4000000000,
        0x20000000000, 0x50000000000, 0xa0000000000, 0x140000000000,
        0x280000000000, 0x500000000000, 0xa00000000000, 0x400000000000,
        0x2000000000000, 0x5000000000000, 0xa000000000000, 0x14000000000000,
        0x28000000000000, 0x50000000000000, 0xa0000000000000, 0x40000000000000,
        0x200000000000000, 0x500000000000000, 0xa00000000000000, 0x1400000000000000,
        0x2800000000000000, 0x5000000000000000, 0xa000000000000000, 0x4000000000000000,
        0x0, 0x0, 0x0, 0x0,
        0x0, 0x0, 0x0, 0x0,
    },
    {
        0x0, 0x0, 0x0, 0x0,
        0x0, 0x0, 0x0, 0x0,
        0x2, 0x5, 0xa, 0x14,
        0x28, 0x50, 0xa0, 0x40,
        0x200, 0x500, 0xa00, 0x1400,
        0x2800, 0x5000, 0xa000, 0x4000,
        0x20000, 0x50000, 0xa0000, 0x140000,
        0x280000, 0x500000, 0xa00000, 0x400000,
        0x2000000, 0x5000000, 0xa000000, 0x14000000,
        0x28000000, 0x50000000, 0xa0000000, 0x40000000,
        0x200000000, 0x500000000, 0xa00000000, 0x1400000000,
        0x2800000000, 0x5000000000, 0xa000000000, 0x4000000000,
        0x20000000000, 0x50000000000, 0xa0000000000, 0x140000000000,
        0x280000000000, 0x500000000000, 0xa00000000000, 0x400000000000,
        0x2000000000000, 0x5000000000000, 0xa000000000000, 0x14000000000000,
        0x28000000000000, 0x50000000000000, 0xa0000000000000, 0x40000000000000,
    }
};

constexpr int bishop_relevant_bits[64] = {
    6, 5, 5, 5, 5, 5, 5, 6,
    5, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 7, 7, 7, 7, 5, 5,
    5, 5, 7, 9, 9, 7, 5, 5,
    5, 5, 7, 9, 9, 7, 5, 5,
    5, 5, 7, 7, 7, 7, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5,
    6, 5, 5, 5, 5, 5, 5, 6
};

constexpr int rook_relevant_bits[64] = {
    12, 11, 11, 11, 11, 11, 11, 12,
    11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11,
    12, 11, 11, 11, 11, 11, 11, 12
};

// bits REELLEMENT utilises par le jeu de magics ci-dessous ("Best Magics so far")
constexpr int bishop_used_bits[64] = {
     5,  4,  5,  5,  5,  5,  4,  5,
     4,  4,  5,  5,  5,  5,  4,  4,
     4,  4,  7,  7,  7,  7,  4,  4,
     5,  5,  7,  9,  9,  7,  5,  5,
     5,  5,  7,  9,  9,  7,  5,  5,
     4,  4,  7,  7,  7,  7,  4,  4,
     4,  4,  5,  5,  5,  5,  4,  4,
     5,  4,  5,  5,  5,  5,  4,  5,
};

constexpr int rook_used_bits[64] = {
    12, 11, 11, 11, 11, 11, 11, 12,
    11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11,
    10,  9,  9,  9,  9,  9,  9, 10,
    11, 10, 10, 10, 10, 11, 10, 11,
};

// bishop_mask , table précalculée avec le programme Magic
constexpr Bitboard bishop_masks[64] {
    0x0040201008040200ULL, 0x0000402010080400ULL, 0x0000004020100a00ULL, 0x0000000040221400ULL,
    0x0000000002442800ULL, 0x0000000204085000ULL, 0x0000020408102000ULL, 0x0002040810204000ULL,
    0x0020100804020000ULL, 0x0040201008040000ULL, 0x00004020100a0000ULL, 0x0000004022140000ULL,
    0x0000000244280000ULL, 0x0000020408500000ULL, 0x0002040810200000ULL, 0x0004081020400000ULL,
    0x0010080402000200ULL, 0x0020100804000400ULL, 0x004020100a000a00ULL, 0x0000402214001400ULL,
    0x0000024428002800ULL, 0x0002040850005000ULL, 0x0004081020002000ULL, 0x0008102040004000ULL,
    0x0008040200020400ULL, 0x0010080400040800ULL, 0x0020100a000a1000ULL, 0x0040221400142200ULL,
    0x0002442800284400ULL, 0x0004085000500800ULL, 0x0008102000201000ULL, 0x0010204000402000ULL,
    0x0004020002040800ULL, 0x0008040004081000ULL, 0x00100a000a102000ULL, 0x0022140014224000ULL,
    0x0044280028440200ULL, 0x0008500050080400ULL, 0x0010200020100800ULL, 0x0020400040201000ULL,
    0x0002000204081000ULL, 0x0004000408102000ULL, 0x000a000a10204000ULL, 0x0014001422400000ULL,
    0x0028002844020000ULL, 0x0050005008040200ULL, 0x0020002010080400ULL, 0x0040004020100800ULL,
    0x0000020408102000ULL, 0x0000040810204000ULL, 0x00000a1020400000ULL, 0x0000142240000000ULL,
    0x0000284402000000ULL, 0x0000500804020000ULL, 0x0000201008040200ULL, 0x0000402010080400ULL,
    0x0002040810204000ULL, 0x0004081020400000ULL, 0x000a102040000000ULL, 0x0014224000000000ULL,
    0x0028440200000000ULL, 0x0050080402000000ULL, 0x0020100804020000ULL, 0x0040201008040200ULL,
};

// rook_mask , table précalculée avec le programme Magic
constexpr Bitboard rook_masks[64] {
    0x000101010101017eULL, 0x000202020202027cULL, 0x000404040404047aULL, 0x0008080808080876ULL,
    0x001010101010106eULL, 0x002020202020205eULL, 0x004040404040403eULL, 0x008080808080807eULL,
    0x0001010101017e00ULL, 0x0002020202027c00ULL, 0x0004040404047a00ULL, 0x0008080808087600ULL,
    0x0010101010106e00ULL, 0x0020202020205e00ULL, 0x0040404040403e00ULL, 0x0080808080807e00ULL,
    0x00010101017e0100ULL, 0x00020202027c0200ULL, 0x00040404047a0400ULL, 0x0008080808760800ULL,
    0x00101010106e1000ULL, 0x00202020205e2000ULL, 0x00404040403e4000ULL, 0x00808080807e8000ULL,
    0x000101017e010100ULL, 0x000202027c020200ULL, 0x000404047a040400ULL, 0x0008080876080800ULL,
    0x001010106e101000ULL, 0x002020205e202000ULL, 0x004040403e404000ULL, 0x008080807e808000ULL,
    0x0001017e01010100ULL, 0x0002027c02020200ULL, 0x0004047a04040400ULL, 0x0008087608080800ULL,
    0x0010106e10101000ULL, 0x0020205e20202000ULL, 0x0040403e40404000ULL, 0x0080807e80808000ULL,
    0x00017e0101010100ULL, 0x00027c0202020200ULL, 0x00047a0404040400ULL, 0x0008760808080800ULL,
    0x00106e1010101000ULL, 0x00205e2020202000ULL, 0x00403e4040404000ULL, 0x00807e8080808000ULL,
    0x007e010101010100ULL, 0x007c020202020200ULL, 0x007a040404040400ULL, 0x0076080808080800ULL,
    0x006e101010101000ULL, 0x005e202020202000ULL, 0x003e404040404000ULL, 0x007e808080808000ULL,
    0x7e01010101010100ULL, 0x7c02020202020200ULL, 0x7a04040404040400ULL, 0x7608080808080800ULL,
    0x6e10101010101000ULL, 0x5e20202020202000ULL, 0x3e40404040404000ULL, 0x7e80808080808000ULL,
};

// nombres magiques du fou
constexpr U64 bishop_magics[64] {
    0xFFEDF9FD7CFCFFFFULL, 0xFC0962854A77F576ULL, 0x5822022042000000ULL, 0x2CA804A100200020ULL,
    0x0204042200000900ULL, 0x2002121024000002ULL, 0xFC0A66C64A7EF576ULL, 0x7FFDFDFCBD79FFFFULL,
    0xFC0846A64A34FFF6ULL, 0xFC087A874A3CF7F6ULL, 0x1001080204002100ULL, 0x1810080489021800ULL,
    0x0062040420010A00ULL, 0x5028043004300020ULL, 0xFC0864AE59B4FF76ULL, 0x3C0860AF4B35FF76ULL,
    0x73C01AF56CF4CFFBULL, 0x41A01CFAD64AAFFCULL, 0x040C0422080A0598ULL, 0x4228020082004050ULL,
    0x0200800400E00100ULL, 0x020B001230021040ULL, 0x7C0C028F5B34FF76ULL, 0xFC0A028E5AB4DF76ULL,
    0x0020208050A42180ULL, 0x001004804B280200ULL, 0x2048020024040010ULL, 0x0102C04004010200ULL,
    0x020408204C002010ULL, 0x02411100020080C1ULL, 0x102A008084042100ULL, 0x0941030000A09846ULL,
    0x0244100800400200ULL, 0x4000901010080696ULL, 0x0000280404180020ULL, 0x0800042008240100ULL,
    0x0220008400088020ULL, 0x04020182000904C9ULL, 0x0023010400020600ULL, 0x0041040020110302ULL,
    0xDCEFD9B54BFCC09FULL, 0xF95FFA765AFD602BULL, 0x1401210240484800ULL, 0x0022244208010080ULL,
    0x1105040104000210ULL, 0x2040088800C40081ULL, 0x43FF9A5CF4CA0C01ULL, 0x4BFFCD8E7C587601ULL,
    0xFC0FF2865334F576ULL, 0xFC0BF6CE5924F576ULL, 0x80000B0401040402ULL, 0x0020004821880A00ULL,
    0x8200002022440100ULL, 0x0009431801010068ULL, 0xC3FFB7DC36CA8C89ULL, 0xC3FF8A54F4CA2C89ULL,
    0xFFFFFCFCFD79EDFFULL, 0xFC0863FCCB147576ULL, 0x040C000022013020ULL, 0x2000104000420600ULL,
    0x0400000260142410ULL, 0x0800633408100500ULL, 0xFC087E8E4BB2F736ULL, 0x43FF9E4EF4CA2C89ULL,
};

// nombres magiques de la tour
constexpr U64 rook_magics[64] {
    0xA180022080400230ULL, 0x0040100040022000ULL, 0x0080088020001002ULL, 0x0080080280841000ULL,
    0x4200042010460008ULL, 0x04800A0003040080ULL, 0x0400110082041008ULL, 0x008000A041000880ULL,
    0x10138001A080C010ULL, 0x0000804008200480ULL, 0x00010011012000C0ULL, 0x0022004128102200ULL,
    0x000200081201200CULL, 0x202A001048460004ULL, 0x0081000100420004ULL, 0x4000800380004500ULL,
    0x0000208002904001ULL, 0x0090004040026008ULL, 0x0208808010002001ULL, 0x2002020020704940ULL,
    0x8048010008110005ULL, 0x6820808004002200ULL, 0x0A80040008023011ULL, 0x00B1460000811044ULL,
    0x4204400080008EA0ULL, 0xB002400180200184ULL, 0x2020200080100380ULL, 0x0010080080100080ULL,
    0x2204080080800400ULL, 0x0000A40080360080ULL, 0x02040604002810B1ULL, 0x008C218600004104ULL,
    0x8180004000402000ULL, 0x488C402000401001ULL, 0x4018A00080801004ULL, 0x1230002105001008ULL,
    0x8904800800800400ULL, 0x0042000C42003810ULL, 0x008408110400B012ULL, 0x0018086182000401ULL,
    0x2240088020C28000ULL, 0x001001201040C004ULL, 0x0A02008010420020ULL, 0x0010003009010060ULL,
    0x0004008008008014ULL, 0x0080020004008080ULL, 0x0282020001008080ULL, 0x50000181204A0004ULL,
    0x48FFFE99FECFAA00ULL, 0x48FFFE99FECFAA00ULL, 0x497FFFADFF9C2E00ULL, 0x613FFFDDFFCE9200ULL,
    0xFFFFFFE9FFE7CE00ULL, 0xFFFFFFF5FFF3E600ULL, 0x0003FF95E5E6A4C0ULL, 0x510FFFF5F63C96A0ULL,
    0xEBFFFFB9FF9FC526ULL, 0x61FFFEDDFEEDAEAEULL, 0x53BFFFEDFFDEB1A2ULL, 0x127FFFB9FFDFB5F6ULL,
    0x411FFFDDFFDBF4D6ULL, 0x0801000804000603ULL, 0x0003FFEF27EEBE74ULL, 0x7645FFFECBFEA79EULL,
};


//======================================================
//  Tables d'attaques compactes ("fancy magic")
//
//  Un seul tableau 1D par pièce, et un offset par case donnant le
//  début de son bloc.
//
//  https://www.chessprogramming.org/Magic_Bitboards
//  https://www.chessprogramming.org/Best_Magics_so_far
//
//======================================================

//=========================================================
//! \brief  offsets de début de bloc ; l'élément 64 vaut la taille de la table
//---------------------------------------------------------
constexpr std::array<U32, N_SQUARES + 1> make_attack_offsets(const int (&bits)[N_SQUARES])
{
    std::array<U32, N_SQUARES + 1> offsets{};
    for (SQUARE sq = 0; sq < N_SQUARES; ++sq)
        offsets[sq + 1] = offsets[sq] + (1u << bits[sq]);
    return offsets;
}

#if defined USE_PEXT
constexpr auto bishop_offsets = make_attack_offsets(bishop_relevant_bits);
constexpr auto rook_offsets   = make_attack_offsets(rook_relevant_bits);
#else
constexpr auto bishop_offsets = make_attack_offsets(bishop_used_bits);
constexpr auto rook_offsets   = make_attack_offsets(rook_used_bits);
#endif

constexpr size_t BISHOP_ATTACKS_SIZE = bishop_offsets[N_SQUARES];   // magic  4 800 / PEXT   5 248
constexpr size_t ROOK_ATTACKS_SIZE   = rook_offsets[N_SQUARES];     // magic 88 064 / PEXT 102 400

// table d'attaques du fou, bloc de la case sq en [bishop_offsets[sq], bishop_offsets[sq+1])
alignas(64) extern Bitboard BISHOP_ATTACKS[BISHOP_ATTACKS_SIZE];

// table d'attaques de la tour : idem avec rook_offsets
alignas(64) extern Bitboard ROOK_ATTACKS[ROOK_ATTACKS_SIZE];


//======================================================
//  Métadonnées magic regroupées par case
//======================================================

#if defined USE_PEXT

//! \brief  Données propre à une case, suffisant pour obtenir les attaques d'un fou ou d'une tour depuis cette case.
struct alignas(16) Magic
{
    Bitboard           mask;      // cases pertinentes (hors bords)
    const Bitboard*    attacks;   // début du bloc de la case dans la table

    [[nodiscard]] inline Bitboard attacks_of(const Bitboard occupied) const noexcept
    {
        return attacks[_pext_u64(occupied, mask)];
    }
};

static_assert(sizeof(Magic) == 16, "Magic doit tenir en un quart de ligne de cache");

//! \brief  Construit les Magic des 64 cases d'un type de glisseur
constexpr std::array<Magic, N_SQUARES> make_magics(const Bitboard (&masks)[N_SQUARES],
                                                   const U64      (&)[N_SQUARES],
                                                   const int      (&)[N_SQUARES],
                                                   const std::array<U32, N_SQUARES + 1>& offsets,
                                                   const Bitboard* table)
{
    std::array<Magic, N_SQUARES> res{};
    for (SQUARE sq = 0; sq < N_SQUARES; ++sq)
        res[sq] = Magic{masks[sq], table + offsets[sq]};
    return res;
}

#else

//! \brief  Données propre à une case, suffisant pour obtenir les attaques d'un fou ou d'une tour depuis cette case.
struct alignas(32) Magic
{
    Bitboard           mask;      // cases pertinentes (hors bords)
    U64                magic;     // nombre magique
    const Bitboard*    attacks;   // début du bloc de la case dans la table
    U64                shift;     // 64 - used_bits (U64 : complète à 32 octets)

    [[nodiscard]] inline Bitboard attacks_of(const Bitboard occupied) const noexcept
    {
        return attacks[((occupied & mask) * magic) >> shift];
    }
};

static_assert(sizeof(Magic) == 32, "Magic doit tenir en une demi-ligne de cache");

//! \brief  Construit les Magic des 64 cases d'un type de glisseur
constexpr std::array<Magic, N_SQUARES> make_magics(const Bitboard (&masks)[N_SQUARES],
                                                   const U64      (&magics)[N_SQUARES],
                                                   const int      (&used_bits)[N_SQUARES],
                                                   const std::array<U32, N_SQUARES + 1>& offsets,
                                                   const Bitboard* table)
{
    std::array<Magic, N_SQUARES> res{};
    for (SQUARE sq = 0; sq < N_SQUARES; ++sq)
        res[sq] = Magic{masks[sq], magics[sq], table + offsets[sq],
                        static_cast<U64>(64 - used_bits[sq])};
    return res;
}

#endif

alignas(64) inline constexpr auto BISHOP_MAGIC = make_magics(bishop_masks, bishop_magics, bishop_used_bits,
                                                             bishop_offsets, BISHOP_ATTACKS);
alignas(64) inline constexpr auto ROOK_MAGIC   = make_magics(rook_masks,   rook_magics,   rook_used_bits,
                                                             rook_offsets,   ROOK_ATTACKS);

//! \brief  l'indice PEXT a popcount(mask) bits : il doit tenir dans le bloc
constexpr bool check_masks(const std::array<Magic, N_SQUARES>& magic,
                           const int (&bits)[N_SQUARES])
{
    for (SQUARE sq = 0; sq < N_SQUARES; ++sq)
        if (std::popcount(magic[sq].mask) != bits[sq])
            return false;
    return true;
}

//! \brief  un bloc réduit ne peut pas dépasser le bloc plein
constexpr bool check_used_bits(const int (&used)[N_SQUARES],
                               const int (&bits)[N_SQUARES])
{
    for (SQUARE sq = 0; sq < N_SQUARES; ++sq)
        if (used[sq] > bits[sq])
            return false;
    return true;
}

static_assert(check_masks(BISHOP_MAGIC, bishop_relevant_bits));
static_assert(check_masks(ROOK_MAGIC,   rook_relevant_bits));
static_assert(check_used_bits(bishop_used_bits, bishop_relevant_bits));
static_assert(check_used_bits(rook_used_bits,   rook_relevant_bits));

//======================================================
//! \brief  Donne l'attaque du pion (pas le déplacement) pour la couleur C
//!
//! \param[in]  sq  case occupée par le pion
//!
//! \return Bitboard des cases attaquées par le pion
//------------------------------------------------------
template <Color C>
[[nodiscard]] constexpr Bitboard pawn_attacks(const SQUARE sq) noexcept {
    assert(SQ::is_ok(sq));
    return(PAWN_ATTACKS[C][sq]);
}

//======================================================
//! \brief  Donne l'attaque du pion (pas le déplacement) pour la couleur C
//!         (variante avec couleur passée en paramètre d'exécution)
//!
//! \param[in]  C   couleur du pion (0 = WHITE, 1 = BLACK)
//! \param[in]  sq  case occupée par le pion
//!
//! \return Bitboard des cases attaquées par le pion
//------------------------------------------------------
[[nodiscard]] constexpr Bitboard pawn_attacks(const int C, const SQUARE sq) noexcept {
    assert(SQ::is_ok(sq));
    return(PAWN_ATTACKS[C][sq]);
}


//======================================================
//! \brief  Donne les cases attaquées par un cavalier
//!
//! \param[in]  sq  case occupée par le cavalier
//!
//! \return Bitboard des cases attaquées
//------------------------------------------------------
[[nodiscard]] constexpr Bitboard knight_moves(const SQUARE sq) noexcept {
    assert(SQ::is_ok(sq));
    return(KNIGHT_ATTACKS[sq]);
}

//======================================================
//! \brief  Donne les cases attaquées par un roi
//!
//! \param[in]  sq  case occupée par le roi
//!
//! \return Bitboard des cases attaquées
//------------------------------------------------------
[[nodiscard]] constexpr Bitboard king_moves(const SQUARE sq) noexcept {
    assert(SQ::is_ok(sq));
    return(KING_ATTACKS[sq]);
}

//======================================================
//! \brief  Donne les cases attaquées par un fou, via les tables
//!         magic bitboards (ou PEXT)
//!
//! \param[in]  sq        case occupée par le fou
//! \param[in]  occupied  bitboard de toutes les cases occupées
//!
//! \return Bitboard des cases attaquées
//------------------------------------------------------
[[nodiscard]] inline U64 bishop_moves(const SQUARE sq, const U64 occupied) noexcept {
    assert(SQ::is_ok(sq));
    return BISHOP_MAGIC[sq].attacks_of(occupied);
}

//======================================================
//! \brief  Donne les cases attaquées par une tour, via les tables
//!         magic bitboards (ou PEXT)
//!
//! \param[in]  sq        case occupée par la tour
//! \param[in]  occupied  bitboard de toutes les cases occupées
//!
//! \return Bitboard des cases attaquées
//------------------------------------------------------
[[nodiscard]] inline U64 rook_moves(const SQUARE sq, const U64 occupied) noexcept {
    assert(SQ::is_ok(sq));
    return ROOK_MAGIC[sq].attacks_of(occupied);
}


//======================================================
//! \brief  Donne les cases attaquées par une dame
//!         (union des attaques tour et fou)
//!
//! \param[in]  sq        case occupée par la dame
//! \param[in]  occupied  bitboard de toutes les cases occupées
//!
//! \return Bitboard des cases attaquées
//------------------------------------------------------
[[nodiscard]] inline U64 queen_moves(const SQUARE sq, const U64 occupied) noexcept {
    assert(SQ::is_ok(sq));
    return(Attacks::rook_moves(sq, occupied) | Attacks::bishop_moves(sq, occupied));
}

//======================================================
//! \brief attacks_bb(Square, Bitboard) retourne les attaques de la pièce donnée,
//! en supposant l'échiquier occupé selon le Bitboard passé en paramètre.
//! Les attaques des pièces glissantes ne continuent pas au-delà d'une case occupée.
//!
//! \param[in]  sq        case occupée par la pièce
//! \param[in]  occupied  bitboard de toutes les cases occupées
//!
//! \return Bitboard des cases attaquées
//------------------------------------------------------
template<PieceType Pt>
inline Bitboard attacks_bb(SQUARE sq, Bitboard occupied) noexcept
{
    assert((Pt != PieceType::PAWN) && (SQ::is_ok(sq)));

    if constexpr (Pt == PieceType::KNIGHT)
        return Attacks::knight_moves(sq);
    else if constexpr (Pt == PieceType::BISHOP)
        return Attacks::bishop_moves(sq, occupied);
    else if constexpr (Pt == PieceType::ROOK)
        return Attacks::rook_moves(sq, occupied);
    else if constexpr (Pt == PieceType::QUEEN)
        return Attacks::bishop_moves(sq, occupied) | Attacks::rook_moves(sq, occupied);
    else if constexpr (Pt == PieceType::KING)
        return Attacks::king_moves(sq);
    else
        return 0;
}

//======================================================
//! \brief  Donne les cases attaquées par une pièce, type donné en paramètre
//!         d'exécution (dispatch runtime, cf. attacks_bb<Pt> pour la
//!         version template)
//!
//! \param[in]  pt        type de la pièce
//! \param[in]  sq        case occupée par la pièce
//! \param[in]  occupied  bitboard de toutes les cases occupées
//!
//! \return Bitboard des cases attaquées
//------------------------------------------------------
inline Bitboard attacks_bb(PieceType pt, SQUARE sq, Bitboard occupied) noexcept
{
    assert((pt != PieceType::PAWN) && (SQ::is_ok(sq)));

    switch (pt)
    {
    case PieceType::KNIGHT:
        return Attacks::knight_moves(sq);
    case PieceType::BISHOP:
        return Attacks::bishop_moves(sq, occupied);
    case PieceType::ROOK:
        return Attacks::rook_moves(sq, occupied);
    case PieceType::QUEEN:
        return Attacks::bishop_moves(sq, occupied) | Attacks::rook_moves(sq, occupied);
    case PieceType::KING:
        return Attacks::king_moves(sq);

    case PieceType::PAWN:
    case PieceType::NONE:
        return 0;

    default:
        return 0;
    }
}
Bitboard set_occupancy(int index, int bits_in_mask, Bitboard attack_mask);
Bitboard bishop_attacks_on_the_fly(SQUARE sq, Bitboard block);
void     init_bishop_attacks();
Bitboard rook_attacks_on_the_fly(SQUARE sq, Bitboard block);
void     init_rook_attacks();

void init_masks();


}
#endif // ATTACKS_H
