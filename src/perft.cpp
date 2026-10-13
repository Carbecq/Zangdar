#include <iostream>
#include "Board.h"
#include "defines.h"
#include "Move.h"

//=======================================================
//! \brief  Dénombrement des positions atteignables (perft)
//!
//! Parcourt récursivement l'arbre des coups légaux jusqu'à
//! la profondeur donnée. Utilise le bulk-counting à depth==1.
//! Si divide==true, affiche le nombre de nœuds pour chaque
//! coup joué à la racine de l'appel.
//!
//! \param[in,out]  board   échiquier sur lequel les coups sont joués/annulés
//! \param[in]      depth   profondeur restante à explorer
//!
//! \return Nombre de nœuds (feuilles) dénombrés
//-------------------------------------------------------
template <Color C, bool divide=false>
[[nodiscard]] std::uint64_t perft(Board& board, const int depth) noexcept
{
    if (depth == 0)
        return 1;

    U64 nodes  = 0ULL;
    MOVE move;
    Accumulator accum;  // ne sert pas

    MoveList ml;
    board.legal_moves<C, MoveGenType::ALL>(ml);

    // on génère des coups légaux, on peut
    // faire un bulk-counting
    if (depth == 1)
        return ml.count;

    //    std::string decal = "";
    //    for (int k=0; k<depth; k++)
    //        decal += "  ";

    // ml.moves est un "array", il faut utiliser ml.count
    for (size_t index = 0; index < ml.count; index++)
    {
        move = (ml.mlmoves[index].move);

        if constexpr (divide == false)
        {
            board.make_move<C, false>(accum, move);
            nodes += perft<~C, false>(board, depth - 1);
            board.undo_move<C>();
        }
        else
        {
            board.make_move<C, false>(accum, move);
            auto child = perft<~C, false>(board, depth - 1);
            board.undo_move<C>();
            std::cout << Move::name(move) << " : " << child << std::endl;
            nodes += child;
        }
    }

    return nodes;
}

// Explicit instantiations.
template std::uint64_t perft<WHITE, true>(Board& board, const int depth) noexcept;
template std::uint64_t perft<WHITE, false>(Board& board, const int depth) noexcept;
template std::uint64_t perft<BLACK, true>(Board& board, const int depth) noexcept;
template std::uint64_t perft<BLACK, false>(Board& board, const int depth) noexcept;

