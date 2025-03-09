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
    const auto promoted = Move::promotion(move);

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

    assert(pieceBoard[king_square<C>()] == KING);
    assert(dest != from);
    assert(piece != PIECE_NONE);
    assert(piece == pieceBoard[from]);
    assert(captured != KING);
    assert(promoted != PAWN);
    assert(promoted != KING);
    assert(!StatusHistory.empty());

    if constexpr (Update_NNUE == true)
        nnue.push();    // nouvel accumulateur

    const Status& previousStatus = StatusHistory.back(); // référence

    StatusHistory.emplace_back( // ajoute un nouvel élément à la fin
                                previousStatus.key ^ side_key,                              // zobrist will be changed later
                                move,
                                SQUARE_NONE,                                                  // reset en passant. might be set later
                                previousStatus.castling,                                    // copy meta. might be changed
                                previousStatus.fiftymove_counter + 1,                        // increment fifty move counter. might be reset
                                previousStatus.fullmove_counter + (C == Color::BLACK),      // increment move counter
                                0ULL,
                                0ULL
        );

    Status& newStatus = StatusHistory.back();

    // La prise en passant n'est valable que tout de suite
    // Il faut donc la supprimer
    if (previousStatus.ep_square != SQUARE_NONE)
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
            assert(captured == PIECE_NONE);
            assert(promoted == PIECE_NONE);

            move_piece(from, dest, C, piece);

            newStatus.key ^= piece_key[C][piece][from] ^ piece_key[C][piece][dest];
            if (piece == PAWN)
                newStatus.fiftymove_counter = 0;

            if constexpr (Update_NNUE == true)
                nnue.sub_add(C, piece, from, piece, dest);
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
                assert(captured != PIECE_NONE);
                assert(promoted != PIECE_NONE);
                assert(SQ::file(dest) != SQ::file(from));
                assert((C == Color::WHITE && SQ::rank(dest) == 7) || (C == Color::BLACK && SQ::rank(dest) == 0));
                assert((C == Color::WHITE && SQ::rank(from) == 6) || (C == Color::BLACK && SQ::rank(from) == 1));


                promocapt_piece(from, dest, C, captured, promoted);

                newStatus.key ^= piece_key[C][piece][from];
                newStatus.key ^= piece_key[C][promoted][dest];
                newStatus.key ^= piece_key[Them][captured][dest];
                newStatus.fiftymove_counter = 0;

                if constexpr (Update_NNUE == true)
                    nnue.sub_sub_add(C, PAWN, from, captured, promoted, dest);
            }

            //====================================================================================
            //  Capture simple
            //------------------------------------------------------------------------------------
            else
            {
                assert(captured != PIECE_NONE);
                assert(promoted == PIECE_NONE);

                capture_piece(from, dest, C, piece, captured);

                newStatus.key ^= piece_key[C][piece][from] ^ piece_key[C][piece][dest];
                newStatus.key ^= piece_key[Them][captured][dest];
                newStatus.fiftymove_counter = 0;

                if constexpr (Update_NNUE == true)
                    nnue.sub_sub_add(C, piece, from, captured, piece, dest);
            }
        }

        //====================================================================================
        //  Promotion simple
        //------------------------------------------------------------------------------------
        else if (Move::is_promoting(move))
        {
            assert(piece    == PAWN);
            assert(captured == PIECE_NONE);
            assert(promoted != PIECE_NONE);
            assert(SQ::file(dest) == SQ::file(from));
            assert((C == Color::WHITE && SQ::rank(dest) == 7) || (C == Color::BLACK && SQ::rank(dest) == 0));
            assert((C == Color::WHITE && SQ::rank(from) == 6) || (C == Color::BLACK && SQ::rank(from) == 1));

            promotion_piece(from, dest, C, promoted);

            newStatus.key ^= piece_key[C][piece][from];
            newStatus.key ^= piece_key[C][promoted][dest];
            newStatus.fiftymove_counter = 0;

            if constexpr (Update_NNUE == true)
                nnue.sub_add(C, PAWN, from, promoted, dest);
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
            assert(captured == PIECE_NONE);
            assert(promoted == PIECE_NONE);
            assert(SQ::file(dest) == SQ::file(from));
            assert((C == Color::WHITE && SQ::rank(dest) == 3) || (C == Color::BLACK && SQ::rank(dest) == 4));
            assert((C == Color::WHITE && SQ::rank(from) == 1) || (C == Color::BLACK && SQ::rank(from) == 6));

            move_piece(from, dest, C, PAWN);

            newStatus.key ^= piece_key[C][PAWN][from] ^ piece_key[C][PAWN][dest];
            newStatus.fiftymove_counter = 0;

            if constexpr (Update_NNUE == true)
                nnue.sub_add(C, PAWN, from, PAWN, dest);

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
            assert(promoted == PIECE_NONE);
            //TODO assert(SQ::file(dest) == SQ::file(ep_old));
            assert((C == Color::WHITE && SQ::rank(dest) == 5)   || (C == Color::BLACK && SQ::rank(dest) == 2));
            assert((C == Color::WHITE && SQ::rank(from) == 4) || (C == Color::BLACK && SQ::rank(from) == 3));
            assert(SQ::file(dest) - SQ::file(from) == 1     || SQ::file(from) - SQ::file(dest) == 1);

            move_piece(from, dest, C, PAWN);

            newStatus.key ^= piece_key[C][PAWN][from] ^ piece_key[C][PAWN][dest];
            newStatus.fiftymove_counter = 0;

            // Remove the captured pawn
            if constexpr (C == Color::WHITE)
            {
                remove_piece(SQ::south(dest), Color::BLACK, PAWN);

                newStatus.key ^= piece_key[Them][PAWN][SQ::south(dest)];
                if constexpr (Update_NNUE == true)
                    nnue.sub_sub_add(C, PAWN, from, PAWN, SQ::south(dest), PAWN, dest);
             }
            else
            {
                remove_piece(SQ::north(dest), Color::WHITE, PAWN);

                newStatus.key ^= piece_key[Them][PAWN][SQ::north(dest)];
                if constexpr (Update_NNUE == true)
                    nnue.sub_sub_add(C, PAWN, from, PAWN, SQ::north(dest), PAWN, dest);
           }
        }

        //====================================================================================
        //  Roques
        //------------------------------------------------------------------------------------
        else if (Move::is_castling(move))
        {
            assert(piece    == KING);
            assert(captured == PIECE_NONE);
            assert(promoted == PIECE_NONE);
            assert(pieceBoard[from] == KING);
            assert(pieceBoard[dest] == PIECE_NONE);

            //====================================================================================
            //  Petit Roque
            //------------------------------------------------------------------------------------
            if ((SQ::square_BB(dest)) & FILE_G_BB)
            {
                assert((dest == get_king_dest<C, CastleSide::KING_SIDE>())  );

                // Move the King
                move_piece(from, dest, C, KING);

                assert(piece_on(get_king_dest<C, CastleSide::KING_SIDE>()) == KING);

                newStatus.key ^= piece_key[C][piece][from];
                newStatus.key ^= piece_key[C][piece][dest];
                if constexpr (Update_NNUE == true)
                    nnue.sub_add(C, piece, from, piece, dest);

                // Move the Rook
                if constexpr (C == WHITE)
                {
                    move_piece(H1, F1, C, ROOK);

                    assert(piece_on(F1) == ROOK);

                    newStatus.key ^= piece_key[C][ROOK][H1];
                    newStatus.key ^= piece_key[C][ROOK][F1];
                    if constexpr (Update_NNUE == true)
                        nnue.sub_add(C, ROOK, H1, ROOK, F1);
                }
                else
                {
                    move_piece(H8, F8, C, ROOK);

                    assert(piece_on(F8) == ROOK);

                    newStatus.key ^= piece_key[C][ROOK][H8];
                    newStatus.key ^= piece_key[C][ROOK][F8];
                    if constexpr (Update_NNUE == true)
                        nnue.sub_add(C, ROOK, H8, ROOK, F8);
                 }
            }

            //====================================================================================
            //  Grand Roque
            //------------------------------------------------------------------------------------
            else if ((SQ::square_BB(dest)) & FILE_C_BB)
            {
                assert((dest == get_king_dest<C, CastleSide::QUEEN_SIDE>() ));

                // Move the King
                move_piece(from, dest, C, KING);

                assert(piece_on(get_king_dest<C, CastleSide::QUEEN_SIDE>()) == KING);

                newStatus.key ^= piece_key[C][piece][from];
                newStatus.key ^= piece_key[C][piece][dest];
                if constexpr (Update_NNUE == true)
                    nnue.sub_add(C, piece, from, piece, dest);

                // Move the Rook
                if constexpr (C == WHITE)
                {
                    move_piece(A1, D1, C, ROOK);

                    assert(piece_on(D1) == ROOK);

                    newStatus.key ^= piece_key[C][ROOK][A1];
                    newStatus.key ^= piece_key[C][ROOK][D1];
                    if constexpr (Update_NNUE == true)
                        nnue.sub_add(C, ROOK, A1, ROOK, D1);
                 }
                else
                {
                    move_piece(A8, D8, C, ROOK);

                    assert(piece_on(D8) == ROOK);

                    newStatus.key ^= piece_key[C][ROOK][A8];
                    newStatus.key ^= piece_key[C][ROOK][D8];
                    if constexpr (Update_NNUE == true)
                        nnue.sub_add(C, ROOK, A8, ROOK, D8);
                }
            }
        } // Roques
    } // Special

    // Swap sides
    side_to_move = ~side_to_move;

    // pièces donnant échec, pièces clouées
    // attention : on a changé de camp !!!
    calculate_checkers_pinned<~C>();

