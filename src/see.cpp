#include "Board.h"
#include "Move.h"
#include "Attacks.h"


constexpr int SEE_VALUE[N_PIECE_TYPE] = {0, 100, 300, 300, 500, 900, 9999};

//==========================================================================
//! \brief  Détermine si le coup est avantageux :
//! Teste si la valeur SEE du coup est supérieure ou égale au threshold.
//! Basé sur l'algorithme "Swap"
//!
//! \param[in]  move       coup à évaluer (SEE)
//! \param[in]  threshold  seuil à atteindre ou dépasser
//!
//! \return true si la valeur SEE du coup est >= threshold
//---------------------------------------------------------------------------
bool Board::fast_see(const MOVE move, const int threshold) const
{
    // Squelette repris de Berserk

    // Un roque ne prend rien et ne peut rien perdre : sa valeur SEE est nulle.
    if (Move::is_castling(move))
        return threshold <= 0;

    const SQUARE from = Move::from(move);
    const SQUARE dest = Move::dest(move);

    // Gain du coup, ramené au threshold
    int v = SEE_VALUE[Move::captured_type(move)] - threshold;

    // Meilleur cas : on prend et on ne perd rien. Insuffisant => échec.
    if (v < 0)
        return false;

    // Le pire cas est celui où on perd la pièce prenante.
    v -= SEE_VALUE[Move::piece_type(move)];

    // Si la valeur reste positive même après cette perte, l'échange est garanti
    // de battre le threshold.
    if (v >= 0)
        return true;

    // Bitboard de toutes les cases occupées, en enlevant la pièce de départ
    // et en ajoutant la case d'arrivée
    Bitboard occupiedBB  = (occupancy_all() ^ SQ::square_BB(from)) | SQ::square_BB(dest);

    // Bitboard de toutes les attaques (Blanches et Noires) de la case d'arrivée
    Bitboard all_attackersBB = all_attackers(dest, occupiedBB);

    // Bitboards des glisseurs
    const Bitboard bqBB = typePiecesBB[PieceType::BISHOP] | typePiecesBB[PieceType::QUEEN];
    const Bitboard rqBB = typePiecesBB[PieceType::ROOK]   | typePiecesBB[PieceType::QUEEN];

    // C'est au tour de l'adversaire de jouer
    Color color = ~turn();

    while (true)
    {
        // On ne garde que les attaquants encore présents sur l'échiquier
        all_attackersBB &= occupiedBB;

        // Bitboard de mes attaquants
        Bitboard my_attackers = all_attackersBB & colorPiecesBB[color];

        // Si on n'a plus d'attaquants, on s'arrête : "color" perd
        if (!my_attackers)
            break;

        // Recherche de la pièce de moindre valeur qui attaque
        PieceType piece = PieceType::PAWN;
        for (PieceType pt : all_PIECE_TYPE) {
            if (my_attackers & typePiecesBB[pt])
            {
                piece = pt;
                break;
            }
        }

        // Change de camp
        color = ~color;

        // Seul le roi peut reprendre : si l'adversaire a encore un attaquant,
        // la reprise est illégale et le camp du roi perd l'échange.
        if (piece == PieceType::KING)
        {
            if (all_attackersBB & colorPiecesBB[color])
                color = ~color;
            break;
        }

        // Negamax du solde avec alpha = balance, beta = balance+1 :
        //
        //      (balance, balance+1) -> (-balance-1, -balance)
        //
        // et on retranche la pièce qui vient d'être posée sur la case d'arrivée,
        // c'est la prochaine victime.
        v = -v - 1 - SEE_VALUE[piece];

        // Si le solde reste positif pour le camp qui vient de reprendre, même
        // en perdant la pièce qu'il vient de poser, il gagne l'échange
        if (v >= 0)
            break;

        // Supprime l'attaquant "piece" des occupants
        occupiedBB ^= SQ::square_BB(BB::get_lsb(my_attackers & typePiecesBB[piece]));

        // Si l'attaque était diagonale, il peut y avoir
        // des attaquants fou ou dame cachés derrière
        if (piece == PieceType::PAWN || piece == PieceType::BISHOP || piece == PieceType::QUEEN)
            all_attackersBB |= Attacks::bishop_moves(dest, occupiedBB) & bqBB;

        // Si l'attaque était orthogonale, il peut y avoir
        // des attaquants tour ou dame cachés derrière
        if (piece == PieceType::ROOK || piece == PieceType::QUEEN)
            all_attackersBB |= Attacks::rook_moves(dest, occupiedBB) & rqBB;
    }

    // Le camp au trait après la boucle perd
    return (color != turn());
}
