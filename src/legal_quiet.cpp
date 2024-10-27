#include "Board.h"
#include "Board.h"
#include "Attacks.h"
#include "Move.h"

constexpr int PUSH[] = {8, -8};



//=================================================================
//! \brief  Génération de tous les coups "quiet"
//!             + ni capture
//!             + ni promotion
//! \param  ml  Liste des coups dans laquelle on va stocker les coups
//!
//! algorithme de Mperft
//-----------------------------------------------------------------
template <Color C>
constexpr void Board::legal_quiet(MoveList& ml) noexcept
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

    constexpr int pawn_push = PUSH[C];
    const int *dir = allmask[K].direction;
    Bitboard pieceBB;
    Bitboard attackBB;
    int from, to, d;
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
            
            if (/*do_quiet && */ d == abs(pawn_push) && (BB::square_BB(to = from + pawn_push) & emptyBB))
            {
                add_quiet_move(ml, from, to, PAWN, Move::FLAG_NONE);
                if (SQ::is_on_second_rank<C>(from) && (BB::square_BB(to += pawn_push) & emptyBB))
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
                attackBB = Attacks::bishop_moves(from, occupiedBB) & emptyBB & DiagonalMask64[from];
                push_quiet_moves(ml, attackBB, from);
            }
            else if (d == 7)
            {
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
                attackBB = Attacks::rook_moves(from, occupiedBB) & emptyBB & RankMask64[from];
                push_quiet_moves(ml, attackBB, from);
            }
            else if (d == 8)
            {
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


    // pawn
    pieceBB = typePiecesBB[PAWN] & unpinnedBB;


    attackBB = (C ? pieceBB >> 8 : pieceBB << 8) & emptyBB;

    push_pawn_quiet_moves(ml, attackBB & ~PromotionRank[C], pawn_push, Move::FLAG_NONE);
    attackBB = (C ? (((pieceBB & RANK_BB[6]) >> 8) & ~occupiedBB) >> 8 : (((pieceBB & RANK_BB[1]) << 8) & ~occupiedBB) << 8) & emptyBB;
    push_pawn_quiet_moves(ml, attackBB, 2 * pawn_push, Move::FLAG_DOUBLE_MASK);

    // knight

    pieceBB = typePiecesBB[KNIGHT] & unpinnedBB;
    while (pieceBB)
    {
        from = BB::pop_lsb(pieceBB);
        attackBB = Attacks::knight_moves(from) & emptyBB;
        push_piece_quiet_moves(ml, attackBB, from, KNIGHT);
    }

    // bishop or queen
    pieceBB = bq & unpinnedBB;
    while (pieceBB)
    {
        from = BB::pop_lsb(pieceBB);
        attackBB = Attacks::bishop_moves(from, occupiedBB) & emptyBB;
        push_quiet_moves(ml, attackBB, from);
    }

    // rook or queen
    pieceBB = rq & unpinnedBB;
    while (pieceBB)
    {
        from = BB::pop_lsb(pieceBB);
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
    colorPiecesBB[C] ^= BB::square_BB(K);

    auto mask = Attacks::king_moves(K) & ~occupiedBB;
    while (mask)
    {
        to = BB::pop_lsb(mask);
        if (!square_attacked<Them>(to))
            add_quiet_move(ml, K, to, KING, Move::FLAG_NONE);
    }

    // remet le roi dans l'échiquier
    colorPiecesBB[C] ^= BB::square_BB(K);
}

// Explicit instantiations.
template void Board::legal_quiet<WHITE>(MoveList& ml)  noexcept;
template void Board::legal_quiet<BLACK>(MoveList& ml)  noexcept;

