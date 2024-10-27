#include "Board.h"
#include "Bitboard.h"
#include "Attacks.h"
#include "Square.h"
#include "Move.h"

constexpr int PUSH[] = {8, -8};


//=================================================================
//! \brief  Génération de
//!     + toutes les captures légales
//!     + toutes les promotions
//!     + toutes les prises en passant
//!
//! \param  ml  Liste des coups dans laquelle on va stocker les coups
//!
//! algorithme de Mperft
//-----------------------------------------------------------------
template <Color C>
constexpr void Board::legal_noisy(MoveList& ml) noexcept
{
    constexpr Color Them = ~C;
    const int   K = x_king[C];
    
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
    const int *dir = allmask[K].direction;
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
                 *  BB::square_BB(ksc_castle_king_to[us])                   : case G1
                 */
            const Bitboard blockers         = occupancy_all() ^ BB::square_BB(K) ^ BB::square_BB(ksc_castle_rook_from[C]);
            const Bitboard king_path        = (squares_between(K, ksc_castle_king_to[C])  |
                                        BB::square_BB(ksc_castle_king_to[C])) ;
            const bool     king_path_clear  = BB::empty(king_path & blockers);
            const Bitboard rook_path        = squares_between(ksc_castle_rook_to[C], ksc_castle_rook_from[C])
                                       | BB::square_BB(ksc_castle_rook_to[C]);
            const bool     rook_path_clear  = BB::empty(rook_path & blockers);

            if (king_path_clear && rook_path_clear && !(squares_attacked<Them>() & king_path))
                add_quiet_move(ml, K, ksc_castle_king_to[C], KING, Move::FLAG_CASTLE_MASK);
        }
        if (can_castle_q<C>())
        {
            const Bitboard blockers         = occupancy_all() ^ BB::square_BB(K) ^ BB::square_BB(qsc_castle_rook_from[C]);
            const Bitboard king_path        = (squares_between(K, qsc_castle_king_to[C]) |
                                        BB::square_BB(qsc_castle_king_to[C]));
            const bool     king_path_clear  = BB::empty(king_path & blockers);
            const Bitboard rook_path        = squares_between(qsc_castle_rook_to[C], qsc_castle_rook_from[C])
                                       | BB::square_BB(qsc_castle_rook_to[C]);
            const bool     rook_path_clear = BB::empty(rook_path & blockers);

            if (king_path_clear && rook_path_clear && !(squares_attacked<Them>() & king_path))
                add_quiet_move(ml, K, qsc_castle_king_to[C], KING, Move::FLAG_CASTLE_MASK);
        }

        // pawn (pinned)
        pieceBB = typePiecesBB[PAWN] & pinnedBB;

        while (pieceBB) {
            from = BB::pop_lsb(pieceBB);
            d = dir[from];
            
            if (d == abs(pawn_left) && (BB::square_BB(to = from + pawn_left) & Attacks::pawn_attacks<C>(from) & enemyBB ))
            {
                if (SQ::is_on_seventh_rank<C>(from))
                    push_capture_promotion(ml, from, to);
                else
                    add_capture_move(ml, from, to, PAWN, pieceOn[to], Move::FLAG_NONE);
            }
            else if (d == abs(pawn_right) && (BB::square_BB(to = from + pawn_right) & Attacks::pawn_attacks<C>(from) & enemyBB))
            {
                if (SQ::is_on_seventh_rank<C>(from))
                    push_capture_promotion(ml, from, to);
                else
                    add_capture_move(ml, from, to, PAWN, pieceOn[to], Move::FLAG_NONE);
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
            }
            else if (d == 7)
            {
                attackBB = Attacks::bishop_moves(from, occupiedBB) & enemyBB & AntiDiagonalMask64[from];
                push_capture_moves(ml, attackBB, from);
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
            }
            else if (d == 8)
            {
                attackBB = Attacks::rook_moves(from, occupiedBB) & enemyBB & FileMask64[from];
                push_capture_moves(ml, attackBB, from);
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
        if (SQ::file(to) > 0 && our_pawns & BB::square_BB(from) )
        {
            pieceBB = occupiedBB ^ BB::square_BB(from) ^ BB::square_BB(ep) ^ BB::square_BB(to);

            if (!(Attacks::bishop_moves(K, pieceBB) & bq & colorPiecesBB[Them]) &&
                !(Attacks::rook_moves(K, pieceBB)   & rq & colorPiecesBB[Them]) )
            {
                add_capture_move(ml, from, to, PAWN, PAWN, Move::FLAG_ENPASSANT_MASK);
            }
        }

        from = ep + 1;          // 28 + 1 = 29 = f4
        if (SQ::file(to) < 7 && our_pawns & BB::square_BB(from) )
        {
            pieceBB = occupiedBB ^ BB::square_BB(from) ^ BB::square_BB(ep) ^ BB::square_BB(to);

            if ( !(Attacks::bishop_moves(K, pieceBB) & bq & colorPiecesBB[Them]) &&
                !(Attacks::rook_moves(K, pieceBB)   & rq & colorPiecesBB[Them]) )
            {
                add_capture_move(ml, from, to, PAWN, PAWN, Move::FLAG_ENPASSANT_MASK);
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


    // knight

    pieceBB = typePiecesBB[KNIGHT] & unpinnedBB;
    while (pieceBB)
    {
        from = BB::pop_lsb(pieceBB);
        attackBB = Attacks::knight_moves(from) & enemyBB;
        push_piece_capture_moves(ml, attackBB, from, KNIGHT);
    }

    // bishop or queen
    pieceBB = bq & unpinnedBB;
    while (pieceBB)
    {
        from = BB::pop_lsb(pieceBB);
        attackBB = Attacks::bishop_moves(from, occupiedBB) & enemyBB;
        push_capture_moves(ml, attackBB, from);
    }

    // rook or queen
    pieceBB = rq & unpinnedBB;
    while (pieceBB)
    {
        from = BB::pop_lsb(pieceBB);
        attackBB = Attacks::rook_moves(from, occupiedBB) & enemyBB;
        push_capture_moves(ml, attackBB, from);
    }

    // king
    // on enlève le roi de l'échiquier
    /*
     *   .R...t
     *
     *   dans cette position, si le roi va à gauche, il est toujours attaqué.
     *   si on laisse le roi dans l'échiquier, il ne sera pas attaqué
     */
    colorPiecesBB[C] ^= BB::square_BB(K);

    auto mask = Attacks::king_moves(K) & colorPiecesBB[Them];
    while (mask)
    {
        to = BB::pop_lsb(mask);
        if (!square_attacked<Them>(to))
            add_capture_move(ml, K, to, KING, pieceOn[to], Move::FLAG_NONE);
    }

    // remet le roi dans l'échiquier
    colorPiecesBB[C] ^= BB::square_BB(K);
}






// Explicit instantiations.
template void Board::legal_noisy<WHITE>(MoveList& ml)  noexcept;
template void Board::legal_noisy<BLACK>(MoveList& ml)  noexcept;

