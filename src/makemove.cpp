#include <cassert>
#include "Board.h"
#include "Move.h"
#include "zobrist.h"

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


template <Color C, bool Update_NNUE>
void Board::make_move(const MOVE move) noexcept
{
    constexpr Color Them     = ~C;

    const auto dest     = Move::dest(move);
    const auto from     = Move::from(move);
    const auto piece    = Move::piece(move);
    const auto captured = Move::captured(move);
    const auto promo    = Move::promotion(move);

    // std::cout << display() << std::endl << std::endl;
    //    printf("move  = (%s) \n", Move::name(move).c_str());
    //    // binary_print(move);
    //    printf("side  = %s \n", side_name[C].c_str());
    //    printf("from  = %s \n", square_name[from].c_str());
    //    printf("dest  = %s \n", square_name[dest].c_str());
    //    printf("piece = %s \n", piece_name[piece].c_str());
    //    printf("capt  = %s \n", piece_name[captured].c_str());
    //    printf("promo = %s \n", piece_name[promo].c_str());
    //    printf("flags = %u \n", Move::flags(move));

    assert(pieceOn[king_square<C>()] == KING);
    assert(dest != from);
    assert(piece != NO_PIECE);
    assert(captured != KING);
    assert(promo != PAWN);
    assert(promo != KING);
    assert(pieceOn[from] == piece);
    assert(!StatusHistory.empty());

    if constexpr (Update_NNUE == true)
        nnue.push();    // nouvel accumulateur

    const Status& previousStatus = StatusHistory.back(); // référence

    StatusHistory.emplace_back( // ajoute un nouvel élément à la fin
                                previousStatus.key ^ side_key,                              // zobrist will be changed later
                                move,
                                NO_SQUARE,                                                  // reset en passant. might be set later
                                previousStatus.castling,                                    // copy meta. might be changed
                                previousStatus.fiftymove_counter + 1,                        // increment fifty move counter. might be reset
                                previousStatus.fullmove_counter + (C == Color::BLACK),      // increment move counter
                                0ULL,
                                0ULL
        );

    Status& newStatus = StatusHistory.back();

    // La prise en passant n'est valable que tout de suite
    // Il faut donc la supprimer
    if (previousStatus.ep_square != NO_SQUARE)
        newStatus.key ^= ep_key[previousStatus.ep_square];

    // Déplacement du roi
    if (piece == KING)
        x_king[C] = dest;

    // Droit au roque, remove ancient value
    newStatus.key ^= castle_key[previousStatus.castling];

    // Castling permissions
    newStatus.castling = previousStatus.castling & castle_mask[from] & castle_mask[dest];

    // Droit au roque; add new value
    newStatus.key ^= castle_key[newStatus.castling];

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
            assert(captured == NO_PIECE);
            assert(promo    == NO_PIECE);

            BB::toggle_bit2(colorPiecesBB[C], from, dest);
            BB::toggle_bit2(typePiecesBB[piece], from, dest);
            
            pieceOn[from] = NO_PIECE;
            pieceOn[dest] = piece;

            if constexpr (Update_NNUE == true)
                nnue.sub_add(C, piece, from, piece, dest);
            newStatus.key ^= piece_key[C][piece][from] ^ piece_key[C][piece][dest];

            if (piece == PAWN)
            {
                newStatus.fiftymove_counter = 0;
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
                assert(captured != NO_PIECE);
                assert(promo    != NO_PIECE);
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
                
                pieceOn[from] = NO_PIECE;
                pieceOn[dest] = promo;

                if constexpr (Update_NNUE == true)
                    nnue.sub_sub_add(C, PAWN, from, captured, promo, dest);
                newStatus.key ^= piece_key[C][piece][from];
                newStatus.key ^= piece_key[C][promo][dest];
                newStatus.key ^= piece_key[Them][captured][dest];

                newStatus.fiftymove_counter = 0;
            }

            //====================================================================================
            //  Capture simple
            //------------------------------------------------------------------------------------
            else
            {
                assert(captured != NO_PIECE);
                assert(promo == NO_PIECE);

                BB::toggle_bit2(colorPiecesBB[C],    from, dest);   //  déplacement de la pièce
                BB::toggle_bit2(typePiecesBB[piece], from, dest);

                BB::toggle_bit(colorPiecesBB[Them],    dest);       //  suppression de la pièce prise
                BB::toggle_bit(typePiecesBB[captured], dest);

                pieceOn[from] = NO_PIECE;
                pieceOn[dest] = piece;

                if constexpr (Update_NNUE == true)
                    nnue.sub_sub_add(C, piece, from, captured, piece, dest);
                newStatus.key ^= piece_key[C][piece][from] ^ piece_key[C][piece][dest];
                newStatus.key ^= piece_key[Them][captured][dest];

                newStatus.fiftymove_counter = 0;
            }
        }

        //====================================================================================
        //  Promotion simple
        //------------------------------------------------------------------------------------
        else if (Move::is_promoting(move))
        {
            assert(piece    == PAWN);
            assert(captured == NO_PIECE);
            assert(promo    != NO_PIECE);
            assert(SQ::file(dest) == SQ::file(from));
            assert((C == Color::WHITE && SQ::rank(dest) == 7) ||
                   (C == Color::BLACK && SQ::rank(dest) == 0));
            assert((C == Color::WHITE && SQ::rank(from) == 6) ||
                   (C == Color::BLACK && SQ::rank(from) == 1));

            BB::toggle_bit(typePiecesBB[PAWN], from);       // suppression du pion
            BB::toggle_bit(colorPiecesBB[C], from);

            BB::toggle_bit(typePiecesBB[promo], dest);      // Ajoute la pièce promue
            BB::toggle_bit(colorPiecesBB[C], dest);
            
            pieceOn[from] = NO_PIECE;
            pieceOn[dest] = promo;

            if constexpr (Update_NNUE == true)
                nnue.sub_add(C, PAWN, from, promo, dest);
            newStatus.key ^= piece_key[C][piece][from];
            newStatus.key ^= piece_key[C][promo][dest];
            newStatus.fiftymove_counter = 0;
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
            assert(captured == NO_PIECE);
            assert(promo == NO_PIECE);
            assert(SQ::file(dest) == SQ::file(from));
            assert((C == Color::WHITE && SQ::rank(dest) == 3) || (C == Color::BLACK && SQ::rank(dest) == 4));
            assert((C == Color::WHITE && SQ::rank(from) == 1) || (C == Color::BLACK && SQ::rank(from) == 6));

            BB::toggle_bit2(colorPiecesBB[C], from, dest);
            BB::toggle_bit2(typePiecesBB[piece], from, dest);
            
            pieceOn[from] = NO_PIECE;
            pieceOn[dest] = piece;

            if constexpr (Update_NNUE == true)
                nnue.sub_add(C, PAWN, from, PAWN, dest);
            newStatus.key      ^= piece_key[C][PAWN][from] ^ piece_key[C][PAWN][dest];
            newStatus.fiftymove_counter = 0;

            int new_ep;
            if constexpr (C == Color::WHITE)
                new_ep = SQ::south(dest);
            else
                new_ep = SQ::north(dest);

            if (!BB::empty(pawn_attackers<Them>(new_ep))  )     // La case de prise en-passant est-elle attaquée par un pion ?
            {
                // printf("attaquée \n");
                // En toute rigueur, il faudrait tester la légalité de la prise
                // On s'en passera ...
                newStatus.ep_square = new_ep;
                newStatus.key ^= ep_key[newStatus.ep_square];
            }
            else
            {
                // printf("PAS attaquée \n");
            }
        }

        //====================================================================================
        //  Prise en passant
        //------------------------------------------------------------------------------------
        else if (Move::is_enpassant(move))
        {
            assert(piece == PAWN);
            assert(captured == PAWN);
            assert(promo == NO_PIECE);
            //TODO assert(SQ::file(dest) == SQ::file(ep_old));
            assert((C == Color::WHITE && SQ::rank(dest) == 5)   || (C == Color::BLACK && SQ::rank(dest) == 2));
            assert((C == Color::WHITE && SQ::rank(from) == 4) || (C == Color::BLACK && SQ::rank(from) == 3));
            assert(SQ::file(dest) - SQ::file(from) == 1     || SQ::file(from) - SQ::file(dest) == 1);

            BB::toggle_bit2(colorPiecesBB[C],   from, dest);
            BB::toggle_bit2(typePiecesBB[PAWN], from, dest);
            
            pieceOn[from] = NO_PIECE;
            pieceOn[dest] = PAWN;

            newStatus.key      ^= piece_key[C][PAWN][from] ^ piece_key[C][PAWN][dest];
            newStatus.fiftymove_counter = 0;

            // Remove the captured pawn
            if constexpr (C == Color::WHITE)
            {
                BB::toggle_bit(typePiecesBB[PAWN],          SQ::south(dest));
                BB::toggle_bit(colorPiecesBB[Color::BLACK], SQ::south(dest));
                pieceOn[SQ::south(dest)] = NO_PIECE;

                if constexpr (Update_NNUE == true)
                    nnue.sub_sub_add(C, PAWN, from, PAWN, SQ::south(dest), PAWN, dest);
                newStatus.key      ^= piece_key[Them][PAWN][SQ::south(dest)];
            }
            else
            {
                BB::toggle_bit(typePiecesBB[PAWN],          SQ::north(dest));
                BB::toggle_bit(colorPiecesBB[Color::WHITE], SQ::north(dest));
                pieceOn[SQ::north(dest)] = NO_PIECE;

                if constexpr (Update_NNUE == true)
                    nnue.sub_sub_add(C, PAWN, from, PAWN, SQ::north(dest), PAWN, dest);
                newStatus.key      ^= piece_key[Them][PAWN][SQ::north(dest)];
            }
        }

        //====================================================================================
        //  Roques
        //------------------------------------------------------------------------------------
        else if (Move::is_castling(move))
        {
            assert(piece    == KING);
            assert(captured == NO_PIECE);
            assert(promo    == NO_PIECE);
            assert(pieceOn[from] == KING);
            assert(pieceOn[dest] == NO_PIECE);

            //====================================================================================
            //  Petit Roque
            //------------------------------------------------------------------------------------
            if ((SQ::square_BB(dest)) & FILE_G_BB)
            {
                assert((dest == get_king_dest<C, CastleSide::KING_SIDE>())  );

                // Move the King
                BB::toggle_bit2(colorPiecesBB[C], from, dest);
                BB::toggle_bit2(typePiecesBB[KING], from, dest);
                
                pieceOn[from] = NO_PIECE;
                pieceOn[dest] = KING;

                assert(piece_on(get_king_dest<C, CastleSide::KING_SIDE>()) == KING);

                if constexpr (Update_NNUE == true)
                    nnue.sub_add(C, piece, from, piece, dest);
                newStatus.key ^= piece_key[C][piece][from];
                newStatus.key ^= piece_key[C][piece][dest];

                // Move the Rook
                if constexpr (C == WHITE)
                {
                    BB::toggle_bit2(colorPiecesBB[C],   H1, F1);
                    BB::toggle_bit2(typePiecesBB[ROOK], H1, F1);

                    pieceOn[H1] = NO_PIECE;
                    pieceOn[F1] = ROOK;

                    assert(piece_on(F1) == ROOK);

                    if constexpr (Update_NNUE == true)
                        nnue.sub_add(C, ROOK, H1, ROOK, F1);
                    newStatus.key ^= piece_key[C][ROOK][H1];
                    newStatus.key ^= piece_key[C][ROOK][F1];
                }
                else
                {
                    BB::toggle_bit2(colorPiecesBB[C],   H8, F8);
                    BB::toggle_bit2(typePiecesBB[ROOK], H8, F8);

                    pieceOn[H8] = NO_PIECE;
                    pieceOn[F8] = ROOK;

                    assert(piece_on(F8) == ROOK);

                    if constexpr (Update_NNUE == true)
                        nnue.sub_add(C, ROOK, H8, ROOK, F8);
                    newStatus.key ^= piece_key[C][ROOK][H8];
                    newStatus.key ^= piece_key[C][ROOK][F8];
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
                
                pieceOn[from] = NO_PIECE;
                pieceOn[dest] = KING;

                assert(piece_on(get_king_dest<C, CastleSide::QUEEN_SIDE>()) == KING);

                if constexpr (Update_NNUE == true)
                    nnue.sub_add(C, piece, from, piece, dest);
                newStatus.key ^= piece_key[C][piece][from];
                newStatus.key ^= piece_key[C][piece][dest];

                // Move the Rook
                if constexpr (C == WHITE)
                {
                    BB::toggle_bit2(colorPiecesBB[C],   A1, D1);
                    BB::toggle_bit2(typePiecesBB[ROOK], A1, D1);

                    pieceOn[A1] = NO_PIECE;
                    pieceOn[D1] = ROOK;

                    assert(piece_on(D1) == ROOK);

                    if constexpr (Update_NNUE == true)
                        nnue.sub_add(C, ROOK, A1, ROOK, D1);
                    newStatus.key ^= piece_key[C][ROOK][A1];
                    newStatus.key ^= piece_key[C][ROOK][D1];
                }
                else
                {
                    BB::toggle_bit2(colorPiecesBB[C],   A8, D8);
                    BB::toggle_bit2(typePiecesBB[ROOK], A8, D8);

                    pieceOn[A8] = NO_PIECE;
                    pieceOn[D8] = ROOK;

                    assert(piece_on(D8) == ROOK);

                    if constexpr (Update_NNUE == true)
                        nnue.sub_add(C, ROOK, A8, ROOK, D8);
                    newStatus.key ^= piece_key[C][ROOK][A8];
                    newStatus.key ^= piece_key[C][ROOK][D8];
                }
            }
        } // Roques
    } // Special

    // Swap sides
    side_to_move = ~side_to_move;

    // pièces donnant échec, pièces clouées
    // attention : on a changé de camp !!!
    calculate_checkers_pinned<~C>();

