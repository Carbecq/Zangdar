#include "Board.h"
#include "Move.h"
#include <cassert>

/* This is the castle_mask array. We can use it to determine
the castling permissions after a move. What we do is
logical-AND the castle bits with the castle_mask bits for
both of the move's ints. Let's say castle is 1, meaning
that white can still castle kingside. Now we play a move
where the rook on h1 gets captured. We AND castle with
castle_mask[63], so we have 1&14, and castle becomes 0 and
white can't castle kingside anymore.
 (TSCP) */

constexpr U32 castle_mask[64] = {
    13, 15, 15, 15, 12, 15, 15, 14,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    7, 15, 15, 15,  3, 15, 15, 11
};


template <Color C> void Board::make_move(const MOVE move) noexcept
{
    constexpr Color Them     = ~C;
    const auto dest     = Move::dest(move);
    const auto from     = Move::from(move);
    const auto piece    = Move::piece(move);
    const auto captured = Move::captured(move);
    const auto promo    = Move::promotion(move);
#ifndef NDEBUG
    // on ne passe ici qu'en debug
    const auto ep_old   = ep_square;
#endif

    //    printf("move  = (%s) \n", Move::name(move).c_str());
    //    binary_print(move);
    //    printf("side  = %s \n", side_name[C].c_str());
    //    printf("from  = %s \n", square_name[from].c_str());
    //    printf("dest  = %s \n", square_name[dest].c_str());
    //    printf("piece = %s \n", piece_name[piece].c_str());
    //    printf("capt  = %s \n", piece_name[captured].c_str());
    //    printf("promo = %s \n", piece_name[promo].c_str());
    //    printf("flags = %u \n", Move::flags(move));

    assert(pieceOn[king_square<C>()] == KING);
    assert(dest != from);
    assert(piece != NO_TYPE);
    assert(captured != KING);
    assert(promo != PAWN);
    assert(promo != KING);
    assert(pieceOn[from] == piece);

#if defined USE_NNUE
    nnue.push();    // nouveau accumulateur
#endif

    // Sauvegarde des caractéristiques de la position
    game_history[gamemove_counter] = UndoInfo{hash, pawn_hash, move, ep_square, halfmove_counter, castling, checkers, pinned} ;

// La prise en passant n'est valable que tout de suite
// Il faut donc la supprimer
#if defined USE_HASH
    if (ep_square != NO_SQUARE) {
        hash ^= ep_key[ep_square];
    }
#endif

    // Remove ep
    ep_square = NO_SQUARE;

    // Déplacement du roi
    if (piece == KING)
        x_king[C] = dest;

// Droit au roque, remove ancient value
#if defined USE_HASH
    hash ^= castle_key[castling];
#endif

    // Castling permissions
    castling = castling & castle_mask[from] & castle_mask[dest];

// Droit au roque; add new value
#if defined USE_HASH
    hash ^= castle_key[castling];
#endif

    // Increment halfmove clock
    halfmove_counter++;

    // Fullmoves
    fullmove_counter += (C == Color::BLACK);

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
            assert(captured == NO_TYPE);
            assert(promo    == NO_TYPE);

            BB::toggle_bit2(colorPiecesBB[C], from, dest);
            BB::toggle_bit2(typePiecesBB[piece], from, dest);
            
            pieceOn[from] = NO_TYPE;
            pieceOn[dest] = piece;

#if defined USE_NNUE
            nnue.update_feature<false>(C, piece, from);     // remove piece
            nnue.update_feature<true>(C, piece, dest);      // add piece
#endif
#if defined USE_HASH
            hash ^= piece_key[C][piece][from] ^ piece_key[C][piece][dest];
#endif

            if (piece == PAWN)
            {
                halfmove_counter = 0;
#if defined USE_HASH
                pawn_hash ^= piece_key[C][PAWN][from] ^ piece_key[C][PAWN][dest];
#endif
            }
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
                assert(piece    == PAWN);
                assert(captured != NO_TYPE);
                assert(promo    != NO_TYPE);
                assert(SQ::file(dest) != SQ::file(from));
                assert((C == Color::WHITE && SQ::rank(dest) == 7) ||
                       (C == Color::BLACK && SQ::rank(dest) == 0));
                assert((C == Color::WHITE && SQ::rank(from) == 6) ||
                       (C == Color::BLACK && SQ::rank(from) == 1));

                BB::toggle_bit(typePiecesBB[PAWN], from);       // suppression du pion
                BB::toggle_bit(colorPiecesBB[C], from);

                BB::toggle_bit(typePiecesBB[captured], dest);   // Remove the captured piece
                BB::toggle_bit(colorPiecesBB[Them], dest);

                BB::toggle_bit(typePiecesBB[promo], dest);      // Ajoute la pièce promue
                BB::toggle_bit(colorPiecesBB[C], dest);
                
                pieceOn[from] = NO_TYPE;
                pieceOn[dest] = promo;

#if defined USE_NNUE
                nnue.update_feature<false>(C, PAWN, from);          // remove pawn
                nnue.update_feature<false>(~C, captured, dest);     // remove captured
                nnue.update_feature<true>(C, promo, dest);          // add promoted
#endif
#if defined USE_HASH
                hash ^= piece_key[C][piece][from];
                hash ^= piece_key[C][promo][dest];
                hash ^= piece_key[Them][captured][dest];

                pawn_hash ^= piece_key[C][PAWN][from];
#endif

                halfmove_counter = 0;
            }

            //====================================================================================
            //  Capture simple
            //------------------------------------------------------------------------------------
            else
            {
                assert(captured != NO_TYPE);
                assert(promo == NO_TYPE);

                BB::toggle_bit2(colorPiecesBB[C],    from, dest);   //  déplacement de la pièce
                BB::toggle_bit2(typePiecesBB[piece], from, dest);

                BB::toggle_bit(colorPiecesBB[Them],    dest);       //  suppression de la pièce prise
                BB::toggle_bit(typePiecesBB[captured], dest);

                pieceOn[from] = NO_TYPE;
                pieceOn[dest] = piece;

#if defined USE_NNUE
                nnue.update_feature<false>(C, piece, from);
                nnue.update_feature<true>(C, piece, dest);
                nnue.update_feature<false>(~C, captured, dest);
#endif
#if defined USE_HASH
                hash ^= piece_key[C][piece][from] ^ piece_key[C][piece][dest];
                hash ^= piece_key[Them][captured][dest];

                if (piece == PAWN)
                    pawn_hash ^= piece_key[C][PAWN][from] ^ piece_key[C][PAWN][dest];
                if (captured == PAWN)
                    pawn_hash ^= piece_key[Them][PAWN][dest];
#endif
                halfmove_counter = 0;
             }
        }

        //====================================================================================
        //  Promotion simple
        //------------------------------------------------------------------------------------
        else if (Move::is_promoting(move))
        {
            assert(piece    == PAWN);
            assert(captured == NO_TYPE);
            assert(promo    != NO_TYPE);
            assert(SQ::file(dest) == SQ::file(from));
            assert((C == Color::WHITE && SQ::rank(dest) == 7) ||
                   (C == Color::BLACK && SQ::rank(dest) == 0));
            assert((C == Color::WHITE && SQ::rank(from) == 6) ||
                   (C == Color::BLACK && SQ::rank(from) == 1));

            BB::toggle_bit(typePiecesBB[PAWN], from);       // suppression du pion
            BB::toggle_bit(colorPiecesBB[C], from);

            BB::toggle_bit(typePiecesBB[promo], dest);      // Ajoute la pièce promue
            BB::toggle_bit(colorPiecesBB[C], dest);
            
            pieceOn[from] = NO_TYPE;
            pieceOn[dest] = promo;

#if defined USE_NNUE
            nnue.update_feature<false>(C, PAWN, from);
            nnue.update_feature<true>(C, promo, dest);
#endif
#if defined USE_HASH
            hash ^= piece_key[C][piece][from];
            hash ^= piece_key[C][promo][dest];

            pawn_hash ^= piece_key[C][PAWN][from];
#endif

            halfmove_counter = 0;
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
            assert(piece == PAWN);
            assert(captured == NO_TYPE);
            assert(promo == NO_TYPE);
            assert(SQ::file(dest) == SQ::file(from));
            assert((C == Color::WHITE && SQ::rank(dest) == 3) || (C == Color::BLACK && SQ::rank(dest) == 4));
            assert((C == Color::WHITE && SQ::rank(from) == 1) || (C == Color::BLACK && SQ::rank(from) == 6));

            BB::toggle_bit2(colorPiecesBB[C], from, dest);
            BB::toggle_bit2(typePiecesBB[piece], from, dest);
            
            pieceOn[from] = NO_TYPE;
            pieceOn[dest] = piece;

#if defined USE_NNUE
            nnue.update_feature<false>(C, PAWN, from);
            nnue.update_feature<true>(C, PAWN, dest);
#endif
#if defined USE_HASH
            hash      ^= piece_key[C][PAWN][from] ^ piece_key[C][PAWN][dest];
            pawn_hash ^= piece_key[C][PAWN][from] ^ piece_key[C][PAWN][dest];
#endif

            halfmove_counter = 0;
            ep_square = (C == Color::WHITE) ? SQ::south(dest) : SQ::north(dest);

#if defined USE_HASH
            hash ^= ep_key[ep_square];
#endif
        }

        //====================================================================================
        //  Prise en passant
        //------------------------------------------------------------------------------------
        else if (Move::is_enpassant(move))
        {
            assert(piece == PAWN);
            assert(captured == PAWN);
            assert(promo == NO_TYPE);
            assert(SQ::file(dest) == SQ::file(ep_old));
            assert((C == Color::WHITE && SQ::rank(dest) == 5)   || (C == Color::BLACK && SQ::rank(dest) == 2));
            assert((C == Color::WHITE && SQ::rank(from) == 4) || (C == Color::BLACK && SQ::rank(from) == 3));
            assert(SQ::file(dest) - SQ::file(from) == 1     || SQ::file(from) - SQ::file(dest) == 1);

            BB::toggle_bit2(colorPiecesBB[C],   from, dest);
            BB::toggle_bit2(typePiecesBB[PAWN], from, dest);
            
            pieceOn[from] = NO_TYPE;
            pieceOn[dest] = PAWN;

#if defined USE_NNUE
            nnue.update_feature<false>(C, PAWN, from);
            nnue.update_feature<true>(C, PAWN, dest);
#endif
#if defined USE_HASH
            hash      ^= piece_key[C][PAWN][from] ^ piece_key[C][PAWN][dest];
            pawn_hash ^= piece_key[C][PAWN][from] ^ piece_key[C][PAWN][dest];
#endif
            halfmove_counter = 0;

            // Remove the captured pawn
            if (C == Color::WHITE)
            {
                BB::toggle_bit(typePiecesBB[PAWN],          SQ::south(dest));
                BB::toggle_bit(colorPiecesBB[Color::BLACK], SQ::south(dest));
                pieceOn[SQ::south(dest)] = NO_TYPE;

#if defined USE_NNUE
                nnue.update_feature<false>(~C, PAWN, SQ::south(dest));
#endif
#if defined USE_HASH
                hash      ^= piece_key[Them][PAWN][SQ::south(dest)];
                pawn_hash ^= piece_key[Them][PAWN][SQ::south(dest)];
#endif
            }
            else
            {
                BB::toggle_bit(typePiecesBB[PAWN],          SQ::north(dest));
                BB::toggle_bit(colorPiecesBB[Color::WHITE], SQ::north(dest));
                pieceOn[SQ::north(dest)] = NO_TYPE;

#if defined USE_NNUE
                nnue.update_feature<false>(~C, PAWN, SQ::north(dest));
#endif
#if defined USE_HASH
                hash      ^= piece_key[Them][PAWN][SQ::north(dest)];
                pawn_hash ^= piece_key[Them][PAWN][SQ::north(dest)];
#endif
            }
        }

        //====================================================================================
        //  Roques
        //------------------------------------------------------------------------------------
        else if (Move::is_castling(move))
        {
            assert(piece    == KING);
            assert(captured == NO_TYPE);
            assert(promo    == NO_TYPE);
            assert(pieceOn[from] == KING);
            assert(pieceOn[dest] == NO_TYPE);

            //====================================================================================
            //  Petit Roque
            //------------------------------------------------------------------------------------
            if ((SQ::square_BB(dest)) & FILE_G_BB)
            {
                assert((dest == get_king_dest<C, CastleSide::KING_SIDE>())  );

                // Move the King
                BB::toggle_bit2(colorPiecesBB[C], from, dest);
                BB::toggle_bit2(typePiecesBB[KING], from, dest);
                
                pieceOn[from] = NO_TYPE;
                pieceOn[dest] = KING;

                assert(piece_on(get_king_dest<C, CastleSide::KING_SIDE>()) == KING);

#if defined USE_NNUE
                nnue.update_feature<false>(C, piece, from);
                nnue.update_feature<true>(C, piece, dest);
#endif
#if defined USE_HASH
                hash ^= piece_key[C][piece][from];
                hash ^= piece_key[C][piece][dest];
#endif
                // Move the Rook
                if constexpr (C == WHITE)
                {
                    BB::toggle_bit2(colorPiecesBB[C],   H1, F1);
                    BB::toggle_bit2(typePiecesBB[ROOK], H1, F1);

                    pieceOn[H1] = NO_TYPE;
                    pieceOn[F1] = ROOK;

                    assert(piece_on(F1) == ROOK);
#if defined USE_NNUE
                    nnue.update_feature<false>(C, ROOK, H1);     // remove piece
                    nnue.update_feature<true>(C, ROOK, F1);    // add piece
#endif
#if defined USE_HASH
                    hash ^= piece_key[C][ROOK][H1];
                    hash ^= piece_key[C][ROOK][F1];
#endif
                }
                else
                {
                    BB::toggle_bit2(colorPiecesBB[C],   H8, F8);
                    BB::toggle_bit2(typePiecesBB[ROOK], H8, F8);

                    pieceOn[H8] = NO_TYPE;
                    pieceOn[F8] = ROOK;

                    assert(piece_on(F8) == ROOK);
#if defined USE_NNUE
                    nnue.update_feature<false>(C, ROOK, H8);     // remove piece
                    nnue.update_feature<true>(C, ROOK,  F8);    // add piece
#endif
#if defined USE_HASH
                    hash ^= piece_key[C][ROOK][H8];
                    hash ^= piece_key[C][ROOK][F8];
#endif
                }
            }

            //====================================================================================
            //  Grand Roque
            //------------------------------------------------------------------------------------
            else if ((SQ::square_BB(dest)) & FILE_C_BB)
            {
                assert((dest == get_king_dest<C, CastleSide::QUEEN_SIDE>() ));

                // Move the King
                BB::toggle_bit2(colorPiecesBB[C], from, dest);
                BB::toggle_bit2(typePiecesBB[KING], from, dest);
                
                pieceOn[from] = NO_TYPE;
                pieceOn[dest] = KING;

                assert(piece_on(get_king_dest<C, CastleSide::QUEEN_SIDE>()) == KING);

#if defined USE_NNUE
                nnue.update_feature<false>(C, piece, from);
                nnue.update_feature<true>(C, piece, dest);
#endif
#if defined USE_HASH
                hash ^= piece_key[C][piece][from];
                hash ^= piece_key[C][piece][dest];
#endif

                // Move the Rook
                if constexpr (C == WHITE)
                {
                    BB::toggle_bit2(colorPiecesBB[C],   A1, D1);
                    BB::toggle_bit2(typePiecesBB[ROOK], A1, D1);

                    pieceOn[A1] = NO_TYPE;
                    pieceOn[D1] = ROOK;

                    assert(piece_on(D1) == ROOK);
#if defined USE_NNUE
                    nnue.update_feature<false>(C, ROOK, A1);
                    nnue.update_feature<true>(C, ROOK, D1);
#endif
#if defined USE_HASH
                    hash ^= piece_key[C][ROOK][A1];
                    hash ^= piece_key[C][ROOK][D1];
#endif
                }
                else
                {
                    BB::toggle_bit2(colorPiecesBB[C],   A8, D8);
                    BB::toggle_bit2(typePiecesBB[ROOK], A8, D8);

                    pieceOn[A8] = NO_TYPE;
                    pieceOn[D8] = ROOK;

                    assert(piece_on(D8) == ROOK);
#if defined USE_NNUE
                    nnue.update_feature<false>(C, ROOK, A8);
                    nnue.update_feature<true>(C, ROOK,  D8);
#endif
#if defined USE_HASH
                    hash ^= piece_key[C][ROOK][A8];
                    hash ^= piece_key[C][ROOK][D8];
#endif
                }
            }
        } // Roques
    } // Special

    // Swap sides
    side_to_move = ~side_to_move;

    gamemove_counter++;

    // pièces donnant échec, pièces clouées
    // attention : on a chabgé de camp !!!
    checkers_pinned<~C>();

