#include "Bitboard.h"
#include "Board.h"
#include "NNUE.h"
#include <iostream>

//==================================================================
//! \brief  Vérifie la cohérence interne de la position
//! Utilisé en mode debug (assertions)
//------------------------------------------------------------------
bool Board::valid() const noexcept
{
//    std::cout << "valid debut" << std::endl;

    U64 hash_1, hash_2, hash_3[2];
    calculate_hash(hash_1, hash_2, hash_3);
    if (get_status().key != hash_1)
    {
        std::cout << "erreur key " << std::endl;
        return false;
    }
    if (get_status().pawn_key != hash_2)
    {
        std::cout << "erreur pawn key" << std::endl;
        return false;
    }
    if (get_status().mat_key[WHITE] != hash_3[WHITE])
    {
        std::cout << "erreur mat key white" << std::endl;
        return false;
    }
    if (get_status().mat_key[BLACK] != hash_3[BLACK])
    {
        std::cout << "erreur mat key black" << std::endl;
        return false;
    }

    const SQUARE epsq = get_ep_square();
    Color color = turn();
    if (epsq != SQUARE_NONE) {
        if (color == Color::WHITE && SQ::rank(get_ep_square()) != 5) {
            std::cout << "erreur 1" << std::endl;
            return false;
        }
        if (color == Color::BLACK && SQ::rank(get_ep_square()) != 2) {
            std::cout << "erreur 2" << std::endl;
            return false;
        }
        if (color == Color::WHITE && BB::empty(pawn_attackers<WHITE>(epsq)))
        {
            std::cout << "case de pep impossible Blanc" << std::endl;
            return false;
        }
        if (color == Color::BLACK && BB::empty(pawn_attackers<BLACK>(epsq)))
        {
            std::cout << "case de pep impossible Noir" << std::endl;
            return false;
        }
    }
    
     if (colorPiecesBB[0] & colorPiecesBB[1]) {
        BB::PrintBB(colorPiecesBB[0], "erreur 5");
        BB::PrintBB(colorPiecesBB[1], "erreur 5");
        std::cout << "erreur 5" << std::endl;
        return false;
    }

    // pas de pion sur les rangées 1 et 8
    if (occupancy_p<PieceType::PAWN>() & (RANK_1_BB | RANK_8_BB)) {
        printf("%s \n", display().c_str());
        BB::PrintBB(occupancy_p<PieceType::PAWN>(), "erreur 6");
        BB::PrintBB(RANK_1_BB, "erreur 6");
        BB::PrintBB(RANK_8_BB, "erreur 6");

        std::cout << "erreur 6" << std::endl;
        return false;
    }

    // Attention : ça marche car l'enum PieceType est consécutif
    for (U32 i = PieceType::PAWN; i <= PieceType::KING; ++i) {
        for (U32 j = i + 1; j <= PieceType::KING; ++j) {
            if (typePiecesBB[i] & typePiecesBB[j]) {
                std::cout << "erreur 7 " << std::endl;
                return false;
            }
        }
    }

    if (king_square[WHITE] != get_king_square<WHITE>())
    {
        std::cout << "erreur roi blanc" << std::endl;
        return(false);
    }
    
    if (king_square[BLACK] != get_king_square<BLACK>())
    {
        std::cout << "erreur roi noir" << std::endl;
        return(false);
    }

    for (SQUARE i=A1; i<N_SQUARES; ++i)
    {
        if (piece_square[i] != piece_on(i))
        {
            Piece p1 = piece_square[i] ;
            auto n1  = pieceToChar(p1);
            Piece p2 =  piece_on(i) ;
            auto n2  = pieceToChar(p2);

            std::cout << "erreur piece case " << i << " : " << square_name[i]
                         << " board : " << n1 << " pieceon : "<< n2 <<  std::endl;
            return(false);
        }
    }

    // L'union des bitboards par type doit correspondre à l'union des bitboards par couleur
    {
        Bitboard all_types = 0;
        for (U32 i = PieceType::PAWN; i <= PieceType::KING; ++i)
            all_types |= typePiecesBB[i];

        if (all_types != occupancy_all())
        {
            std::cout << "erreur union typePiecesBB != colorPiecesBB" << std::endl;
            return false;
        }
    }

    // Nombre de pions : max 8 par couleur
    if (BB::count_bit(occupancy_cp<WHITE, PieceType::PAWN>()) > 8)
    {
        std::cout << "erreur trop de pions blancs" << std::endl;
        return false;
    }
    if (BB::count_bit(occupancy_cp<BLACK, PieceType::PAWN>()) > 8)
    {
        std::cout << "erreur trop de pions noirs" << std::endl;
        return false;
    }

    // Nombre total de pièces : max 16 par couleur
    if (BB::count_bit(colorPiecesBB[WHITE]) > 16)
    {
        std::cout << "erreur trop de pieces blanches" << std::endl;
        return false;
    }
    if (BB::count_bit(colorPiecesBB[BLACK]) > 16)
    {
        std::cout << "erreur trop de pieces noires" << std::endl;
        return false;
    }

    // Un seul roi
    if (BB::count_bit(occupancy_cp<WHITE, PieceType::KING>()) != 1)
    {
        std::cout << "le nombre de roi blanc est incorrect" << std::endl;
        return false;
    }
    if (BB::count_bit(occupancy_cp<BLACK, PieceType::KING>()) != 1)
    {
        std::cout << "le nombre de roi noir est incorrect" << std::endl;
        return false;
    }

    // pions + cavaliers
    if (BB::count_bit(occupancy_cp<WHITE, PieceType::PAWN>()) + BB::count_bit(occupancy_cp<WHITE, PieceType::KNIGHT>()) > 10)
    {
        std::cout << "il y a trop de cavaliers blancs" << std::endl;
        return false;
    }
    if (BB::count_bit(occupancy_cp<BLACK, PieceType::PAWN>()) + BB::count_bit(occupancy_cp<BLACK, PieceType::KNIGHT>()) > 10)
    {
        std::cout << "il y a trop de cavaliers noirs" << std::endl;
        return false;
    }

    // pions + fous
    if (BB::count_bit(occupancy_cp<WHITE, PieceType::PAWN>()) + BB::count_bit(occupancy_cp<WHITE, PieceType::BISHOP>()) > 10)
    {
        std::cout << "il y a trop de fous blancs" << std::endl;
        return false;
    }
    if (BB::count_bit(occupancy_cp<BLACK, PieceType::PAWN>()) + BB::count_bit(occupancy_cp<BLACK, PieceType::BISHOP>()) > 10)
    {
        std::cout << "il y a trop de fous noirs" << std::endl;
        return false;
    }

    // pions + tours
    if (BB::count_bit(occupancy_cp<WHITE, PieceType::PAWN>()) + BB::count_bit(occupancy_cp<WHITE, PieceType::ROOK>()) > 10)
    {
        std::cout << "il y a trop de tours blanches" << std::endl;
        return false;
    }
    if (BB::count_bit(occupancy_cp<BLACK, PieceType::PAWN>()) + BB::count_bit(occupancy_cp<BLACK, PieceType::ROOK>()) > 10)
    {
        std::cout << "il y a trop de tours noires" << std::endl;
        return false;
    }

    // pions + dames
    if (BB::count_bit(occupancy_cp<WHITE, PieceType::PAWN>()) + BB::count_bit(occupancy_cp<WHITE, PieceType::QUEEN>()) > 9)
    {
        std::cout << "il y a trop de dames blanches" << std::endl;
        return false;
    }
    if (BB::count_bit(occupancy_cp<BLACK, PieceType::PAWN>()) + BB::count_bit(occupancy_cp<BLACK, PieceType::QUEEN>()) > 9)
    {
        std::cout << "il y a trop de dames noires" << std::endl;
        return false;
    }

    // Le camp qui n'est PAS au trait ne doit pas être en échec
    // (l'adversaire vient de jouer, il n'a pas pu laisser son roi en échec)
    if (color == WHITE)
    {
        if (!BB::empty(attackers<WHITE>(king_square[BLACK])))
        {
            std::cout << "erreur roi noir en echec alors que c'est aux blancs de jouer" << std::endl;
            return false;
        }
    }
    else
    {
        if (!BB::empty(attackers<BLACK>(king_square[WHITE])))
        {
            std::cout << "erreur roi blanc en echec alors que c'est aux noirs de jouer" << std::endl;
            return false;
        }
    }

    // Droits de roque : cohérence avec les pièces sur l'échiquier
    U32 castling = get_status().castling;
    if ((castling & CASTLE_WK) && (piece_square[E1] != Piece::WHITE_KING || piece_square[H1] != Piece::WHITE_ROOK))
    {
        std::cout << "erreur droit roque WK sans roi e1 ou tour h1" << std::endl;
        return false;
    }
    if ((castling & CASTLE_WQ) && (piece_square[E1] != Piece::WHITE_KING || piece_square[A1] != Piece::WHITE_ROOK))
    {
        std::cout << "erreur droit roque WQ sans roi e1 ou tour a1" << std::endl;
        return false;
    }
    if ((castling & CASTLE_BK) && (piece_square[E8] != Piece::BLACK_KING || piece_square[H8] != Piece::BLACK_ROOK))
    {
        std::cout << "erreur droit roque BK sans roi e8 ou tour h8" << std::endl;
        return false;
    }
    if ((castling & CASTLE_BQ) && (piece_square[E8] != Piece::BLACK_KING || piece_square[A8] != Piece::BLACK_ROOK))
    {
        std::cout << "erreur droit roque BQ sans roi e8 ou tour a8" << std::endl;
        return false;
    }

    // En-passant : un pion adverse doit être devant la case ep
    if (epsq != SQUARE_NONE)
    {
        if (color == WHITE)
        {
            // Les blancs jouent, ep en rangée 6, le pion noir est en rangée 5 (epsq - 8)
            if (piece_square[epsq - 8] != Piece::BLACK_PAWN)
            {
                std::cout << "erreur pep : pas de pion noir devant la case ep" << std::endl;
                return false;
            }
        }
        else
        {
            // Les noirs jouent, ep en rangée 3, le pion blanc est en rangée 4 (epsq + 8)
            if (piece_square[epsq + 8] != Piece::WHITE_PAWN)
            {
                std::cout << "erreur pep : pas de pion blanc devant la case ep" << std::endl;
                return false;
            }
        }
    }

    // Compteur de 50 coups : doit être dans [0, 100]
    if (get_fiftymove_counter() < 0 || get_fiftymove_counter() > 100)
    {
        std::cout << "erreur fiftymove counter hors limites : " << get_fiftymove_counter() << std::endl;
        return false;
    }

    return true;
}