#if defined DEBUG_HASH
    U64 hash_1, hash_2;
    calculate_hash(hash_1, hash_2);

    if (hash_1 != get_status().hash)
    {
        std::cout << Move::name(move) << "  : hash pb : hash = " << get_status().hash << " ; calc = " << hash_1 << std::endl;
        std::cout << display() << std::endl << std::endl;
    }
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
    const Status& previousStatus = StatusHistory.back();

    StatusHistory.emplace_back(
                                previousStatus.key ^ side_key,  // zobrist will be changed later
                                Move::MOVE_NULL,
                                NO_SQUARE,                                               // reset en passant. might be set later
                                previousStatus.castling,                                 // copy meta. might be changed
                                previousStatus.fiftymove_counter + 1,                     // increment fifty move counter. might be reset
                                previousStatus.fullmove_counter + (C == Color::BLACK),   // increment move counter
                                0ULL,
                                0ULL);

    Status& newStatus = StatusHistory.back();

    // La prise en passant n'est valable que tout de suite
    // Il faut donc la supprimer
    if (previousStatus.ep_square != NO_SQUARE)
        newStatus.key ^= ep_key[previousStatus.ep_square];

    // Swap sides
    side_to_move = ~side_to_move;

    // attention : on a changé de camp !!!
    calculate_checkers_pinned<~C>();

#ifndef NDEBUG
    // on ne passe ici qu'en debug
    valid();
#endif
}

//=============================================================================
//! \brief  Met une pièce à la case indiquée
//-----------------------------------------------------------------------------
template <bool Update_NNUE>
void Board::set_piece(const int square, const Color color, const PieceType piece) noexcept
{
    colorPiecesBB[color] |= SQ::square_BB(square);
    typePiecesBB[piece]  |= SQ::square_BB(square);
    pieceOn[square]       = piece;

    if constexpr (Update_NNUE == true)
        nnue.add(color, piece, square);
}


// Explicit instantiations.
template void Board::make_move<WHITE, true>(const U32 move) noexcept;
template void Board::make_move<BLACK, true>(const U32 move) noexcept;
template void Board::make_move<WHITE, false>(const U32 move) noexcept;
template void Board::make_move<BLACK, false>(const U32 move) noexcept;

template void Board::make_nullmove<WHITE>() noexcept;
template void Board::make_nullmove<BLACK>() noexcept;

template void Board::set_piece<true>(const int square, const Color color, const PieceType piece) noexcept;
template void Board::set_piece<false>(const int square, const Color color, const PieceType piece) noexcept;
