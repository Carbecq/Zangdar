#include <cassert>
#include "Board.h"
#include "Move.h"
#include "NNUE.h"
#include "zobrist.h"

template <Color US, bool Update_NNUE>
void Board::make_move(const MOVE move) noexcept
{
    constexpr Color THEM     = ~US;

    const auto  dest     = Move::dest(move);
    const auto  from     = Move::from(move);
    const Piece piece    = Move::piece(move);
    const Piece captured = Move::captured(move);
    const Piece promoted = Move::promoted(move);

    // printf("------------------------------------------make move \n");
    // std::cout << display() << std::endl << std::endl;
    //    printf("move  = (%s) \n", Move::name(move).c_str());
    //    binary_print(move, "debut makemove");
    //    printf("side  = %s \n", side_name[Us].c_str());
    //    printf("from  = %s \n", square_name[from].c_str());
    //    printf("dest  = %s \n", square_name[dest].c_str());
    //    printf("piece = %c \n", pieceToChar(piece));
    //    printf("capt  = %d : pièce prise=%c \n", Move::is_capturing(move), pieceToChar(captured));
    //    printf("promo = %d : %c (%d) \n", Move::is_promoting(move), pieceToChar(promoted), Move::promoted(move));
    //    printf("flags = %u \n", Move::flags(move));

    assert(Move::type(piece_square[get_king_square<US>()]) == PieceType::KING);
    assert(dest     != from);
    assert(piece    != Piece::NONE);
    assert(piece    == piece_square[from]);
    assert(Move::type(captured) != PieceType::KING);
    assert(Move::type(promoted) != PieceType::PAWN);
    assert(Move::type(promoted) != PieceType::KING);
    assert(!statusHistory.empty());

    /*
              0b0fff PPPP CCCC MMMM DDDDDD FFFFFF
     * 00000000      1110 1110 0000 010000 001000
     *                         pion
     *               Noir-None
     *
     *
     */


    // Lazy Updates
    // On ne stocke que les modifications à faire,
    // mais on ne les applique pas.

    if constexpr (Update_NNUE == true)
    {
        nnue.push();    // nouvel accumulateur
    }
    Accumulator& accum = get_accumulator();
    DirtyPieces& dp = accum.dirtyPieces;

    dp.sub_1 = {from, piece};
    dp.add_1 = {dest, piece};

    const Status& previousStatus = statusHistory.back(); // précédent status (sous forme de référence)
    statusHistory.emplace_back(previousStatus);     // ajoute 1 élément à la fin
    Status& newStatus = statusHistory.back();

    newStatus.key ^= side_key;
    newStatus.move = move;
    newStatus.ep_square = SQUARE_NONE;
    newStatus.fiftymove_counter++;
    newStatus.fullmove_counter += (US == Color::BLACK);
    newStatus.checkers = 0ULL;
    newStatus.pinned   = 0ULL;

    // La prise en passant n'est valable que tout de suite
    // Il faut donc la supprimer
    if (previousStatus.ep_square != SQUARE_NONE)
        newStatus.key ^= ep_key[previousStatus.ep_square];

     // Déplacement du roi
    if (Move::type(piece) == PieceType::KING)
        king_square[US] = dest;

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
            assert(captured == Piece::NONE);
            assert(promoted == Piece::NONE);

            move_piece(from, dest, US, piece);

            newStatus.key ^= piece_key[static_cast<U32>(piece)][from] ^ piece_key[static_cast<U32>(piece)][dest];
            if (Move::type(piece) == PieceType::PAWN)
            {
                newStatus.fiftymove_counter = 0;
                newStatus.pawn_key ^= piece_key[static_cast<U32>(piece)][from] ^ piece_key[static_cast<U32>(piece)][dest];
            }
            else
            {
                newStatus.mat_key[US] ^= piece_key[static_cast<U32>(piece)][from] ^ piece_key[static_cast<U32>(piece)][dest];
            }

            if constexpr (Update_NNUE == true)
            {
                dp.type  = DirtyPieces::NORMAL;
                // nnue.sub_add(acc, acc, piece, from, piece, dest, wking, bking);
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
                assert(Move::type(piece) == PieceType::PAWN);
                assert(captured != Piece::NONE);
                assert(promoted != Piece::NONE);
                assert(SQ::file(dest) != SQ::file(from));
                assert((US == Color::WHITE && SQ::rank(dest) == 7) || (US == Color::BLACK && SQ::rank(dest) == 0));
                assert((US == Color::WHITE && SQ::rank(from) == 6) || (US == Color::BLACK && SQ::rank(from) == 1));


                promocapt_piece(from, dest, US, captured, promoted);

                newStatus.key         ^= piece_key[static_cast<U32>(piece)][from];  // pion disparait
                newStatus.pawn_key    ^= piece_key[static_cast<U32>(piece)][from];

                newStatus.key         ^= piece_key[static_cast<U32>(promoted)][dest];   // pièce promue apparait
                newStatus.mat_key[US] ^= piece_key[static_cast<U32>(promoted)][dest];


                newStatus.key           ^= piece_key[static_cast<U32>(captured)][dest]; // pièce prise disparait
                newStatus.mat_key[THEM] ^= piece_key[static_cast<U32>(captured)][dest];

                newStatus.fiftymove_counter = 0;

                if constexpr (Update_NNUE == true)
                {
                    assert(US == Move::color(piece));
                    assert(THEM == Move::color(captured));

                    // nnue.sub_sub_add(acc, acc, piece, from, captured, promoted, dest, wking, bking);
                    dp.type  = DirtyPieces::CAPTURE;
                    dp.sub_2 = {dest, captured};
                    dp.add_1.piece = promoted;
                }
            }

            //====================================================================================
            //  Capture simple
            //------------------------------------------------------------------------------------
            else
            {
                assert(captured != Piece::NONE);
                assert(promoted == Piece::NONE);

                capture_piece(from, dest, US, piece, captured);

                newStatus.key ^= piece_key[static_cast<U32>(piece)][from] ^ piece_key[static_cast<U32>(piece)][dest];
                if (Move::type(piece) == PieceType::PAWN)
                    newStatus.pawn_key    ^= piece_key[static_cast<U32>(piece)][from] ^ piece_key[static_cast<U32>(piece)][dest];
                else
                    newStatus.mat_key[US] ^= piece_key[static_cast<U32>(piece)][from] ^ piece_key[static_cast<U32>(piece)][dest];

                newStatus.key ^= piece_key[static_cast<U32>(captured)][dest];
                if (Move::type(captured) == PieceType::PAWN)
                    newStatus.pawn_key      ^= piece_key[static_cast<U32>(captured)][dest];
                else
                    newStatus.mat_key[THEM] ^= piece_key[static_cast<U32>(captured)][dest];

                newStatus.fiftymove_counter = 0;

                if constexpr (Update_NNUE == true)
                {
                    assert(US   == Move::color(piece));
                    assert(THEM == Move::color(captured));

                    dp.type  = DirtyPieces::CAPTURE;
                    dp.sub_2 = {dest, captured};
                    // nnue.sub_sub_add(acc, acc, piece, from, captured, piece, dest, wking, bking);
                }
            }
        }

        //====================================================================================
        //  Promotion simple
        //------------------------------------------------------------------------------------
        else if (Move::is_promoting(move))
        {
            assert(Move::type(piece) == PieceType::PAWN);
            assert(captured == Piece::NONE);
            assert(promoted != Piece::NONE);
            assert(SQ::file(dest) == SQ::file(from));
            assert((US == Color::WHITE && SQ::rank(dest) == 7) || (US == Color::BLACK && SQ::rank(dest) == 0));
            assert((US == Color::WHITE && SQ::rank(from) == 6) || (US == Color::BLACK && SQ::rank(from) == 1));

            promotion_piece(from, dest, US, promoted);

            newStatus.key      ^= piece_key[static_cast<U32>(piece)][from];
            newStatus.pawn_key ^= piece_key[static_cast<U32>(piece)][from];
            newStatus.key         ^= piece_key[static_cast<U32>(promoted)][dest];
            newStatus.mat_key[US] ^= piece_key[static_cast<U32>(promoted)][dest];
            newStatus.fiftymove_counter = 0;

            if constexpr (Update_NNUE == true)
            {
                dp.type  = DirtyPieces::NORMAL;
                dp.add_1.piece = promoted;
                // nnue.sub_add(acc, acc, piece, from, promoted, dest, wking, bking);
            }
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
            assert(Move::type(piece) == PieceType::PAWN);
            assert(captured == Piece::NONE);
            assert(promoted == Piece::NONE);
            assert(SQ::file(dest) == SQ::file(from));
            assert((US == Color::WHITE && SQ::rank(dest) == 3) || (US == Color::BLACK && SQ::rank(dest) == 4));
            assert((US == Color::WHITE && SQ::rank(from) == 1) || (US == Color::BLACK && SQ::rank(from) == 6));

            move_piece(from, dest, US, piece);

            newStatus.key      ^= piece_key[static_cast<U32>(piece)][from] ^ piece_key[static_cast<U32>(piece)][dest];
            newStatus.pawn_key ^= piece_key[static_cast<U32>(piece)][from] ^ piece_key[static_cast<U32>(piece)][dest];

            newStatus.fiftymove_counter = 0;

            if constexpr (Update_NNUE == true)
            {
                dp.type  = DirtyPieces::NORMAL;
                // nnue.sub_add(acc, acc, piece, from, piece, dest, wking, bking);
            }

            int new_ep;
            if constexpr (US == Color::WHITE)
                new_ep = SQ::south(dest);
            else
                new_ep = SQ::north(dest);

            if (!BB::empty(pawn_attackers<THEM>(new_ep))  )     // La case de prise en-passant est-elle attaquée par un pion ?
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
            assert(Move::type(piece)    == PieceType::PAWN);
            assert(Move::type(captured) == PieceType::PAWN);
            assert(promoted == Piece::NONE);
            //TODO assert(SQ::file(dest) == SQ::file(ep_old));
            assert((US == Color::WHITE && SQ::rank(dest) == 5) || (US == Color::BLACK && SQ::rank(dest) == 2));
            assert((US == Color::WHITE && SQ::rank(from) == 4) || (US == Color::BLACK && SQ::rank(from) == 3));
            assert(SQ::file(dest) - SQ::file(from) == 1       || SQ::file(from) - SQ::file(dest) == 1);

            move_piece(from, dest, US, piece);

            newStatus.key      ^= piece_key[static_cast<U32>(piece)][from] ^ piece_key[static_cast<U32>(piece)][dest];
            newStatus.pawn_key ^= piece_key[static_cast<U32>(piece)][from] ^ piece_key[static_cast<U32>(piece)][dest];
            newStatus.fiftymove_counter = 0;

            // Remove the captured pawn
            if constexpr (US == Color::WHITE)
            {
                remove_piece(SQ::south(dest), Color::BLACK, captured);

                newStatus.key      ^= piece_key[static_cast<U32>(captured)][SQ::south(dest)];
                newStatus.pawn_key ^= piece_key[static_cast<U32>(captured)][SQ::south(dest)];
                if constexpr (Update_NNUE == true)
                {
                    dp.type = DirtyPieces::CAPTURE;
                    dp.sub_2 = {SQ::south(dest), captured};
                    // nnue.sub_sub_add(acc, acc, piece, from, captured, SQ::south(dest), piece, dest, wking, bking);
                }
            }
            else
            {
                remove_piece(SQ::north(dest), Color::WHITE, captured);

                newStatus.key      ^= piece_key[static_cast<U32>(captured)][SQ::north(dest)];
                newStatus.pawn_key ^= piece_key[static_cast<U32>(captured)][SQ::north(dest)];
                if constexpr (Update_NNUE == true)
                {
                    dp.type = DirtyPieces::CAPTURE;
                    dp.sub_2 = {SQ::north(dest), captured};
                    // nnue.sub_sub_add(acc, acc, piece, from, captured, SQ::north(dest), piece, dest, wking, bking);
                }
            }
        }

        //====================================================================================
        //  Roques
        //------------------------------------------------------------------------------------
        else if (Move::is_castling(move))
        {
            assert(Move::type(piece) == PieceType::KING);
            assert(captured == Piece::NONE);
            assert(promoted == Piece::NONE);
            assert(Move::type(piece_square[from]) == PieceType::KING);
            assert(piece_square[dest] == Piece::NONE);

            //====================================================================================
            //  Petit Roque
            //------------------------------------------------------------------------------------
            if ((SQ::square_BB(dest)) & FILE_G_BB)
            {
                assert((dest == get_king_dest<US, CastleSide::KING_SIDE>())  );

                // Move the King
                move_piece(from, dest, US, piece);

                assert(Move::type(piece_on(get_king_dest<US, CastleSide::KING_SIDE>())) == PieceType::KING);

                newStatus.key         ^= piece_key[static_cast<U32>(piece)][from] ^ piece_key[static_cast<U32>(piece)][dest];
                newStatus.mat_key[US] ^= piece_key[static_cast<U32>(piece)][from] ^ piece_key[static_cast<U32>(piece)][dest];

                // if constexpr (Update_NNUE == true)
                // nnue.sub_add(acc, acc, piece, from, piece, dest, wking, bking);

                // Move the Rook
                if constexpr (US == WHITE)
                {
                    move_piece(H1, F1, US, Piece::WHITE_ROOK);

                    assert(piece_on(F1) == Piece::WHITE_ROOK);

                    newStatus.key         ^= piece_key[static_cast<U32>(Piece::WHITE_ROOK)][H1] ^ piece_key[static_cast<U32>(Piece::WHITE_ROOK)][F1];
                    newStatus.mat_key[US] ^= piece_key[static_cast<U32>(Piece::WHITE_ROOK)][H1] ^ piece_key[static_cast<U32>(Piece::WHITE_ROOK)][F1];
                    if constexpr (Update_NNUE == true)
                    {
                        dp.type = DirtyPieces::CASTLING;
                        dp.add_2 = {F1, Piece::WHITE_ROOK};
                        dp.sub_2 = {H1, Piece::WHITE_ROOK};
                        // nnue.sub_add(acc, acc, Piece::WHITE_ROOK, H1, Piece::WHITE_ROOK, F1, wking, bking);
                    }
                }
                else
                {
                    move_piece(H8, F8, US, Piece::BLACK_ROOK);

                    assert(piece_on(F8) == Piece::BLACK_ROOK);

                    newStatus.key         ^= piece_key[static_cast<U32>(Piece::BLACK_ROOK)][H8] ^ piece_key[static_cast<U32>(Piece::BLACK_ROOK)][F8];
                    newStatus.mat_key[US] ^= piece_key[static_cast<U32>(Piece::BLACK_ROOK)][H8] ^ piece_key[static_cast<U32>(Piece::BLACK_ROOK)][F8];
                    if constexpr (Update_NNUE == true)
                    {
                        dp.type = DirtyPieces::CASTLING;
                        dp.add_2 = {F8, Piece::BLACK_ROOK};
                        dp.sub_2 = {H8, Piece::BLACK_ROOK};
                        // nnue.sub_add(acc, acc, Piece::BLACK_ROOK, H8, Piece::BLACK_ROOK, F8, wking, bking);
                    }
                }
            }

            //====================================================================================
            //  Grand Roque
            //------------------------------------------------------------------------------------
            else if ((SQ::square_BB(dest)) & FILE_C_BB)
            {
                assert((dest == get_king_dest<US, CastleSide::QUEEN_SIDE>() ));

                // Move the King
                move_piece(from, dest, US, piece);

                assert(Move::type(piece_on(get_king_dest<US, CastleSide::QUEEN_SIDE>())) == PieceType::KING);

                newStatus.key         ^= piece_key[static_cast<U32>(piece)][from] ^ piece_key[static_cast<U32>(piece)][dest];
                newStatus.mat_key[US] ^= piece_key[static_cast<U32>(piece)][from] ^ piece_key[static_cast<U32>(piece)][dest];
                // if constexpr (Update_NNUE == true)
                // nnue.sub_add(acc, acc, piece, from, piece, dest, wking, bking);

                // Move the Rook
                if constexpr (US == WHITE)
                {
                    move_piece(A1, D1, US, Piece::WHITE_ROOK);

                    assert(piece_on(D1) == Piece::WHITE_ROOK);

                    newStatus.key         ^= piece_key[static_cast<U32>(Piece::WHITE_ROOK)][A1] ^ piece_key[static_cast<U32>(Piece::WHITE_ROOK)][D1];
                    newStatus.mat_key[US] ^= piece_key[static_cast<U32>(Piece::WHITE_ROOK)][A1] ^ piece_key[static_cast<U32>(Piece::WHITE_ROOK)][D1];
                    if constexpr (Update_NNUE == true)
                    {
                        dp.type = DirtyPieces::CASTLING;
                        dp.add_2 = {D1, Piece::WHITE_ROOK};
                        dp.sub_2 = {A1, Piece::WHITE_ROOK};
                        // nnue.sub_add(acc, acc, Piece::WHITE_ROOK, A1, Piece::WHITE_ROOK, D1, wking, bking);
                    }
                }
                else
                {
                    move_piece(A8, D8, US, Piece::BLACK_ROOK);

                    assert(piece_on(D8) == Piece::BLACK_ROOK);

                    newStatus.key         ^= piece_key[static_cast<U32>(Piece::BLACK_ROOK)][A8] ^ piece_key[static_cast<U32>(Piece::BLACK_ROOK)][D8];
                    newStatus.mat_key[US] ^= piece_key[static_cast<U32>(Piece::BLACK_ROOK)][A8] ^ piece_key[static_cast<U32>(Piece::BLACK_ROOK)][D8];
                    if constexpr (Update_NNUE == true)
                    {
                        dp.type = DirtyPieces::CASTLING;
                        dp.add_2 = {D8, Piece::BLACK_ROOK};
                        dp.sub_2 = {A8, Piece::BLACK_ROOK};
                        // nnue.sub_add(acc, acc, Piece::BLACK_ROOK, A8, Piece::BLACK_ROOK, D8, wking, bking);
                    }
                }
            }
        } // Roques
    } // Special

    // Swap sides
    side_to_move = ~side_to_move;

    // pièces donnant échec, pièces clouées
    // attention : on a changé de camp !!!
    calculate_checkers_pinned<THEM>();

    // Push the accumulator change
    accum.updated[WHITE] = false;
    accum.updated[BLACK] = false;
    accum.king_square   = king_square;

