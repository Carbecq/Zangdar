#include <iostream>
#include "Board.h"
#include "defines.h"
#include "Move.h"

template <Color C, bool divide=false>
[[nodiscard]] std::uint64_t Board::perft(const int depth) noexcept
{
    if (depth == 0)
        return 1;

    U64 nodes  = 0ULL;
    U32 move;

    MoveList ml;
    legal_moves<C>(ml);

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

        //        std::cout << "   " <<decal <<  Move::name(move) << " : " << std::endl;

        //        const auto to       = Move::dest(move);
        //        const auto from     = Move::from(move);
        //        const auto piece    = Move::piece(move);
        //        const auto captured = Move::captured(move);
        //        const auto promo    = Move::promotion(move);

        //        printf("move  = (%s) \n", Move::name(move).c_str());
        //        binary_print(move, "message");
        //        printf("from  = %s \n", square_name[from].c_str());
        //        printf("dest  = %s \n", square_name[to].c_str());
        //        printf("piece = %s \n", piece_name[piece].c_str());
        //        printf("capt  = %s \n", piece_name[captured].c_str());
        //        printf("promo = %s \n", piece_name[promo].c_str());
        //        printf("flags = %d \n", Move::flags(move));

        if constexpr (divide == false)
        {
            make_move<C>(move);
            nodes += perft<~C, false>(depth - 1);
            undo_move<C>();
        }
        else
        {
            make_move<C>(move);
            auto child = perft<~C, false>(depth - 1);
            undo_move<C>();
            std::cout << Move::name(move) << " : " << child << std::endl;
            nodes += child;
        }
    }

    return nodes;
}

// Explicit instantiations.
template std::uint64_t Board::perft<WHITE, true>(const int depth) noexcept;
template std::uint64_t Board::perft<WHITE, false>(const int depth) noexcept;
template std::uint64_t Board::perft<BLACK, true>(const int depth) noexcept;
template std::uint64_t Board::perft<BLACK, false>(const int depth) noexcept;

