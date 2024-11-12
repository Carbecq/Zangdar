#include "Board.h"
#include "Move.h"
#include "types.h"

//=============================================================
//! \brief  Enlève un coup
//-------------------------------------------------------------
template <Color C> constexpr void Board::undo_move() noexcept
{
    constexpr Color Them     = ~C;

    // Swap sides
    side_to_move = ~side_to_move;

    gamemove_counter--;
    
    const auto &move    = game_history[gamemove_counter].move;
    const auto dest     = Move::dest(move);
    const auto from     = Move::from(move);
    const auto piece    = Move::piece(move);
    const auto captured = Move::captured(move);
    const auto promo    = Move::promotion(move);

    // En passant
    // back : Returns a reference to the last element in the vector.
    ep_square = game_history[gamemove_counter].ep_square;

    // Halfmoves
    halfmove_counter = game_history[gamemove_counter].halfmove_counter;

    // Fullmoves
    fullmove_counter -= (C == Color::BLACK);

    // Déplacement du roi
    if (piece == KING)
        x_king[C] = from;

    // Pièces donnant échec
    checkers = game_history[gamemove_counter].checkers;
    pinned   = game_history[gamemove_counter].pinned;

    // Castling
    castling = game_history[gamemove_counter].castling;

#if defined USE_HASH
    hash      = game_history[gamemove_counter].hash;
    pawn_hash = game_history[gamemove_counter].pawn_hash;
#endif

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
            pieceOn[dest] = NO_TYPE;
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
                
                pieceOn[from]= PAWN;
                pieceOn[dest]  = captured;
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
            pieceOn[dest] = NO_TYPE;
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
            pieceOn[dest] = NO_TYPE;
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
                pieceOn[dest] = NO_TYPE;
                pieceOn[SQ::south(dest)] = PAWN;
            }
            else
            {
                BB::toggle_bit(typePiecesBB[PAWN], SQ::north(dest));
                BB::toggle_bit(colorPiecesBB[Color::WHITE], SQ::north(dest));
                
                pieceOn[from] = PAWN;
                pieceOn[dest] = NO_TYPE;
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
                // Move the rook
                BB::toggle_bit2(colorPiecesBB[C],   get_rook_from<C, CastleSide::KING_SIDE>(), get_rook_dest<C, CastleSide::KING_SIDE>());
                BB::toggle_bit2(typePiecesBB[ROOK], get_rook_from<C, CastleSide::KING_SIDE>(), get_rook_dest<C, CastleSide::KING_SIDE>());
                
                pieceOn[from] = piece;
                pieceOn[dest] = NO_TYPE;
                pieceOn[get_rook_from<C, CastleSide::KING_SIDE>()] = ROOK;
                pieceOn[get_rook_dest<C, CastleSide::KING_SIDE>()] = NO_TYPE;
            }

            //====================================================================================
            //  Grand Roque
            //------------------------------------------------------------------------------------
            else if ((SQ::square_BB(dest)) & FILE_C_BB)
            {
                // Move the rook
                BB::toggle_bit2(colorPiecesBB[C],   get_rook_from<C, CastleSide::QUEEN_SIDE>(), get_rook_dest<C, CastleSide::QUEEN_SIDE>());
                BB::toggle_bit2(typePiecesBB[ROOK], get_rook_from<C, CastleSide::QUEEN_SIDE>(), get_rook_dest<C, CastleSide::QUEEN_SIDE>());
                
                pieceOn[from] = piece;
                pieceOn[dest] = NO_TYPE;
                pieceOn[get_rook_from<C, CastleSide::QUEEN_SIDE>()] = ROOK;
                pieceOn[get_rook_dest<C, CastleSide::QUEEN_SIDE>()] = NO_TYPE;
            }
        }
    }

#ifndef NDEBUG
    // on ne passe ici qu'en debug
    valid();
#endif
}

//===================================================================
//! \brief  Enlève un NullMove
//-------------------------------------------------------------------
template <Color C> constexpr void Board::undo_nullmove() noexcept
{
    // Swap sides
    side_to_move = ~side_to_move;

    gamemove_counter--;

    // En passant
    ep_square = game_history[gamemove_counter].ep_square;

    // Halfmoves
    halfmove_counter = game_history[gamemove_counter].halfmove_counter;

    // Fullmoves
    fullmove_counter -= (C == Color::BLACK);

    // Castling
    castling = game_history[gamemove_counter].castling;

#if defined USE_HASH
    hash      = game_history[gamemove_counter].hash;
    pawn_hash = game_history[gamemove_counter].pawn_hash;
#endif

    checkers = game_history[gamemove_counter].checkers;
    pinned   = game_history[gamemove_counter].pinned;

#ifndef NDEBUG
    // on ne passe ici qu'en debug
    valid();
#endif
}



// Explicit instantiations.

template void Board::undo_move<WHITE>() noexcept ;
template void Board::undo_move<BLACK>() noexcept ;

template void Board::undo_nullmove<WHITE>() noexcept ;
template void Board::undo_nullmove<BLACK>() noexcept ;