#ifndef NDEBUG
    // on ne passe ici qu'en debug
    if (valid<false>(" après make_move") == false)
    {
        printf("------------------------------------------make move \n");
        std::cout << display() << std::endl << std::endl;
        printf("move  = (%s) \n", Move::name(move).c_str());
        binary_print(move, "debut makemove");
        printf("side  = %s \n", side_name[US].c_str());
        printf("from  = %s \n", square_name[from].c_str());
        printf("dest  = %s \n", square_name[dest].c_str());
        printf("piece = %c \n", pieceToChar(piece));
        printf("capt  = %d : pièce prise  = (%c) \n", Move::is_capturing(move), pieceToChar(captured));
        printf("promo = %d : pièce promue = (%c) \n", Move::is_promoting(move), pieceToChar(promoted));
        printf("flags = %u \n", Move::flags(move));

        assert(false);
    }

#endif
}


//=======================================================================
//! \brief un Null Move est un coup où on passe son tour.
//! la position ne change pas, excepté le camp qui change.
//-----------------------------------------------------------------------
template <Color Us> void Board::make_nullmove() noexcept
{
    constexpr Color Them     = ~Us;

    const Status& previousStatus = statusHistory.back();
    statusHistory.emplace_back(previousStatus);

    // statusHistory.emplace_back(
    //                             previousStatus.key ^ side_key,  // zobrist will be changed later
    //                             previousStatus.pawn_key,
    //                             Move::MOVE_NULL,
    //                             SQUARE_NONE,                                               // reset en passant. might be set later
    //                             previousStatus.castling,                                 // copy meta. might be changed
    //                             previousStatus.fiftymove_counter + 1,                     // increment fifty move counter. might be reset
    //                             previousStatus.fullmove_counter + (color == Color::BLACK),   // increment move counter
    //                             0ULL,
    //                             0ULL);

    Status& newStatus = statusHistory.back();

    newStatus.key ^= side_key;
    newStatus.move = Move::MOVE_NULL;
    newStatus.ep_square = SQUARE_NONE;
    newStatus.fiftymove_counter++;
    newStatus.fullmove_counter += (Us == Color::BLACK);
    newStatus.checkers = 0ULL;
    newStatus.pinned   = 0ULL;


    // La prise en passant n'est valable que tout de suite
    // Il faut donc la supprimer
    if (previousStatus.ep_square != SQUARE_NONE)
        newStatus.key ^= ep_key[previousStatus.ep_square];

    // Swap sides
    side_to_move = ~side_to_move;

    // attention : on a changé de camp !!!
    calculate_checkers_pinned<Them>();

#ifndef NDEBUG
    // on ne passe ici qu'en debug
    assert(valid<false>("après make null_move"));
#endif
}

