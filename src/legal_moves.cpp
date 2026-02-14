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
template <Color US, MoveGenType MGType>
void Board::legal_moves(MoveList& ml) noexcept
{
    constexpr Color THEM = ~US;
    constexpr bool GenNoisy = (MGType & MoveGenType::NOISY) != 0;
    constexpr bool GenQuiet = (MGType & MoveGenType::QUIET) != 0;

    const U32     K = get_king_square<US>();
    
    const Bitboard occupiedBB = occupancy_all();        // toutes les pièces (Blanches + Noires)
    Bitboard emptyBB          = ~occupiedBB;            // ATTENTION emptyBB sera redéfini
    Bitboard enemyBB          = colorPiecesBB[THEM];

    const Bitboard bq         = typePiecesBB[PieceType::BISHOP] | typePiecesBB[PieceType::QUEEN];
    const Bitboard rq         = typePiecesBB[PieceType::ROOK]   | typePiecesBB[PieceType::QUEEN];

    //-----------------------------------------------------------------------------------------
    //  Calcul des pièces clouées, et des échecs
    //  algorithme de Surge
    //-----------------------------------------------------------------------------------------

    const Bitboard checkersBB = get_status().checkers;
    const Bitboard pinnedBB   = get_status().pinned;
    const Bitboard unpinnedBB = colorPiecesBB[US] & ~pinnedBB;

    //-----------------------------------------------------------------------------------------

    constexpr int pawn_left  = PUSH[US] - 1;
    constexpr int pawn_right = PUSH[US] + 1;
    constexpr int pawn_push  = PUSH[US];

    const int abs_pawn_left  = abs(PUSH[US] - 1);
    const int abs_pawn_right = abs(PUSH[US] + 1);
    const int abs_pawn_push  = abs(PUSH[US]);

    const int *dir = DirectionMask[K].direction;

    Bitboard pieceBB;
    Bitboard attackBB;
    U32 from, to;
    int d;
    U32 ep;
    U32 x_checker = Square::SQUARE_NONE;

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
        if constexpr (GenQuiet)
        {
            gen_castle<US, CastleSide::KING_SIDE>(ml);
            gen_castle<US, CastleSide::QUEEN_SIDE>(ml);
        }

        // pawn (pinned)
        pieceBB = typePiecesBB[PieceType::PAWN] & pinnedBB;

        while (pieceBB) {
            from = BB::pop_lsb(pieceBB);
            d = dir[from];
            
            if constexpr (GenNoisy)
            {
                if (d == abs_pawn_left && (SQ::square_BB(to = from + pawn_left) & Attacks::pawn_attacks<US>(from) & enemyBB ))
                {
                    if (SQ::is_on_seventh_rank<US>(from))
                        push_capture_promotion(ml, from, to, US);
                    else
                        add_capture_move(ml, from, to, Move::make_piece(US, PieceType::PAWN), piece_square[to], Move::FLAG_NONE);
                }
                else if (d == abs_pawn_right && (SQ::square_BB(to = from + pawn_right) & Attacks::pawn_attacks<US>(from) & enemyBB))
                {
                    if (SQ::is_on_seventh_rank<US>(from))
                        push_capture_promotion(ml, from, to, US);
                    else
                        add_capture_move(ml, from, to, Move::make_piece(US, PieceType::PAWN), piece_square[to], Move::FLAG_NONE);
                }
            }
            
            if constexpr (GenQuiet)
            {
                if (d == abs_pawn_push && (SQ::square_BB(to = from + pawn_push) & emptyBB))
                {
                    add_quiet_move(ml, from, to, Move::make_piece(US, PieceType::PAWN), Move::FLAG_NONE);
                    if (SQ::is_on_second_rank<US>(from) && (SQ::square_BB(to += pawn_push) & emptyBB))
                        add_quiet_move(ml, from, to, Move::make_piece(US, PieceType::PAWN), Move::FLAG_DOUBLE_MASK);
                }
            }
        }

        // bishop or queen (pinned)
        pieceBB = bq & pinnedBB;

        while (pieceBB)
        {
            from = BB::pop_lsb(pieceBB);
            d = dir[from];
            attackBB = Attacks::bishop_moves(from, occupiedBB);
            Bitboard diagBB  = attackBB & DiagonalMask64[from];
            Bitboard adiagBB = attackBB & AntiDiagonalMask64[from];

            if (d == 9)
            {
                if constexpr (GenNoisy) push_capture_moves(ml, diagBB & enemyBB, from);
                if constexpr (GenQuiet) push_quiet_moves(ml,   diagBB & emptyBB, from);
            }
            else if (d == 7)
            {
                if constexpr (GenNoisy) push_capture_moves(ml, adiagBB & enemyBB, from);
                if constexpr (GenQuiet) push_quiet_moves(ml,   adiagBB & emptyBB, from);
            }
        }

        // rook or queen (pinned)
        pieceBB = rq & pinnedBB;

        while (pieceBB)
        {
            from = BB::pop_lsb(pieceBB);
            d = dir[from];
            attackBB = Attacks::rook_moves(from, occupiedBB);
            Bitboard diagBB  = attackBB & RankMask64[from];
            Bitboard adiagBB = attackBB & FileMask64[from];

            if (d == 1)
            {
                if constexpr (GenNoisy) push_capture_moves(ml, diagBB & enemyBB, from);
                if constexpr (GenQuiet) push_quiet_moves(ml,   diagBB & emptyBB, from);
            }
            else if (d == 8)
            {
                if constexpr (GenNoisy) push_capture_moves(ml, adiagBB & enemyBB, from);
                if constexpr (GenQuiet) push_quiet_moves(ml,   adiagBB & emptyBB, from);
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
    if constexpr (GenNoisy)
    {
        if (   get_status().ep_square != Square::SQUARE_NONE
            && (!checkersBB || x_checker == get_status().ep_square - pawn_push))
        {
            // file : a...h

            to = get_status().ep_square;             // e3 = 20
            ep = to - pawn_push;        // 20 - (-8)  noirs = 28 = e4
            from = ep - 1;              // 28 - 1 = 27 = d4

            Bitboard our_pawns = occupancy_cp<US, PieceType::PAWN>();
            if (SQ::file(to) > 0 && our_pawns & SQ::square_BB(from) )
            {
                pieceBB = occupiedBB ^ SQ::square_BB(from) ^ SQ::square_BB(ep) ^ SQ::square_BB(to);

                if (!(Attacks::bishop_moves(K, pieceBB) & bq & colorPiecesBB[THEM]) &&
                    !(Attacks::rook_moves(K, pieceBB)   & rq & colorPiecesBB[THEM]) )
                {
                    add_capture_move(ml, from, to,
                                     Move::make_piece(US, PieceType::PAWN),
                                     Move::make_piece(THEM, PieceType::PAWN), Move::FLAG_ENPASSANT_MASK);
                }
            }

            from = ep + 1;          // 28 + 1 = 29 = f4
            if (SQ::file(to) < 7 && our_pawns & SQ::square_BB(from) )
            {
                pieceBB = occupiedBB ^ SQ::square_BB(from) ^ SQ::square_BB(ep) ^ SQ::square_BB(to);

                if (!(Attacks::bishop_moves(K, pieceBB) & bq & colorPiecesBB[THEM]) &&
                    !(Attacks::rook_moves(K, pieceBB)   & rq & colorPiecesBB[THEM]) )
                {
                    add_capture_move(ml, from, to, Move::make_piece(US, PieceType::PAWN), Move::make_piece(THEM, PieceType::PAWN), Move::FLAG_ENPASSANT_MASK);
                }
            }
        }
    }

    // pawn
    pieceBB = typePiecesBB[PieceType::PAWN] & unpinnedBB;

    if constexpr (GenNoisy)
    {
        if constexpr (US==Color::WHITE)
            attackBB = ((pieceBB & ~FILE_A_BB) << 7) & enemyBB;
        else
            attackBB = ((pieceBB & ~FILE_A_BB) >> 9 ) & enemyBB;

        push_capture_promotions(ml, attackBB & PromotionRank[US], pawn_left, US);
        push_pawn_capture_moves(ml, attackBB & ~PromotionRank[US], pawn_left, US);

        if constexpr (US==Color::WHITE)
            attackBB = ((pieceBB & ~FILE_H_BB) << 9) & enemyBB;
        else
            attackBB = ((pieceBB & ~FILE_H_BB) >> 7) & enemyBB;

        push_capture_promotions(ml, attackBB & PromotionRank[US], pawn_right, US);
        push_pawn_capture_moves(ml, attackBB & ~PromotionRank[US], pawn_right, US);
    }

    if constexpr (US==Color::WHITE)
        attackBB = (pieceBB << 8) & emptyBB;
    else
        attackBB = (pieceBB >> 8) & emptyBB;

    if constexpr (GenNoisy)
    {
        push_quiet_promotions(ml, attackBB & PromotionRank[US], pawn_push, US);
    }

    if constexpr (GenQuiet)
    {
        push_pawn_quiet_moves(ml, attackBB & ~PromotionRank[US], pawn_push, US, Move::FLAG_NONE);

        if constexpr (US==Color::WHITE)
            attackBB = ((((pieceBB & RANK_2_BB) << 8) & ~occupiedBB) << 8) & emptyBB;
        else
            attackBB = ((((pieceBB & RANK_7_BB) >> 8) & ~occupiedBB) >> 8) & emptyBB;

        push_pawn_quiet_moves(ml, attackBB, 2 * pawn_push, US, Move::FLAG_DOUBLE_MASK);
    }

    // knight

    pieceBB = typePiecesBB[PieceType::KNIGHT] & unpinnedBB;
    while (pieceBB)
    {
        from = BB::pop_lsb(pieceBB);
        attackBB = Attacks::knight_moves(from);
        if constexpr (GenNoisy) push_piece_capture_moves(ml, attackBB & enemyBB, from, US, PieceType::KNIGHT);
        if constexpr (GenQuiet) push_piece_quiet_moves(ml,   attackBB & emptyBB, from, US, PieceType::KNIGHT);
    }

    // bishop or queen
    pieceBB = bq & unpinnedBB;
    while (pieceBB)
    {
        from = BB::pop_lsb(pieceBB);
        attackBB = Attacks::bishop_moves(from, occupiedBB);
        if constexpr (GenNoisy) push_capture_moves(ml, attackBB & enemyBB, from);
        if constexpr (GenQuiet) push_quiet_moves(ml,   attackBB & emptyBB, from);
    }

    // rook or queen
    pieceBB = rq & unpinnedBB;
    while (pieceBB)
    {
        from = BB::pop_lsb(pieceBB);
        attackBB = Attacks::rook_moves(from, occupiedBB);
        if constexpr (GenNoisy) push_capture_moves(ml, attackBB & enemyBB, from);
        if constexpr (GenQuiet) push_quiet_moves(ml,   attackBB & emptyBB, from);
    }

    // king
    /* on enlève le roi de l'échiquier :
     * Dans cette position
     *   ..R...t.
     *   12345678
     *
     *   Dans cette position, si le roi va à gauche, il est toujours attaqué par la tour ennemie.
     *     car la case (2) sera attaquée par la tour noire.
     *   Or, si on calcule les attaques de NOIR avec le roi BLANC,
     *     la case (2) ne sera pas attaquée par la tour NOIRE.
     *   Il faut donc calculer les attaques de NOIR sans le roi bLANC.
     */
    colorPiecesBB[US] ^= SQ::square_BB(K);  // suppression du roi ami
    attackBB = squares_attacked<THEM>();    // attaques ennemies SANS le roi ami

    if constexpr (GenNoisy)
    {
        auto maskn = Attacks::king_moves(K) & colorPiecesBB[THEM];
        while (maskn)
        {
            to = BB::pop_lsb(maskn);

            if (BB::empty(attackBB & SQ::square_BB(to)))
                add_capture_move(ml, K, to, Move::make_piece(US, PieceType::KING), piece_square[to], Move::FLAG_NONE);
        }
    }

    if constexpr (GenQuiet)
    {
        auto maskq = Attacks::king_moves(K) & ~occupiedBB;
        while (maskq)
        {
            to = BB::pop_lsb(maskq);

            if (BB::empty(attackBB & SQ::square_BB(to)))
                add_quiet_move(ml, K, to, Move::make_piece(US, PieceType::KING), Move::FLAG_NONE);
        }
    }

    // remet le roi dans l'échiquier
    colorPiecesBB[US] ^= SQ::square_BB(K);
}

// Explicit instantiations.
template void Board::legal_moves<WHITE, MoveGenType::NOISY>(MoveList& ml)  noexcept;
template void Board::legal_moves<BLACK, MoveGenType::NOISY>(MoveList& ml)  noexcept;

template void Board::legal_moves<WHITE, MoveGenType::QUIET>(MoveList& ml)  noexcept;
template void Board::legal_moves<BLACK, MoveGenType::QUIET>(MoveList& ml)  noexcept;

template void Board::legal_moves<WHITE, MoveGenType::ALL>(MoveList& ml)  noexcept;
template void Board::legal_moves<BLACK, MoveGenType::ALL>(MoveList& ml)  noexcept;
