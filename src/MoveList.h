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
    //! \brief  Construit une liste de coups vide
    MoveList() : count(0) {}

public:
    //! \brief  Vide la liste des coups
    void clear() { count = 0; }

    //! \brief  Retourne le nombre de coups stockés dans la liste
    size_t size() const { return count; }

    std::array<MLMove, MAX_MOVES> mlmoves;
    size_t                        count;



private:
};


#endif // MOVELIST_H