//=============================================================================
//! \brief  Met une pièce à la case indiquée
//-----------------------------------------------------------------------------
void Board::add_piece(const int square, const Color color, const Piece piece) noexcept
{
    colorPiecesBB[color] |= SQ::square_BB(square);
    typePiecesBB[static_cast<U32>(Move::type(piece))]  |= SQ::square_BB(square);
    piece_square[square]    = piece;
}

void Board::set_piece(const int square, const Color color, const Piece piece) noexcept
{
    BB::toggle_bit(colorPiecesBB[color], square);
    BB::toggle_bit(typePiecesBB[static_cast<U32>(Move::type(piece))],  square);
    piece_square[square] = piece;
}

void Board::remove_piece(const int square, const Color color, const Piece piece) noexcept
{
    BB::toggle_bit(colorPiecesBB[color], square);
    BB::toggle_bit(typePiecesBB[static_cast<U32>(Move::type(piece))], square);
    piece_square[square] = Piece::NONE;
}

//=============================================================================
//! \brief  Déplace une pièce à la case indiquée
//-----------------------------------------------------------------------------
void Board::move_piece(const int from,
                       const int dest,
                       const Color color,
                       const Piece piece) noexcept
{
    BB::toggle_bit2(colorPiecesBB[color], from, dest);
    BB::toggle_bit2(typePiecesBB[static_cast<U32>(Move::type(piece))], from, dest);
    piece_square[from] = Piece::NONE;
    piece_square[dest] = piece;
}