#ifndef NDEBUG
    // on ne passe ici qu'en debug
    assert(valid<Update_NNUE>(" après make_move"));
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
                                SQUARE_NONE,                                               // reset en passant. might be set later
                                previousStatus.castling,                                 // copy meta. might be changed
                                previousStatus.fiftymove_counter + 1,                     // increment fifty move counter. might be reset
                                previousStatus.fullmove_counter + (C == Color::BLACK),   // increment move counter
                                0ULL,
                                0ULL);

    Status& newStatus = StatusHistory.back();

    // La prise en passant n'est valable que tout de suite
    // Il faut donc la supprimer
    if (previousStatus.ep_square != SQUARE_NONE)
        newStatus.key ^= ep_key[previousStatus.ep_square];

    // Swap sides
    side_to_move = ~side_to_move;

    // attention : on a changé de camp !!!
    calculate_checkers_pinned<~C>();

#ifndef NDEBUG
    // on ne passe ici qu'en debug
    assert(valid<false>("après make null_move"));
#endif
}

//=============================================================================
//! \brief  Met une pièce à la case indiquée
//-----------------------------------------------------------------------------
template <bool Update_NNUE>
void Board::add_piece(const int square, const Color color, const PieceType piece) noexcept
{
    colorPiecesBB[color] |= SQ::square_BB(square);
    typePiecesBB[piece]  |= SQ::square_BB(square);
    pieceBoard[square]       = piece;

    if constexpr (Update_NNUE == true)
        nnue.add(color, piece, square);
}

