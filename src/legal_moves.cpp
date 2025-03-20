#include "Board.h"
#include "Board.h"
#include "Attacks.h"
#include "Move.h"

constexpr int PUSH[] = {8, -8};



//=================================================================
//! \brief  Génération de tous les coups légaux
//! \param  ml  Liste des coups dans laquelle on va stocker les coups
//!
//! algorithme de Mperft
//-----------------------------------------------------------------
template <Color C, MoveGenType MGType>
void Board::legal_moves(MoveList& ml) noexcept
{
    constexpr Color Them = ~C;
    const int   K = king_square<C>();
    
    const Bitboard occupiedBB = occupancy_all();     // toutes les pièces (Blanches + Noires)
    Bitboard emptyBB    = ~occupiedBB;
    Bitboard enemyBB    = colorPiecesBB[Them];

    const Bitboard bq         = typePiecesBB[static_cast<U32>(PieceType::BISHOP)] | typePiecesBB[static_cast<U32>(PieceType::QUEEN)];
    const Bitboard rq         = typePiecesBB[static_cast<U32>(PieceType::ROOK)]   | typePiecesBB[static_cast<U32>(PieceType::QUEEN)];

    //-----------------------------------------------------------------------------------------
    //  Calcul des pièces clouées, et des échecs
    //  algorithme de Surge
    //-----------------------------------------------------------------------------------------

    const Bitboard checkersBB = get_status().checkers;
    const Bitboard pinnedBB   = get_status().pinned;
    const Bitboard unpinnedBB = colorPiecesBB[C] & ~pinnedBB;

    //-----------------------------------------------------------------------------------------

    constexpr int pawn_left = PUSH[C] - 1;
    constexpr int pawn_right = PUSH[C] + 1;
    constexpr int pawn_push = PUSH[C];
    const int *dir = DirectionMask[K].direction;

    Bitboard pieceBB;
    Bitboard attackBB;
    int from, to, d, ep;
    int x_checker = SquareType::SQUARE_NONE;

    //-------------------------------------------------------------------------------------------

    ml.clear();

    //    std::cout << "legal_gen 1 ; ch=%d " << BB::count_bit(checkersBB) << std::endl;

    // in check: capture or block the (single) checker if any;
    if (checkersBB)
    {
        if (BB::count_bit(checkersBB) == 1)
        {
            x_checker = BB::get_lsb(checkersBB);
            emptyBB = squares_between(K, x_checker);
            enemyBB = checkersBB;
        } else {
            emptyBB = enemyBB  = 0;
        }
    }
    else
    {
        // not in check: castling & pinned pieces moves

        // castling
        if constexpr (MGType & MoveGenType::QUIET)
        {
            gen_castle<C, CastleSide::KING_SIDE>(ml);
            gen_castle<C, CastleSide::QUEEN_SIDE>(ml);
        }

        // pawn (pinned)
        pieceBB = typePiecesBB[static_cast<U32>(PieceType::PAWN)] & pinnedBB;

        while (pieceBB) {
            from = BB::pop_lsb(pieceBB);
            d = dir[from];
            
            if constexpr (MGType & MoveGenType::NOISY)
            {
                if (d == abs(pawn_left) && (SQ::square_BB(to = from + pawn_left) & Attacks::pawn_attacks<C>(from) & enemyBB ))
                {
                    if (SQ::is_on_seventh_rank<C>(from))
                        push_capture_promotion(ml, from, to, C);
                    else
                        add_capture_move(ml, from, to, Move::make_piece(C, PieceType::PAWN), pieceBoard[to], Move::FLAG_NONE);
                }
                else if (d == abs(pawn_right) && (SQ::square_BB(to = from + pawn_right) & Attacks::pawn_attacks<C>(from) & enemyBB))
                {
                    if (SQ::is_on_seventh_rank<C>(from))
                        push_capture_promotion(ml, from, to, C);
                    else
                        add_capture_move(ml, from, to, Move::make_piece(C, PieceType::PAWN), pieceBoard[to], Move::FLAG_NONE);
                }
            }
            
            if constexpr (MGType & MoveGenType::QUIET)
            {
                if (d == abs(pawn_push) && (SQ::square_BB(to = from + pawn_push) & emptyBB))
                {
                    add_quiet_move(ml, from, to, Move::make_piece(C, PieceType::PAWN), Move::FLAG_NONE);
                    if (SQ::is_on_second_rank<C>(from) && (SQ::square_BB(to += pawn_push) & emptyBB))
                        add_quiet_move(ml, from, to, Move::make_piece(C, PieceType::PAWN), Move::FLAG_DOUBLE_MASK);
                }
            }
        }

        // bishop or queen (pinned)
        pieceBB = bq & pinnedBB;

        while (pieceBB)
        {
            from = BB::pop_lsb(pieceBB);
            d = dir[from];
            if (d == 9)
            {
                if constexpr (MGType & MoveGenType::NOISY)
                {
                    attackBB = Attacks::bishop_moves(from, occupiedBB) & enemyBB & DiagonalMask64[from];
                    push_capture_moves(ml, attackBB, from);
                }
                if constexpr (MGType & MoveGenType::QUIET)
                {
                    attackBB = Attacks::bishop_moves(from, occupiedBB) & emptyBB & DiagonalMask64[from];
                    push_quiet_moves(ml, attackBB, from);
                }
            }
            else if (d == 7)
            {
                if constexpr (MGType & MoveGenType::NOISY)
                {
                    attackBB = Attacks::bishop_moves(from, occupiedBB) & enemyBB & AntiDiagonalMask64[from];
                    push_capture_moves(ml, attackBB, from);
                }
                if constexpr (MGType & MoveGenType::QUIET)
                {
                    attackBB = Attacks::bishop_moves(from, occupiedBB) & emptyBB & AntiDiagonalMask64[from];
                    push_quiet_moves(ml, attackBB, from);
                }
            }
        }

        // rook or queen (pinned)
        pieceBB = rq & pinnedBB;

        while (pieceBB)
        {
            from = BB::pop_lsb(pieceBB);
            d = dir[from];
            if (d == 1)
            {
                if constexpr (MGType & MoveGenType::NOISY)
                {
                    attackBB = Attacks::rook_moves(from, occupiedBB) & enemyBB & RankMask64[from];
                    push_capture_moves(ml, attackBB, from);
                }
                if constexpr (MGType & MoveGenType::QUIET)
                {
                    attackBB = Attacks::rook_moves(from, occupiedBB) & emptyBB & RankMask64[from];
                    push_quiet_moves(ml, attackBB, from);
                }
            }
            else if (d == 8)
            {
                if constexpr (MGType & MoveGenType::NOISY)
                {
                    attackBB = Attacks::rook_moves(from, occupiedBB) & enemyBB & FileMask64[from];
                    push_capture_moves(ml, attackBB, from);
                }
                if constexpr (MGType & MoveGenType::QUIET)
                {
                    attackBB = Attacks::rook_moves(from, occupiedBB) & emptyBB & FileMask64[from];
                    push_quiet_moves(ml, attackBB, from);
                }
            }
        }
    }


    // common moves
    //    std::cout << "legal_gen common " << std::endl;

    // enpassant capture
    /*
     *      p P
     *
     *        P
     *
     * ep_square = e3 = 20
     * from      = d4 = 27
     * to
     *
     */
    if constexpr (MGType & MoveGenType::NOISY)
    {
        if (get_status().ep_square!=SquareType::SQUARE_NONE && (!checkersBB || x_checker == get_status().ep_square - pawn_push))
        {
            // file : a...h

            to = get_status().ep_square;             // e3 = 20
            ep = to - pawn_push;        // 20 - (-8)  noirs = 28 = e4
            from = ep - 1;              // 28 - 1 = 27 = d4

            Bitboard our_pawns = occupancy_cp<C, PieceType::PAWN>();
            if (SQ::file(to) > 0 && our_pawns & SQ::square_BB(from) )
            {
                pieceBB = occupiedBB ^ SQ::square_BB(from) ^ SQ::square_BB(ep) ^ SQ::square_BB(to);

                if (!(Attacks::bishop_moves(K, pieceBB) & bq & colorPiecesBB[Them]) &&
                    !(Attacks::rook_moves(K, pieceBB)   & rq & colorPiecesBB[Them]) )
                {
                    add_capture_move(ml, from, to,
                                     Move::make_piece(C, PieceType::PAWN),
                                     Move::make_piece(Them, PieceType::PAWN), Move::FLAG_ENPASSANT_MASK);
                }
            }

            from = ep + 1;          // 28 + 1 = 29 = f4
            if (SQ::file(to) < 7 && our_pawns & SQ::square_BB(from) )
            {
                pieceBB = occupiedBB ^ SQ::square_BB(from) ^ SQ::square_BB(ep) ^ SQ::square_BB(to);

                if ( !(Attacks::bishop_moves(K, pieceBB) & bq & colorPiecesBB[Them]) &&
                    !(Attacks::rook_moves(K, pieceBB)   & rq & colorPiecesBB[Them]) )
                {
                    add_capture_move(ml, from, to, Move::make_piece(C, PieceType::PAWN), Move::make_piece(Them, PieceType::PAWN), Move::FLAG_ENPASSANT_MASK);
                }
            }
        }
    }

    // pawn
    pieceBB = typePiecesBB[static_cast<U32>(PieceType::PAWN)] & unpinnedBB;

    if constexpr (MGType & MoveGenType::NOISY)
    {
        if constexpr (C==Color::WHITE)
            attackBB = ((pieceBB & ~FILE_A_BB) << 7) & enemyBB;
        else
            attackBB = ((pieceBB & ~FILE_A_BB) >> 9 ) & enemyBB;

        push_capture_promotions(ml, attackBB & PromotionRank[C], pawn_left, C);
        push_pawn_capture_moves(ml, attackBB & ~PromotionRank[C], pawn_left, C);

        if constexpr (C==Color::WHITE)
            attackBB = ((pieceBB & ~FILE_H_BB) << 9) & enemyBB;
        else
            attackBB = ((pieceBB & ~FILE_H_BB) >> 7) & enemyBB;

        push_capture_promotions(ml, attackBB & PromotionRank[C], pawn_right, C);
        push_pawn_capture_moves(ml, attackBB & ~PromotionRank[C], pawn_right, C);
    }

    if constexpr (C==Color::WHITE)
        attackBB = (pieceBB << 8) & emptyBB;
    else
        attackBB = (pieceBB >> 8) & emptyBB;

    if constexpr (MGType & MoveGenType::NOISY)
    {
        push_quiet_promotions(ml, attackBB & PromotionRank[C], pawn_push, C);
    }

    if constexpr (MGType & MoveGenType::QUIET)
    {
        push_pawn_quiet_moves(ml, attackBB & ~PromotionRank[C], pawn_push, C, Move::FLAG_NONE);

        if constexpr (C==Color::WHITE)
            attackBB = ((((pieceBB & RANK_2_BB) << 8) & ~occupiedBB) << 8) & emptyBB;
        else
            attackBB = ((((pieceBB & RANK_7_BB) >> 8) & ~occupiedBB) >> 8) & emptyBB;

        push_pawn_quiet_moves(ml, attackBB, 2 * pawn_push, C, Move::FLAG_DOUBLE_MASK);
    }

    // knight

    pieceBB = typePiecesBB[static_cast<U32>(PieceType::KNIGHT)] & unpinnedBB;
    while (pieceBB)
    {
        from = BB::pop_lsb(pieceBB);
        if constexpr (MGType & MoveGenType::NOISY)
        {
            attackBB = Attacks::knight_moves(from) & enemyBB;
            push_piece_capture_moves(ml, attackBB, from, C, PieceType::KNIGHT);
        }
        if constexpr (MGType & MoveGenType::QUIET)
        {
            attackBB = Attacks::knight_moves(from) & emptyBB;
            push_piece_quiet_moves(ml, attackBB, from, C, PieceType::KNIGHT);
        }
    }

    // bishop or queen
    pieceBB = bq & unpinnedBB;
    while (pieceBB)
    {
        from = BB::pop_lsb(pieceBB);
        if constexpr (MGType & MoveGenType::NOISY)
        {
            attackBB = Attacks::bishop_moves(from, occupiedBB) & enemyBB;
            push_capture_moves(ml, attackBB, from);
        }
        if constexpr (MGType & MoveGenType::QUIET)
        {
            attackBB = Attacks::bishop_moves(from, occupiedBB) & emptyBB;
            push_quiet_moves(ml, attackBB, from);
        }
    }

    // rook or queen
    pieceBB = rq & unpinnedBB;
    while (pieceBB)
    {
        from = BB::pop_lsb(pieceBB);
        if constexpr (MGType & MoveGenType::NOISY)
        {
            attackBB = Attacks::rook_moves(from, occupiedBB) & enemyBB;
            push_capture_moves(ml, attackBB, from);
        }
        if constexpr (MGType & MoveGenType::QUIET)
        {
            attackBB = Attacks::rook_moves(from, occupiedBB) & emptyBB;
            push_quiet_moves(ml, attackBB, from);
        }
    }

    // king
    // on enlève le roi de l'échiquier
    /*
     *   .R...t
     *
     *   dans cette position, si le roi va à gauche, il est toujours attaqué.
     *   si on laisse le roi dans l'échiquier, il ne sera pas attaqué
     */
    colorPiecesBB[C] ^= SQ::square_BB(K);

    if constexpr (MGType & MoveGenType::NOISY)
    {
        auto maskn = Attacks::king_moves(K) & colorPiecesBB[Them];
        while (maskn)
        {
            to = BB::pop_lsb(maskn);
            if (!square_attacked<Them>(to))
                add_capture_move(ml, K, to, Move::make_piece(C, PieceType::KING), pieceBoard[to], Move::FLAG_NONE);
        }
    }

    if constexpr (MGType & MoveGenType::QUIET)
    {
        auto maskq = Attacks::king_moves(K) & ~occupiedBB;
        while (maskq)
        {
            to = BB::pop_lsb(maskq);
            if (!square_attacked<Them>(to))
                add_quiet_move(ml, K, to, Move::make_piece(C, PieceType::KING), Move::FLAG_NONE);
        }
    }

    // remet le roi dans l'échiquier
    colorPiecesBB[C] ^= SQ::square_BB(K);
}

// Explicit instantiations.
template void Board::legal_moves<WHITE, MoveGenType::NOISY>(MoveList& ml)  noexcept;
template void Board::legal_moves<BLACK, MoveGenType::NOISY>(MoveList& ml)  noexcept;

template void Board::legal_moves<WHITE, MoveGenType::QUIET>(MoveList& ml)  noexcept;
template void Board::legal_moves<BLACK, MoveGenType::QUIET>(MoveList& ml)  noexcept;

template void Board::legal_moves<WHITE, MoveGenType::ALL>(MoveList& ml)  noexcept;
template void Board::legal_moves<BLACK, MoveGenType::ALL>(MoveList& ml)  noexcept;
