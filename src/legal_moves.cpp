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
template <Color C>
constexpr void Board::legal_moves(MoveList& ml) noexcept
{
    constexpr Color Them = ~C;
    const int   K = king_square<C>();
    
    const Bitboard occupiedBB = occupancy_all();     // toutes les pièces (Blanches + Noires)
    Bitboard emptyBB    = ~occupiedBB;
    Bitboard enemyBB    = colorPiecesBB[Them];

    const Bitboard bq         = typePiecesBB[BISHOP] | typePiecesBB[QUEEN];
    const Bitboard rq         = typePiecesBB[ROOK]   | typePiecesBB[QUEEN];

    //-----------------------------------------------------------------------------------------
    //  Calcul des pièces clouées, et des échecs
    //  algorithme de Surge
    //-----------------------------------------------------------------------------------------

    Bitboard checkersBB = checkers;
    Bitboard pinnedBB   = pinned;

    //-----------------------------------------------------------------------------------------
    const Bitboard unpinnedBB = colorPiecesBB[C] & ~pinnedBB;

    constexpr int pawn_left = PUSH[C] - 1;
    constexpr int pawn_right = PUSH[C] + 1;
    constexpr int pawn_push = PUSH[C];
    const int *dir = DirectionMask[K].direction;
    Bitboard pieceBB;
    Bitboard attackBB;
    int from, to, d, ep;
    int x_checker = NO_SQUARE;

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
        if (can_castle_k<C>())
        {
            /*  squares_between(ksq, ksc_castle_king_to[us])   : case F1
                 *  SQ::square_BB(ksc_castle_king_to[us])                   : case G1
                 */
            const Bitboard blockers         = occupancy_all() ^ SQ::square_BB(K) ^ SQ::square_BB(ksc_castle_rook_from[C]);
            const Bitboard king_path        = (squares_between(K, ksc_castle_king_to[C])  |
                                        SQ::square_BB(ksc_castle_king_to[C])) ;
            const bool     king_path_clear  = BB::empty(king_path & blockers);
            const Bitboard rook_path        = squares_between(ksc_castle_rook_to[C], ksc_castle_rook_from[C])
                                       | SQ::square_BB(ksc_castle_rook_to[C]);
            const bool     rook_path_clear  = BB::empty(rook_path & blockers);

            if (king_path_clear && rook_path_clear && !(squares_attacked<Them>() & king_path))
                add_quiet_move(ml, K, ksc_castle_king_to[C], KING, Move::FLAG_CASTLE_MASK);
        }
        if (can_castle_q<C>())
        {
            const Bitboard blockers         = occupancy_all() ^ SQ::square_BB(K) ^ SQ::square_BB(qsc_castle_rook_from[C]);
            const Bitboard king_path        = (squares_between(K, qsc_castle_king_to[C]) |
                                        SQ::square_BB(qsc_castle_king_to[C]));
            const bool     king_path_clear  = BB::empty(king_path & blockers);
            const Bitboard rook_path        = squares_between(qsc_castle_rook_to[C], qsc_castle_rook_from[C])
                                       | SQ::square_BB(qsc_castle_rook_to[C]);
            const bool     rook_path_clear = BB::empty(rook_path & blockers);

            if (king_path_clear && rook_path_clear && !(squares_attacked<Them>() & king_path))
                add_quiet_move(ml, K, qsc_castle_king_to[C], KING, Move::FLAG_CASTLE_MASK);
        }

        // pawn (pinned)
        pieceBB = typePiecesBB[PAWN] & pinnedBB;

        while (pieceBB) {
            from = BB::pop_lsb(pieceBB);
            d = dir[from];
            
            if (d == abs(pawn_left) && (SQ::square_BB(to = from + pawn_left) & Attacks::pawn_attacks<C>(from) & enemyBB ))
            {
                if (SQ::is_on_seventh_rank<C>(from))
                    push_capture_promotion(ml, from, to);
                else
                    add_capture_move(ml, from, to, PAWN, pieceOn[to], Move::FLAG_NONE);
            }
            else if (d == abs(pawn_right) && (SQ::square_BB(to = from + pawn_right) & Attacks::pawn_attacks<C>(from) & enemyBB))
            {
                if (SQ::is_on_seventh_rank<C>(from))
                    push_capture_promotion(ml, from, to);
                else
                    add_capture_move(ml, from, to, PAWN, pieceOn[to], Move::FLAG_NONE);
            }
            
            if (d == abs(pawn_push) && (SQ::square_BB(to = from + pawn_push) & emptyBB))
            {
                add_quiet_move(ml, from, to, PAWN, Move::FLAG_NONE);
                if (SQ::is_on_second_rank<C>(from) && (SQ::square_BB(to += pawn_push) & emptyBB))
                    add_quiet_move(ml, from, to, PAWN, Move::FLAG_DOUBLE_MASK);
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
                attackBB = Attacks::bishop_moves(from, occupiedBB) & enemyBB & DiagonalMask64[from];
                push_capture_moves(ml, attackBB, from);
                attackBB = Attacks::bishop_moves(from, occupiedBB) & emptyBB & DiagonalMask64[from];
                push_quiet_moves(ml, attackBB, from);
            }
            else if (d == 7)
            {
                attackBB = Attacks::bishop_moves(from, occupiedBB) & enemyBB & AntiDiagonalMask64[from];
                push_capture_moves(ml, attackBB, from);
                attackBB = Attacks::bishop_moves(from, occupiedBB) & emptyBB & AntiDiagonalMask64[from];
                push_quiet_moves(ml, attackBB, from);
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
                attackBB = Attacks::rook_moves(from, occupiedBB) & enemyBB & RankMask64[from];
                push_capture_moves(ml, attackBB, from);
                attackBB = Attacks::rook_moves(from, occupiedBB) & emptyBB & RankMask64[from];
                push_quiet_moves(ml, attackBB, from);
            }
            else if (d == 8)
            {
                attackBB = Attacks::rook_moves(from, occupiedBB) & enemyBB & FileMask64[from];
                push_capture_moves(ml, attackBB, from);
                attackBB = Attacks::rook_moves(from, occupiedBB) & emptyBB & FileMask64[from];
                push_quiet_moves(ml, attackBB, from);
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

    if (ep_square!=NO_SQUARE && (!checkersBB || x_checker == ep_square - pawn_push))
    {
        // file : a...h

        to = ep_square;             // e3 = 20
        ep = to - pawn_push;        // 20 - (-8)  noirs = 28 = e4
        from = ep - 1;              // 28 - 1 = 27 = d4
        
        Bitboard our_pawns = occupancy_cp<C, PAWN>();
        if (SQ::file(to) > 0 && our_pawns & SQ::square_BB(from) )
        {
            pieceBB = occupiedBB ^ SQ::square_BB(from) ^ SQ::square_BB(ep) ^ SQ::square_BB(to);

            if (!(Attacks::bishop_moves(K, pieceBB) & bq & colorPiecesBB[Them]) &&
                !(Attacks::rook_moves(K, pieceBB)   & rq & colorPiecesBB[Them]) )
            {
                add_capture_move(ml, from, to, PAWN, PAWN, Move::FLAG_ENPASSANT_MASK);
//                add_quiet_move(ml, from, to, PAWN, Move::FLAG_ENPASSANT);
            }
        }

        from = ep + 1;          // 28 + 1 = 29 = f4
        if (SQ::file(to) < 7 && our_pawns & SQ::square_BB(from) )
        {
            pieceBB = occupiedBB ^ SQ::square_BB(from) ^ SQ::square_BB(ep) ^ SQ::square_BB(to);

            if ( !(Attacks::bishop_moves(K, pieceBB) & bq & colorPiecesBB[Them]) &&
                !(Attacks::rook_moves(K, pieceBB)   & rq & colorPiecesBB[Them]) )
            {
                add_capture_move(ml, from, to, PAWN, PAWN, Move::FLAG_ENPASSANT_MASK);
//                add_quiet_move(ml, from, to, PAWN, Move::FLAG_ENPASSANT);
            }
        }
    }


    // pawn
    pieceBB = typePiecesBB[PAWN] & unpinnedBB;

    attackBB = (C ? (pieceBB & ~FILE_BB[0]) >> 9 : (pieceBB & ~FILE_BB[0]) << 7) & enemyBB;
    push_capture_promotions(ml, attackBB & PromotionRank[C], pawn_left);
    push_pawn_capture_moves(ml, attackBB & ~PromotionRank[C], pawn_left);

    attackBB = (C ? (pieceBB & ~FILE_BB[7]) >> 7 : (pieceBB & ~FILE_BB[7]) << 9) & enemyBB;
    push_capture_promotions(ml, attackBB & PromotionRank[C], pawn_right);
    push_pawn_capture_moves(ml, attackBB & ~PromotionRank[C], pawn_right);

    attackBB = (C ? pieceBB >> 8 : pieceBB << 8) & emptyBB;
    push_quiet_promotions(ml, attackBB & PromotionRank[C], pawn_push);

    push_pawn_quiet_moves(ml, attackBB & ~PromotionRank[C], pawn_push, Move::FLAG_NONE);
    attackBB = (C ? (((pieceBB & RANK_BB[6]) >> 8) & ~occupiedBB) >> 8 : (((pieceBB & RANK_BB[1]) << 8) & ~occupiedBB) << 8) & emptyBB;
    push_pawn_quiet_moves(ml, attackBB, 2 * pawn_push, Move::FLAG_DOUBLE_MASK);

    // knight

    pieceBB = typePiecesBB[KNIGHT] & unpinnedBB;
    while (pieceBB)
    {
        from = BB::pop_lsb(pieceBB);
        attackBB = Attacks::knight_moves(from) & enemyBB;
        push_piece_capture_moves(ml, attackBB, from, KNIGHT);
        attackBB = Attacks::knight_moves(from) & emptyBB;
        push_piece_quiet_moves(ml, attackBB, from, KNIGHT);
    }

    // bishop or queen
    pieceBB = bq & unpinnedBB;
    while (pieceBB)
    {
        from = BB::pop_lsb(pieceBB);
        attackBB = Attacks::bishop_moves(from, occupiedBB) & enemyBB;
        push_capture_moves(ml, attackBB, from);
        attackBB = Attacks::bishop_moves(from, occupiedBB) & emptyBB;
        push_quiet_moves(ml, attackBB, from);
    }

    // rook or queen
    pieceBB = rq & unpinnedBB;
    while (pieceBB)
    {
        from = BB::pop_lsb(pieceBB);
        attackBB = Attacks::rook_moves(from, occupiedBB) & enemyBB;
        push_capture_moves(ml, attackBB, from);
        attackBB = Attacks::rook_moves(from, occupiedBB) & emptyBB;
        push_quiet_moves(ml, attackBB, from);
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

    auto mask = Attacks::king_moves(K) & colorPiecesBB[Them];
    while (mask)
    {
        to = BB::pop_lsb(mask);
        if (!square_attacked<Them>(to))
            add_capture_move(ml, K, to, KING, pieceOn[to], Move::FLAG_NONE);
    }
    mask = Attacks::king_moves(K) & ~occupiedBB;
    while (mask)
    {
        to = BB::pop_lsb(mask);
        if (!square_attacked<Them>(to))
            add_quiet_move(ml, K, to, KING, Move::FLAG_NONE);
    }

    // remet le roi dans l'échiquier
    colorPiecesBB[C] ^= SQ::square_BB(K);
}

// Explicit instantiations.
template void Board::legal_moves<WHITE>(MoveList& ml)  noexcept;
template void Board::legal_moves<BLACK>(MoveList& ml)  noexcept;

