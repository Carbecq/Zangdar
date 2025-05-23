#include "Board.h"
#include "Move.h"
#include "Attacks.h"


constexpr int SEE_VALUE[N_PIECE_TYPE] = {0, 100, 300, 300, 500, 900, 9999};

//==========================================================================
//! \brief  Détermine si le coup est avantageux :
//! Teste si la valeur SEE du coup est supérieure ou égale au threshold.
//! Basé sur l'algorithme "Swap"
//---------------------------------------------------------------------------
bool Board::fast_see(const MOVE move, const int threshold) const
{
    // Code provenant de Berserk

    // Cette routine n'est appelée qu'en cas de capture

    const int from = Move::from(move);
    const int dest = Move::dest(move);

    // si la valeur de la pièce prise est inférieure au threshold,
    // ce n'est pas la peine de continuer
    int v = SEE_VALUE[static_cast<U32>(Move::captured_type(move))] - threshold;
    if (v < 0)
        return false;

    // Le pire cas est celui où on perd la pièce prenante.
    // SAUF si la pièce qui va reprendre est un pion qui va être promu !!
    v -= SEE_VALUE[static_cast<U32>(Move::piece_type(move))];

    // Si la valeur est positive même après avoir perdu la pièce se déplaçant,
    // alors l'échange est garanti de battre le threshold.
    if (v >= 0)
        return true;

    /* X Y  X&Y  X|Y  X^Y
     * 0 0  0    0    0
     * 0 1  0    1    1
     * 1 0  0    1    1
     * 1 1  1    1    0
     */

    // Bitboard de toutes les cases occupées, en enlevant la pièce de départ
    // et en ajoutant la case d'arrivée
    Bitboard occupiedBB  = (occupancy_all() ^ SQ::square_BB(from)) | SQ::square_BB(dest);

    // Bitboard de toutes les attaques (Blanches et Noires) de la case d'arrivée
    Bitboard all_attackersBB = all_attackers(dest, occupiedBB);

    // Bitboards des sliders
    const Bitboard bqBB = typePiecesBB[static_cast<U32>(PieceType::BISHOP)] | typePiecesBB[static_cast<U32>(PieceType::QUEEN)];
    const Bitboard rqBB = typePiecesBB[static_cast<U32>(PieceType::ROOK)]   | typePiecesBB[static_cast<U32>(PieceType::QUEEN)];

    // C'est au tour de l'adversaire de jouer
    Color color = ~turn();

    while (true)
    {
        // On enlève des attaquants les occupants
        // Make sure we did not add any already used attacks
        all_attackersBB &= occupiedBB;

        // Bitboard de mes attaquants
        Bitboard my_attackers = all_attackersBB & colorPiecesBB[color];

        // Si on n'a plus d'attaquants, on s'arrête : "color" perd
        if (!my_attackers)
            break;

        // Recherche de la pièce de moindre valeur qui attaque
        PieceType piece = PieceType::PAWN;
        for (PieceType pt : all_PIECE_TYPE) {
            if (my_attackers & typePiecesBB[static_cast<U32>(pt)])
            {
                piece = pt;
                break;
            }
        }

        // int piece = PieceType::PAWN;
        // for (piece = PieceType::PAWN; piece < PieceType::KING; piece++)
            // if (my_attackers & typePiecesBB[piece])
            //     break;

        // Change de camp
        color = ~color;

        // Negamax the balance with alpha = balance, beta = balance+1 and
        // add nextVictim's value.
        //
        //      (balance, balance+1) -> (-balance-1, -balance)
        //
        v = -v - 1 - SEE_VALUE[static_cast<U32>(piece)];

        // Si la valeur est positive après avoir donné notre pièce
        // alors on a gagné
        if (v >= 0)
        {
            // As a slide speed up for move legality checking, if our last attacking
            // piece is a king, and our opponent still has attackers, then we've
            // lost as the move we followed would be illegal
            if (piece == PieceType::KING && (all_attackersBB & colorPiecesBB[color]))
                color = ~color;

            break;
        }

        // Supprime l'attaquant "piece" des occupants
        occupiedBB ^= SQ::square_BB(BB::get_lsb(my_attackers & typePiecesBB[static_cast<U32>(piece)]));

        // Si l'attaque était diagonale, il peut y avoir
        // des attaquants fou ou dame cachés derrière
        if (piece == PieceType::PAWN || piece == PieceType::BISHOP || piece == PieceType::QUEEN)
            all_attackersBB |= Attacks::bishop_moves(dest, occupiedBB) & bqBB;

        // Si l'attaque était orthogonale, il peut y avoir
        // des attaquants tour ou dame cachés derrière
        if (piece == PieceType::ROOK || piece == PieceType::QUEEN)
            all_attackersBB |= Attacks::rook_moves(dest, occupiedBB) & rqBB;
    }

    // Side to move after the loop loses
    return (color != turn());
}