void Board::set_piece(const int square, const Color color, const PieceType piece) noexcept
{
    BB::toggle_bit(colorPiecesBB[color], square);
    BB::toggle_bit(typePiecesBB[piece],  square);
    pieceBoard[square] = piece;
}

void Board::remove_piece(const int square, const Color color, const PieceType piece) noexcept
{
    BB::toggle_bit(colorPiecesBB[color], square);
    BB::toggle_bit(typePiecesBB[piece], square);
    pieceBoard[square] = PieceType::PIECE_NONE;
}

//=============================================================================
//! \brief  Déplace une pièce à la case indiquée
//-----------------------------------------------------------------------------
void Board::move_piece(const int from,
                       const int dest,
                       const Color color,
                       const PieceType piece) noexcept
{
    BB::toggle_bit2(colorPiecesBB[color], from, dest);
    BB::toggle_bit2(typePiecesBB[piece], from, dest);
    pieceBoard[from] = PieceType::PIECE_NONE;
    pieceBoard[dest] = piece;
}

//=============================================================================
//! \brief  Déplace une pièce à la case indiquée
//-----------------------------------------------------------------------------
void Board::promotion_piece(const int from,
                            const int dest,
                            const Color color,
                            const PieceType promo) noexcept
{
    BB::toggle_bit(typePiecesBB[PAWN], from); // suppression du pion
    BB::toggle_bit(colorPiecesBB[color], from);

    BB::toggle_bit(typePiecesBB[promo], dest); // Ajoute la pièce promue
    BB::toggle_bit(colorPiecesBB[color], dest);

    pieceBoard[from] = PieceType::PIECE_NONE;
    pieceBoard[dest] = promo;
}

void Board::promocapt_piece(const int from,
                            const int dest,
                            const Color color,
                            const PieceType captured,
                            const PieceType promoted) noexcept
{
    BB::toggle_bit(typePiecesBB[PAWN], from); // suppression du pion
    BB::toggle_bit(colorPiecesBB[color], from);

    BB::toggle_bit(typePiecesBB[captured], dest); // Remove the captured piece
    BB::toggle_bit(colorPiecesBB[~color], dest);

    BB::toggle_bit(typePiecesBB[promoted], dest); // Ajoute la pièce promue
    BB::toggle_bit(colorPiecesBB[color], dest);

    pieceBoard[from] = PieceType::PIECE_NONE;
    pieceBoard[dest] = promoted;
}

//=============================================================================
//! \brief  Déplace une pièce à la case indiquée
//-----------------------------------------------------------------------------
void Board::capture_piece(const int from,
                          const int dest,
                          const Color color,
                          const PieceType piece,
                          const PieceType captured) noexcept
{
    BB::toggle_bit2(colorPiecesBB[color], from, dest);
    BB::toggle_bit2(typePiecesBB[piece], from, dest);

    BB::toggle_bit(colorPiecesBB[~color], dest); //  suppression de la pièce prise
    BB::toggle_bit(typePiecesBB[captured], dest);

    pieceBoard[from] = PieceType::PIECE_NONE;
    pieceBoard[dest] = piece;
}


// Explicit instantiations.
template void Board::make_move<WHITE, true>(const U32 move) noexcept;
template void Board::make_move<BLACK, true>(const U32 move) noexcept;
template void Board::make_move<WHITE, false>(const U32 move) noexcept;
template void Board::make_move<BLACK, false>(const U32 move) noexcept;

template void Board::make_nullmove<WHITE>() noexcept;
template void Board::make_nullmove<BLACK>() noexcept;

template void Board::add_piece<true>(const int square, const Color color, const PieceType piece) noexcept;
template void Board::add_piece<false>(const int square, const Color color, const PieceType piece) noexcept;
