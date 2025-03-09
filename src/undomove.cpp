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

    if constexpr (Update_NNUE == true)
        nnue.pop(); // retourne a l'accumulateur précédent

    const MOVE move = get_status().move;

    // Swap sides
    side_to_move = ~side_to_move;

    // const auto &move    = BoardStatusHistory[gamemove_counter].move;
    const auto dest     = Move::dest(move);
    const auto from     = Move::from(move);
    const auto piece    = Move::piece(move);
    const auto captured = Move::captured(move);
    const auto promoted = Move::promotion(move);

    // Déplacement du roi
    if (piece == KING)
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
                set_piece(from, C, PAWN);
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
            set_piece(from, C, PAWN);
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
            // Replace the captured pawn
            move_piece(dest, from, C, PAWN);

            if constexpr (C == Color::WHITE)
                set_piece(SQ::south(dest), Them, PAWN);
            else
                set_piece(SQ::north(dest), Them, PAWN);
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
                move_piece(dest, from, C, KING);

                // Move the rook
                if constexpr (C == WHITE)
                    move_piece(F1, H1, C, ROOK);
                else
                    move_piece(F8, H8, C, ROOK);
            }

            //====================================================================================
            //  Grand Roque
            //------------------------------------------------------------------------------------
            else if ((SQ::square_BB(dest)) & FILE_C_BB)
            {
                move_piece(dest, from, C, KING);

                // Move the rook
                if constexpr (C == WHITE)
                    move_piece(D1, A1, C, ROOK);
                else
                    move_piece(D8, A8, C, ROOK);
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