//=============================================================================
//! \brief  Déplace une pièce à la case indiquée
//-----------------------------------------------------------------------------
void Board::promotion_piece(const int from,
                            const int dest,
                            const Color color,
                            const Piece promoted) noexcept
{
    BB::toggle_bit(typePiecesBB[static_cast<U32>(PieceType::PAWN)], from);  // suppression du pion
    BB::toggle_bit(colorPiecesBB[color], from);

    BB::toggle_bit(typePiecesBB[static_cast<U32>(Move::type(promoted))], dest);         // Ajoute la pièce promue
    BB::toggle_bit(colorPiecesBB[color], dest);

    piece_square[from] = Piece::NONE;
    piece_square[dest] = promoted;
}

void Board::promocapt_piece(const int from,
                            const int dest,
                            const Color color,
                            const Piece captured,
                            const Piece promoted) noexcept
{
    BB::toggle_bit(typePiecesBB[static_cast<U32>(PieceType::PAWN)], from); // suppression du pion
    BB::toggle_bit(colorPiecesBB[color], from);

    BB::toggle_bit(typePiecesBB[static_cast<U32>(Move::type(captured))], dest); // Remove the captured piece
    BB::toggle_bit(colorPiecesBB[~color], dest);

    BB::toggle_bit(typePiecesBB[static_cast<U32>(Move::type(promoted))], dest); // Ajoute la pièce promue
    BB::toggle_bit(colorPiecesBB[color], dest);

    piece_square[from] = Piece::NONE;
    piece_square[dest] = promoted;
}

//=============================================================================
//! \brief  Déplace une pièce à la case indiquée
//-----------------------------------------------------------------------------
void Board::capture_piece(const int from,
                          const int dest,
                          const Color color,
                          const Piece piece,
                          const Piece captured) noexcept
{
    BB::toggle_bit2(colorPiecesBB[color], from, dest);
    BB::toggle_bit2(typePiecesBB[static_cast<U32>(Move::type(piece))], from, dest);

    BB::toggle_bit(colorPiecesBB[~color], dest);                        //  suppression de la pièce prise
    BB::toggle_bit(typePiecesBB[static_cast<U32>(Move::type(captured))], dest);

    piece_square[from] = Piece::NONE;
    piece_square[dest] = piece;
}


// Explicit instantiations.
template void Board::make_move<WHITE, true>(const U32 move) noexcept;
template void Board::make_move<BLACK, true>(const U32 move) noexcept;
template void Board::make_move<WHITE, false>(const U32 move) noexcept;
template void Board::make_move<BLACK, false>(const U32 move) noexcept;

template void Board::make_nullmove<WHITE>() noexcept;
template void Board::make_nullmove<BLACK>() noexcept;