#if defined USE_HASH
    hash ^= side_key;

#if defined DEBUG_HASH
    U64 hash_1, hash_2;
    calculate_hash(hash_1, hash_2);

    if (hash_1 != hash)
    {
        std::cout << Move::name(move) << "  : hash pb : hash = " << hash << " ; calc = " << hash_1 << std::endl;
        std::cout << display() << std::endl << std::endl;
    }
    if (hash_2 != pawn_hash)
    {
        std::cout << Move::name(move) << "  : pawn_hash pb : pawn_hash = " << pawn_hash << " ; calc = " << hash_2 << std::endl;
        std::cout << display() << std::endl << std::endl;
    }
#endif
#endif


#ifndef NDEBUG
    // on ne passe ici qu'en debug
    assert(valid());
#endif
}


//=======================================================================
//! \brief un Null Move est un coup où on passe son tour.
//! la position ne change pas, excepté le camp qui change.
//-----------------------------------------------------------------------
template <Color C> void Board::make_nullmove() noexcept
{
    // Sauvegarde des caractéristiques de la position
    game_history[gamemove_counter] = UndoInfo{hash, pawn_hash, Move::MOVE_NULL, ep_square, halfmove_counter, castling, checkers, pinned};

// La prise en passant n'est valable que tout de suite
// Il faut donc la supprimer
#if defined USE_HASH
    if (ep_square != NO_SQUARE) {
        hash ^= ep_key[ep_square];
    }
#endif

    // Remove ep
    ep_square = NO_SQUARE;

    // Increment halfmove clock
    halfmove_counter++;

    // Fullmoves
    fullmove_counter += (C == Color::BLACK);

    // Swap sides
    side_to_move = ~side_to_move;

    gamemove_counter++;

    // Null move cannot gives check
    //TODO à améliorer
    // attention : on a chabgé de camp !!!
    checkers_pinned<~C>();
    // checkers = 0ULL;
    // pinned = ??

#if defined USE_HASH
    hash ^= side_key;
#endif

#ifndef NDEBUG
    // on ne passe ici qu'en debug
    valid();
#endif
}

// Explicit instantiations.
template void Board::make_move<WHITE>(const U32 move) noexcept;
template void Board::make_move<BLACK>(const U32 move) noexcept;

template void Board::make_nullmove<WHITE>() noexcept;
template void Board::make_nullmove<BLACK>() noexcept;
