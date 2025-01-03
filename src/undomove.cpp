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
    const auto promo    = Move::promotion(move);

    // Déplacement du roi
    if (piece == KING)
        x_king[C] = from;

    // Remove piece
    BB::toggle_bit(colorPiecesBB[C],    dest);
    BB::toggle_bit(typePiecesBB[piece],  dest);

    // Add piece
    BB::toggle_bit(colorPiecesBB[C], from);
    BB::toggle_bit(typePiecesBB[piece], from);

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
            pieceOn[from] = piece;
            pieceOn[dest] = NO_PIECE;
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
                // Replace pawn with piece
                BB::toggle_bit(typePiecesBB[PAWN], dest);
                BB::toggle_bit(typePiecesBB[promo], dest);

                // Replace the captured piece
                BB::toggle_bit(typePiecesBB[captured], dest);
                BB::toggle_bit(colorPiecesBB[Them], dest);
                
                pieceOn[from] = PAWN;
                pieceOn[dest] = captured;
            }

            //====================================================================================
            //  Capture simple
            //------------------------------------------------------------------------------------
            else
            {
                pieceOn[from]= piece;
                BB::toggle_bit(colorPiecesBB[Them], dest);
                BB::toggle_bit(typePiecesBB[captured], dest);
                pieceOn[dest]  = captured;
            }
        }

        //====================================================================================
        //  Promotion simple
        //------------------------------------------------------------------------------------
        else if (Move::is_promoting(move))
        {
            // Replace piece with pawn
            BB::toggle_bit(typePiecesBB[PAWN], dest);
            BB::toggle_bit(typePiecesBB[promo], dest);
            
            pieceOn[from] = PAWN;
            pieceOn[dest] = NO_PIECE;
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
            pieceOn[from] = piece;
            pieceOn[dest] = NO_PIECE;
        }

        //====================================================================================
        //  Prise en passant
        //------------------------------------------------------------------------------------
        else if (Move::is_enpassant(move))
        {
            // Replace the captured pawn
            if (C == Color::WHITE)
            {
                BB::toggle_bit(typePiecesBB[PAWN], SQ::south(dest));
                BB::toggle_bit(colorPiecesBB[Color::BLACK], SQ::south(dest));
                
                pieceOn[from] = PAWN;
                pieceOn[dest] = NO_PIECE;
                pieceOn[SQ::south(dest)] = PAWN;
            }
            else
            {
                BB::toggle_bit(typePiecesBB[PAWN], SQ::north(dest));
                BB::toggle_bit(colorPiecesBB[Color::WHITE], SQ::north(dest));
                
                pieceOn[from] = PAWN;
                pieceOn[dest] = NO_PIECE;
                pieceOn[SQ::north(dest)] = PAWN;
            }
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
                pieceOn[from] = KING;
                pieceOn[dest] = NO_PIECE;

                // Move the rook
                if constexpr (C == WHITE)
                {
                    BB::toggle_bit2(colorPiecesBB[C],   H1, F1);
                    BB::toggle_bit2(typePiecesBB[ROOK], H1, F1);

                    pieceOn[H1] = ROOK;
                    pieceOn[F1] = NO_PIECE;
                }
                else
                {
                    BB::toggle_bit2(colorPiecesBB[C],   H8, F8);
                    BB::toggle_bit2(typePiecesBB[ROOK], H8, F8);

                    pieceOn[H8] = ROOK;
                    pieceOn[F8] = NO_PIECE;
                }
            }

            //====================================================================================
            //  Grand Roque
            //------------------------------------------------------------------------------------
            else if ((SQ::square_BB(dest)) & FILE_C_BB)
            {
                pieceOn[from] = KING;
                pieceOn[dest] = NO_PIECE;

                // Move the rook
                if constexpr (C == WHITE)
                {
                    BB::toggle_bit2(colorPiecesBB[C],   A1, D1);
                    BB::toggle_bit2(typePiecesBB[ROOK], A1, D1);

                    pieceOn[A1] = ROOK;
                    pieceOn[D1] = NO_PIECE;
                }
                else
                {
                    BB::toggle_bit2(colorPiecesBB[C],   A8, D8);
                    BB::toggle_bit2(typePiecesBB[ROOK], A8, D8);

                    pieceOn[A8] = ROOK;
                    pieceOn[D8] = NO_PIECE;
                }
            }
        }
    }

    StatusHistory.pop_back();

#ifndef NDEBUG
    // on ne passe ici qu'en debug
    valid();
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
    valid();
#endif
}



// Explicit instantiations.

template void Board::undo_move<WHITE, true>() noexcept ;
template void Board::undo_move<BLACK, true>() noexcept ;
template void Board::undo_move<WHITE, false>() noexcept ;
template void Board::undo_move<BLACK, false>() noexcept ;

template void Board::undo_nullmove<WHITE>() noexcept ;
template void Board::undo_nullmove<BLACK>() noexcept ;
