#include "Board.h"
#include "Move.h"
#include "types.h"
#include "NNUE.h"

//=============================================================
//! \brief  Enlève un coup
//-------------------------------------------------------------
template <Color Us, bool Update_NNUE>
void Board::undo_move() noexcept
{
    constexpr Color Them     = ~Us;

    // printf("------------------------------------------undo move \n");
    // std::cout << display() << std::endl << std::endl;

    if constexpr (Update_NNUE == true)
        nnue.pop(); // retourne a l'accumulateur précédent

    const MOVE move = get_status().move;

    // Swap sides
    side_to_move = ~side_to_move;

    const auto  dest     = Move::dest(move);
    const auto  from     = Move::from(move);
    const Piece piece    = Move::piece(move);
    const Piece captured = Move::captured(move);
    const Piece promoted = Move::promoted(move);

    // Déplacement du roi
    if (Move::type(piece) == PieceType::KING)
        king_square[Us] = from;

    //====================================================================================
    //  Coup normal (pas spécial)
    //------------------------------------------------------------------------------------
    if (Move::flags(move) == Move::FLAG_NONE)
    {
        //====================================================================================
        //  Déplacement simple
        //------------------------------------------------------------------------------------
        if (Move::is_depl(move))
        {
            move_piece(dest, from, Us, piece);
        }

        //====================================================================================
        //  Captures
        //------------------------------------------------------------------------------------
        else if (Move::is_capturing(move))
        {
            //====================================================================================
            //  Promotion avec capture
            //------------------------------------------------------------------------------------
            if (Move::is_promoting(move))
            {
                remove_piece(dest, Us, promoted);
                set_piece(dest, Them, captured);
                set_piece(from, Us, piece);
            }

            //====================================================================================
            //  Capture simple
            //------------------------------------------------------------------------------------
            else
            {
                move_piece(dest, from, Us, piece);
                set_piece(dest, Them, captured);
            }
        }

        //====================================================================================
        //  Promotion simple
        //------------------------------------------------------------------------------------
        else if (Move::is_promoting(move))
        {
            remove_piece(dest, Us, promoted);
            set_piece(from, Us, piece);
        }
    }

    //====================================================================================
    //  Coup spécial : Double, EnPassant, Roque
    //------------------------------------------------------------------------------------
    else
    {
        //====================================================================================
        //  Poussée double de pions
        //------------------------------------------------------------------------------------
        if (Move::is_double(move))
        {
            move_piece(dest, from, Us, piece);
        }

        //====================================================================================
        //  Prise en passant
        //------------------------------------------------------------------------------------
        else if (Move::is_enpassant(move))
        {
            move_piece(dest, from, Us, piece);

            if constexpr (Us == Color::WHITE)
                set_piece(SQ::south(dest), Them, captured);
            else
                set_piece(SQ::north(dest), Them, captured);
        }

        //====================================================================================
        //  Roques
        //------------------------------------------------------------------------------------
        else if (Move::is_castling(move))
        {
            //====================================================================================
            //  Petit Roque
            //------------------------------------------------------------------------------------
            if ((SQ::square_BB(dest)) & FILE_G_BB)
            {
                move_piece(dest, from, Us, piece);

                // Move the rook
                if constexpr (Us == WHITE)
                    move_piece(F1, H1, Us, Piece::WHITE_ROOK);
                else
                    move_piece(F8, H8, Us, Piece::BLACK_ROOK);
            }

            //====================================================================================
            //  Grand Roque
            //------------------------------------------------------------------------------------
            else if ((SQ::square_BB(dest)) & FILE_C_BB)
            {
                move_piece(dest, from, Us, piece);

                // Move the rook
                if constexpr (Us == WHITE)
                    move_piece(D1, A1, Us, Piece::WHITE_ROOK);
                else
                    move_piece(D8, A8, Us, Piece::BLACK_ROOK);
            }
        }
    }

    statusHistory.pop_back();

#ifndef NDEBUG
    // on ne passe ici qu'en debug
    assert(valid());
#endif
}

//===================================================================
//! \brief  Enlève un NullMove
//-------------------------------------------------------------------
template <Color Us> void Board::undo_nullmove() noexcept
{
    statusHistory.pop_back();       // supprime le dernier status
    side_to_move = ~side_to_move;   // Change de camp

#ifndef NDEBUG
    // on ne passe ici qu'en debug
    assert(valid());
#endif
}



// Explicit instantiations.

template void Board::undo_move<WHITE, true>() noexcept ;
template void Board::undo_move<BLACK, true>() noexcept ;
template void Board::undo_move<WHITE, false>() noexcept ;
template void Board::undo_move<BLACK, false>() noexcept ;

template void Board::undo_nullmove<WHITE>() noexcept ;
template void Board::undo_nullmove<BLACK>() noexcept ;
