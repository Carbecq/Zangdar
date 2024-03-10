#ifndef ATTACKS_H
#define ATTACKS_H

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

//A lookup table for knight move bitboards
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

//A lookup table for white pawn move bitboards
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

// bishop relevant occupancy bit count for every square on board
//  on utilise en fait le shift = 64 - relevant_bit
constexpr int bishop_shifts[64] {
    58, 59, 59, 59, 59, 59, 59, 58,
    59, 59, 59, 59, 59, 59, 59, 59,
    59, 59, 57, 57, 57, 57, 59, 59,
    59, 59, 57, 55, 55, 57, 59, 59,
    59, 59, 57, 55, 55, 57, 59, 59,
    59, 59, 57, 57, 57, 57, 59, 59,
    59, 59, 59, 59, 59, 59, 59, 59,
    58, 59, 59, 59, 59, 59, 59, 58,
};

// rook relevant occupancy bit count for every square on board
//  on utilise en fait le shift = 64 - relevant_bit
constexpr int rook_shifts[64] {
    52, 53, 53, 53, 53, 53, 53, 52,
    53, 54, 54, 54, 54, 54, 54, 53,
    53, 54, 54, 54, 54, 54, 54, 53,
    53, 54, 54, 54, 54, 54, 54, 53,
    53, 54, 54, 54, 54, 54, 54, 53,
    53, 54, 54, 54, 54, 54, 54, 53,
    53, 54, 54, 54, 54, 54, 54, 53,
    52, 53, 53, 53, 53, 53, 53, 52,
};

// bishop magic numbers
constexpr U64 bishop_magics[64] {
    0x40106000a1160020ULL, 0x0020010250810120ULL, 0x2010010220280081ULL, 0x002806004050c040ULL,
    0x0002021018000000ULL, 0x2001112010000400ULL, 0x0881010120218080ULL, 0x1030820110010500ULL,
    0x0210401044110050ULL, 0x0020080101140100ULL, 0x0c00501100610400ULL, 0x05c051104a000d80ULL,
    0x8602020210008000ULL, 0x02882a0886180000ULL, 0x8000204124104001ULL, 0x0080012608025800ULL,
    0x0010006020322084ULL, 0x000285202a060210ULL, 0x0008003000204114ULL, 0x4000800802004000ULL,
    0x8002100401200040ULL, 0x8006010c11008802ULL, 0x030e200401010840ULL, 0x000080010b829021ULL,
    0x4205040860200461ULL, 0x4004840020015400ULL, 0x01880c0002040491ULL, 0x0002008020080880ULL,
    0x0008840002020204ULL, 0x0008020200405200ULL, 0x040a821904210400ULL, 0x000285202a060210ULL,
    0x4104100440400501ULL, 0x6000841084045004ULL, 0x0016004040040308ULL, 0x0800020080080081ULL,
    0x8804080200002008ULL, 0x00108804800b1000ULL, 0x040404040400b088ULL, 0x080413003142440aULL,
    0x1112080340040900ULL, 0x04028404a0000282ULL, 0x4402020602040100ULL, 0x4000012018001100ULL,
    0x0010080104044040ULL, 0x2040010400200100ULL, 0x0204280084118104ULL, 0x2010010220280081ULL,
    0x0881010120218080ULL, 0x02002c0c02080820ULL, 0x48820aa108280002ULL, 0x4080400042020020ULL,
    0x50600011120a1000ULL, 0x0000602002822400ULL, 0x0020080101140100ULL, 0x0020010250810120ULL,
    0x1030820110010500ULL, 0x0080012608025800ULL, 0x0002810084008800ULL, 0x800080000c208800ULL,
    0xa408002140028204ULL, 0x0010006020322084ULL, 0x0210401044110050ULL, 0x40106000a1160020ULL,
};

