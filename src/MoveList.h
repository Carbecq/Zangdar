#ifndef MOVELIST_H
#define MOVELIST_H

class MoveList;

#include <array>
#include "defines.h"
#include "types.h"
#include "Move.h"
#include "Bitboard.h"
#include "Board.h"

struct MLMove {
    MOVE move;
    I32  value;
};

class MoveList
{
public:
    MoveList() : count(0) {}

public:
    void clear() { count = 0; }
    size_t size() const { return count; }

    std::array<MLMove, MAX_MOVES> mlmoves;
    size_t                        count;



private:
};


#endif // MOVELIST_H
