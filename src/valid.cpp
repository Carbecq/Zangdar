#include "Bitboard.h"
#include "Board.h"
#include <iostream>

template<bool Update_NNUE>
bool Board::valid(const std::string &message) noexcept
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

    if (Update_NNUE == true) {
         int v1 = nnue.evaluate<WHITE>();
         int v2{};
         calculate_nnue(v2);
         if (v1 != v2) {
             std::cout << message << " : erreur nnue " << v1 << "  " << v2 << std::endl;
             std::cout << display() << std::endl;
             return false;
         } else {
             // std::cout << message << " : OK " << v1 << "  " << v2 << std::endl;
         }
     }

    const int epsq = get_ep_square();
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
    
    if (BB::count_bit(occupancy_cp<Color::WHITE, PieceType::KING>()) != 1) {
        std::cout << "erreur 3" << std::endl;
        return false;
    }
    
    if (BB::count_bit(occupancy_cp<Color::BLACK, PieceType::KING>()) != 1) {
        std::cout << "erreur 4" << std::endl;
        return false;
    }

    if (colorPiecesBB[0] & colorPiecesBB[1]) {
        BB::PrintBB(colorPiecesBB[0], "erreur 5");
        BB::PrintBB(colorPiecesBB[1], "erreur 5");
        std::cout << "erreur 5" << std::endl;
        return false;
    }

    if (occupancy_p<PieceType::PAWN>() & (RANK_1_BB | RANK_8_BB)) {
        printf("%s \n", display().c_str());
        BB::PrintBB(occupancy_p<PieceType::PAWN>(), "erreur 6");
        BB::PrintBB(RANK_1_BB, "erreur 6");
        BB::PrintBB(RANK_8_BB, "erreur 6");

        std::cout << "erreur 6" << std::endl;
        return false;
    }

    // Attention : ça marche car l'enum PieceType est consécutif
    for (auto i = static_cast<U32>(PieceType::PAWN); i <= static_cast<U32>(PieceType::KING); ++i) {
        for (auto j = i + 1; j <= static_cast<U32>(PieceType::KING); ++j) {
            if (typePiecesBB[i] & typePiecesBB[j]) {
                std::cout << "erreur 7 " << std::endl;
                return false;
            }
        }
    }

    //    if ((colorPiecesBB[1] | colorPiecesBB[1]) != (typePiecesBB[0] | typePiecesBB[1] | typePiecesBB[2] | typePiecesBB[3] | typePiecesBB[4] | typePiecesBB[5])) {
    //        printf("%s \n\n", display().c_str());
    // BB::PrintBB(colorPiecesBB[0]);
    // BB::PrintBB(colorPiecesBB[1]);
    // BB::PrintBB(typePiecesBB[0]);
    // BB::PrintBB(typePiecesBB[1]);
    // BB::PrintBB(typePiecesBB[2]);
    // BB::PrintBB(typePiecesBB[3]);
    // BB::PrintBB(typePiecesBB[4]);
    // BB::PrintBB(typePiecesBB[5]);

    //        std::cout << "erreur 8" << std::endl;
    //        return false;
    //    }
    
    if (x_king[WHITE] != king_square<WHITE>())
    {
        std::cout << "erreur roi blanc" << std::endl;
        return(false);
    }
    
    if (x_king[BLACK] != king_square<BLACK>())
    {
        std::cout << "erreur roi noir" << std::endl;
        return(false);
    }

    for (int i=0; i<64; i++)
    {
        Piece p1 = pieceBoard[i] ;
        auto n1  = pieceToChar(p1);
        Piece p2 =  piece_on(i) ;
        auto n2  = pieceToChar(p2);

        if (pieceBoard[i] != piece_on(i))
        {
            std::cout << "erreur piece case " << i << " : " << square_name[i]
                         << " board : " << n1 << " pieceon : "<< n2 <<  std::endl;
            return(false);
        }
    }

    //    std::cout << "11 " << std::endl;

    //    if (white_can_castle_k()) {
    //        if (!(Bitboard(king_position(Color::WHITE)) & bitboards::Rank1)) {
    //            return false;
    //        }
    //        if (piece_on(castle_rooks_from_[0]) != ROOK) {
    //            return false;
    //        }
    //    }
    //  std::cout << "12 " << std::endl;

    //    if (white_can_castle_q()) {
    //        if (!(Bitboard(king_position(Color::WHITE)) & bitboards::Rank1)) {
    //            return false;
    //        }
    //        if (piece_on(castle_rooks_from_[1]) != ROOK) {
    //            return false;
    //        }
    //    }
    //   std::cout << "13 " << std::endl;

    //    if (black_can_castle_k()) {
    //        if (!(Bitboard(king_position(Color::BLACK)) & bitboards::Rank8)) {
    //            return false;
    //        }
    //        if (piece_on(castle_rooks_from_[2]) != ROOK) {
    //            return false;
    //        }
    //    }
    //   std::cout << "14 " << std::endl;

    //    if (black_can_castle_q()) {
    //        if (!(Bitboard(king_position(Color::BLACK)) & bitboards::Rank8)) {
    //            return false;
    //        }
    //        if (piece_on(castle_rooks_from_[3]) != ROOK) {
    //            return false;
    //        }
    //    }

    //   std::cout << "valid fin " << std::endl;

    return true;
}

template bool Board::valid<true>(const std::string &message) noexcept;
template bool Board::valid<false>(const std::string &message) noexcept;