// rook magic numbers
constexpr U64 rook_magics[64] {
    0x0a80004000801220ULL, 0x8040004010002008ULL, 0x2080200010008008ULL, 0x1100100008210004ULL,
    0xc200209084020008ULL, 0x2100010004000208ULL, 0x0400081000822421ULL, 0x0200010422048844ULL,
    0x0202800084204008ULL, 0x0801004001002080ULL, 0x2241002000150442ULL, 0x2509002100089004ULL,
    0x1421000500080011ULL, 0x1241000900820400ULL, 0x6031000200040100ULL, 0x0a0500008100084aULL,
    0x0000888004400020ULL, 0x00088e8020004000ULL, 0x000a020010248040ULL, 0x2210010009001020ULL,
    0x0800808004000800ULL, 0x2000808004000200ULL, 0x00200400891a0850ULL, 0x0002220000408124ULL,
    0x1240400280208000ULL, 0x8000400100208100ULL, 0x0000100080802000ULL, 0x0800100100082101ULL,
    0x1421000500080011ULL, 0x0402040080800200ULL, 0x2122000200810804ULL, 0x40a1010200007084ULL,
    0x29404000a1800180ULL, 0x0801004001002080ULL, 0x0000100080802000ULL, 0x2000102042000a00ULL,
    0x0000800800800400ULL, 0x0402040080800200ULL, 0x0000020104000810ULL, 0x2900040062000081ULL,
    0x3780002000444000ULL, 0x0000500020004000ULL, 0x2010004020010100ULL, 0x880200200c420010ULL,
    0x0010040008008080ULL, 0x0006000804010100ULL, 0x00601021060c0098ULL, 0x0001003088410002ULL,
    0x29404000a1800180ULL, 0x8400824000200180ULL, 0x3000801000200080ULL, 0x0200080081500180ULL,
    0x0010040008008080ULL, 0x1001000208040100ULL, 0x0006000401080200ULL, 0x0200800100106080ULL,
    0x00008002204a1101ULL, 0x1040090010224081ULL, 0x4300c0200011000dULL, 0x8002041001002009ULL,
    0x2005000800020411ULL, 0x110a008408100102ULL, 0x0006000108008402ULL, 0x0200002900884402ULL,
};


// bishop attacks table [square][occupancies]
extern Bitboard BISHOP_ATTACKS[64][512];

// rook attacks table [square][occupancies]
extern Bitboard ROOK_ATTACKS[64][4096];

// donne l'attaque du pion, pas le déplacement
template <Color C>
[[nodiscard]] constexpr Bitboard pawn_attacks(const int sq) {
    return(PAWN_ATTACKS[C][sq]);
}
[[nodiscard]] constexpr Bitboard pawn_attacks(const int C, const int sq) {
    return(PAWN_ATTACKS[C][sq]);
}


[[nodiscard]] constexpr Bitboard knight_moves(const int sq) {
    return(KNIGHT_ATTACKS[sq]);
}

[[nodiscard]] constexpr Bitboard king_moves(const int sq) {
    return(KING_ATTACKS[sq]);
}

[[nodiscard]] inline U64 bishop_moves(const int sq, const U64 occupied) {
#if defined USE_PEXT
    return BISHOP_ATTACKS[sq][static_cast<int>(_pext_u64(occupied, bishop_masks[sq]))];
#else
    return BISHOP_ATTACKS[sq][static_cast<int>((occupied & bishop_masks[sq]) * bishop_magics[sq]
                                               >> (bishop_shifts[sq]))];
#endif
}

[[nodiscard]] inline U64 rook_moves(const int sq, const U64 occupied) {
#if defined USE_PEXT
    return ROOK_ATTACKS[sq][static_cast<int>(_pext_u64(occupied, rook_masks[sq]))];
#else
    return ROOK_ATTACKS[sq][static_cast<int>((occupied & rook_masks[sq]) * rook_magics[sq]
                                             >> (rook_shifts[sq]))];
#endif
}


[[nodiscard]] inline U64 queen_moves(const int sq, const U64 occupied) {
    return(Attacks::rook_moves(sq, occupied) | Attacks::bishop_moves(sq, occupied));
}

Bitboard set_occupancy(int index, int bits_in_mask, Bitboard attack_mask);
Bitboard bishop_attacks_on_the_fly(int sq, Bitboard block);
void     init_bishop_attacks();
Bitboard rook_attacks_on_the_fly(int sq, Bitboard block);
void     init_rook_attacks();

void init_masks();


}
#endif // ATTACKS_H
