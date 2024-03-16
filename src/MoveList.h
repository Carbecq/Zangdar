#ifndef MOVELIST_H
#define MOVELIST_H

class MoveList;

#include <array>
#include "defines.h"

struct MLMove {
    MOVE move;
    I32  value;
};

class MoveList
{
public:
    MoveList();

public:
    void clear() { count = 0; }
    size_t size() const { return count; }

    std::array<MLMove, MAX_MOVES> mlmoves;
    size_t                        count;


private:
};


#endif // MOVELIST_H
