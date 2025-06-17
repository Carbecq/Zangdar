#include <assert.h>
#include "Cuckoo.h"
#include "defines.h"
#include "Move.h"
#include "Bitboard.h"
#include "types.h"
#include "Attacks.h"
#include "zobrist.h"

namespace Cuckoo
{

std::array<U64, 8192>  keys{0};
std::array<MOVE, 8192> moves{0};

void init()
{
    [[maybe_unused]] U32 count = 0;

    // skip pawns
    for (Piece piece : { Piece::BLACK_KNIGHT, Piece::BLACK_BISHOP, Piece::BLACK_ROOK, Piece::BLACK_QUEEN, Piece::BLACK_KING,
         Piece::WHITE_KNIGHT, Piece::WHITE_BISHOP, Piece::WHITE_ROOK, Piece::WHITE_QUEEN, Piece::WHITE_KING })
    {
        const int p = static_cast<int>(piece);

        for (U32 from = 0; from < N_SQUARES; from++)
        {
            for (U32 dest = from + 1; dest < N_SQUARES; dest++)
            {
                if (BB::get_bit(Attacks::attacks_bb(Move::type(piece), from, 0), dest))
                {
                    MOVE move = Move::CODE(from, dest, piece, Piece::NONE, Piece::NONE, 0);
                    U64  key  = piece_key[p][from] ^ piece_key[p][dest] ^ side_key;

                    U32 slot = h1(key);

                    while (true)
                    {
                        std::swap(keys[slot], key);
                        std::swap(moves[slot], move);

                        if (move == 0)
                            break;

                        slot = (slot == h1(key)) ? h2(key) : h1(key);
                    }

                    count++;
                }
            }
        }
    }

    assert(count == 3668);
}

} // namespace
