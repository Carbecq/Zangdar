#include "Board.h"
#include "Move.h"
#include "types.h"

//=============================================================
//! \brief  Enlève un coup
//-------------------------------------------------------------
template <Color C, bool Update_NNUE>
void Board::undo_move() noexcept
{
    constexpr Color Them     = ~C;

    // printf("------------------------------------------undo move \n");

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
        x_king[C] = from;

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
            move_piece(dest, from, C, piece);
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
                remove_piece(dest, C, promoted);
                set_piece(dest, Them, captured);
                set_piece(from, C, piece);
            }

            //====================================================================================
            //  Capture simple
            //------------------------------------------------------------------------------------
            else
            {
                move_piece(dest, from, C, piece);
                set_piece(dest, Them, captured);
            }
        }

        //====================================================================================
        //  Promotion simple
        //------------------------------------------------------------------------------------
        else if (Move::is_promoting(move))
        {
            remove_piece(dest, C, promoted);
            set_piece(from, C, piece);
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
            move_piece(dest, from, C, piece);
        }

        //====================================================================================
        //  Prise en passant
        //------------------------------------------------------------------------------------
        else if (Move::is_enpassant(move))
        {
            move_piece(dest, from, C, piece);

            if constexpr (C == Color::WHITE)
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
                move_piece(dest, from, C, piece);

                // Move the rook
                if constexpr (C == WHITE)
                    move_piece(F1, H1, C, Piece::WHITE_ROOK);
                else
                    move_piece(F8, H8, C, Piece::BLACK_ROOK);
            }

            //====================================================================================
            //  Grand Roque
            //------------------------------------------------------------------------------------
            else if ((SQ::square_BB(dest)) & FILE_C_BB)
            {
                move_piece(dest, from, C, piece);

                // Move the rook
                if constexpr (C == WHITE)
                    move_piece(D1, A1, C, Piece::WHITE_ROOK);
                else
                    move_piece(D8, A8, C, Piece::BLACK_ROOK);
            }
        }
    }

    StatusHistory.pop_back();

#ifndef NDEBUG
    // on ne passe ici qu'en debug
    assert(valid<Update_NNUE>("après undo_move"));
#endif
}

//===================================================================
//! \brief  Enlève un NullMove
//-------------------------------------------------------------------
template <Color C> void Board::undo_nullmove() noexcept
{
    StatusHistory.pop_back();       // supprime le dernier status
    side_to_move = ~side_to_move;   // Change de camp

#ifndef NDEBUG
    // on ne passe ici qu'en debug
    assert(valid<false>("après undo_nullmove"));
#endif
}



// Explicit instantiations.

template void Board::undo_move<WHITE, true>() noexcept ;
template void Board::undo_move<BLACK, true>() noexcept ;
template void Board::undo_move<WHITE, false>() noexcept ;
template void Board::undo_move<BLACK, false>() noexcept ;

template void Board::undo_nullmove<WHITE>() noexcept ;
template void Board::undo_nullmove<BLACK>() noexcept ;
