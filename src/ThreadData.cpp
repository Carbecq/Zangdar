#include "ThreadData.h"

ThreadData::ThreadData()
{

}

//==========================================
//! \brief  Evaluation de la position
//! \return Evaluation statique de la position
//! du point de vue du camp au trait.
//------------------------------------------
[[nodiscard]] int ThreadData::evaluate(const Board& board)
{
    // Du fait de lazy Updates, on a découpé la routine en 2

    Accumulator& acc = get_accumulator();

   // Lazy Updates
   nnue->lazy_updates(board, acc);

   return do_evaluate(board, acc);
}

//==========================================
//! \brief  Evaluation de la position
//! \return Evaluation statique de la position
//! du point de vue du camp au trait.
//------------------------------------------
[[nodiscard]] int ThreadData::do_evaluate(const Board& board, Accumulator& acc)
{
    int value;
    if (board.side_to_move == WHITE)
        value = nnue->evaluate<WHITE>(acc, BB::count_bit(board.occupancy_all()));
    else
        value = nnue->evaluate<BLACK>(acc, BB::count_bit(board.occupancy_all()));

    constexpr int PawnScore    =   2;
    constexpr int KnightScore  =   3;
    constexpr int BishopScore  =   3;
    constexpr int RookScore    =   5;
    constexpr int QueenScore   =  12;
    constexpr int ScoreBias    = 230;
    constexpr int ScoreDivisor = 330;

    int phase = PawnScore   * BB::count_bit(board.occupancy_p<PieceType::PAWN>())
              + KnightScore * BB::count_bit(board.occupancy_p<PieceType::KNIGHT>())
              + BishopScore * BB::count_bit(board.occupancy_p<PieceType::BISHOP>())
              + RookScore   * BB::count_bit(board.occupancy_p<PieceType::ROOK>())
              + QueenScore  * BB::count_bit(board.occupancy_p<PieceType::QUEEN>());

    value = value * (ScoreBias + phase) / ScoreDivisor;

    // Make sure the evaluation does not mix with guaranteed win/loss scores
    value = std::clamp(value, -TBWIN_IN_X + 1, TBWIN_IN_X - 1);

    return value;
}

//==================================================
//! \brief  Réalise un coup
//! On met à jour à la fois la position, et le réseau
//--------------------------------------------------
template <Color US, bool Update_NNUE>
void ThreadData::make_move(Board& board, const MOVE move) noexcept
{
    // Lazy Updates
    // On ne stocke que les modifications à faire,
    // mais on ne les applique pas.

    if constexpr (Update_NNUE == true)
    {
        nnue->push();    // nouvel accumulateur
    }
    Accumulator& accum = nnue->get_accumulator();

    // Update de la position
    board.make_move<US, Update_NNUE>(accum, move);

    // Push the accumulator change
    accum.updated[WHITE] = false;
    accum.updated[BLACK] = false;
    accum.king_square    = board.king_square;
}

//=============================================================
//! \brief  Enlève un coup
//! On met à jour à la fois la position, et le réseau
//-------------------------------------------------------------
template <Color US, bool Update_NNUE>
void ThreadData::undo_move(Board& board) noexcept
{
    // printf("------------------------------------------undo move \n");
    // std::cout << display() << std::endl << std::endl;

    if constexpr (Update_NNUE == true)
        nnue->pop(); // retourne a l'accumulateur précédent

    board.undo_move<US>();
}

// Explicit instantiations.
template void ThreadData::make_move<WHITE, true>(Board& board, const U32 move) noexcept;
template void ThreadData::make_move<BLACK, true>(Board& board, const U32 move) noexcept;
template void ThreadData::make_move<WHITE, false>(Board& board, const U32 move) noexcept;
template void ThreadData::make_move<BLACK, false>(Board& board, const U32 move) noexcept;

template void ThreadData::undo_move<WHITE, true>(Board& board) noexcept ;
template void ThreadData::undo_move<BLACK, true>(Board& board) noexcept ;
template void ThreadData::undo_move<WHITE, false>(Board& board) noexcept ;
template void ThreadData::undo_move<BLACK, false>(Board& board) noexcept ;
