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
    MoveList() : count(0) {};

public:
    void clear() { count = 0; }
    size_t size() const { return count; }

    std::array<MLMove, MAX_MOVES> mlmoves;
    size_t                        count;


    //=================================================================
    //! \brief  Ajoute un coup tranquille à la liste des coups
    //!
    //! \param[in]  from    position de départ de la pièce
    //! \param[in]  dest    position d'arrivée de la pièce
    //! \param[in]  piece   type de la pièce jouant
    //! \param[in]  flags   flags du coup (avance double, prise en-passant, roque)
    //-----------------------------------------------------------------
    inline void add_quiet_move(int from, int dest, PieceType piece, U32 flags)
    {
        mlmoves[count++].move = Move::CODE(from, dest, piece, NO_TYPE, NO_TYPE, flags);
    }

private:
};


#endif // MOVELIST_H
