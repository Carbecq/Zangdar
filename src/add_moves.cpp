#include "Board.h"
#include "Move.h"

//=================================================================
//! \brief  Ajoute un coup tranquille à la liste des coups
//!
//! \param[in]  ml      MoveList de stockage des coups
//! \param[in]  from    position de départ de la pièce
//! \param[in]  dest    position d'arrivée de la pièce
//! \param[in]  piece   type de la pièce jouant
//! \param[in]  flags   flags du coup (avance double, prise en-passant, roque)
//-----------------------------------------------------------------
void Board::add_quiet_move(MoveList& ml, int from, int dest, Piece piece, U32 flags)  const noexcept
{
    ml.mlmoves[ml.count++].move = Move::CODE(from, dest, piece, Piece::NONE, Piece::NONE, flags);
}

//=================================================================
//! \brief  Ajoute un coup de capture à la liste des coups
//!
//! \param[in]  ml      MoveList de stockage des coups
//! \param[in]  from    position de départ de la pièce
//! \param[in]  dest    position d'arrivée de la pièce
//! \param[in]  piece   type de la pièce jouant
//! \param[in]  captured   type de la pièce capturée
//! \param[in]  flags   flags du coup (avance double, prise en-passant, roque)
//-----------------------------------------------------------------
void Board::add_capture_move(MoveList& ml, int from, int dest, Piece piece, Piece captured, U32 flags) const noexcept
{
    ml.mlmoves[ml.count++].move  = Move::CODE(from, dest, piece, captured, Piece::NONE, flags);
}

//=================================================================
//! \brief  Ajoute un coup tranquille de promotion à la liste des coups
//!
//! \param[in]  ml      MoveList de stockage des coups
//! \param[in]  from    position de départ de la pièce
//! \param[in]  dest    position d'arrivée de la pièce
//! \param[in]  promo   type de la pièce promue
//! \param[in]  flags   flags du coup (avance double, prise en-passant, roque)
//-----------------------------------------------------------------
void Board::add_quiet_promotion(MoveList& ml, int from, int dest, Color color, Piece promoted) const noexcept
{
    ml.mlmoves[ml.count++].move = Move::CODE(from, dest,
                                             Move::make_piece(color, PieceType::PAWN),
                                             Piece::NONE,
                                             promoted,
                                             Move::FLAG_NONE);
}

//=================================================================
//! \brief  Ajoute un coup de capture et de promotion à la liste des coups
//!
//! \param[in]  ml      MoveList de stockage des coups
//! \param[in]  from    position de départ de la pièce
//! \param[in]  dest    position d'arrivée de la pièce
//! \param[in]  piece   type de la pièce jouant
//! \param[in]  captured   type de la pièce capturée
//! \param[in]  promo   type de la pièce promue
//! \param[in]  flags   flags du coup (avance double, prise en-passant, roque)
//-----------------------------------------------------------------
void Board::add_capture_promotion(MoveList& ml, int from, int dest, Color color, Piece captured, Piece promoted) const noexcept
{
    ml.mlmoves[ml.count++].move  = Move::CODE(from, dest,
                                              Move::make_piece(color, PieceType::PAWN),
                                              captured,
                                              promoted,
                                              Move::FLAG_NONE);
}






//===================================================================
//! \brief  Ajoute une série de coups
//-------------------------------------------------------------------
void Board::push_quiet_moves(MoveList& ml, Bitboard attack, const int from)
{
    int to;

    while (attack) {
        to = BB::pop_lsb(attack);
        add_quiet_move(ml, from, to, piece_square[from], Move::FLAG_NONE);
    }
}
void Board::push_piece_quiet_moves(MoveList& ml, Bitboard attack, const int from, Color color, PieceType piece)
{
    int to;

    while (attack) {
        to = BB::pop_lsb(attack);
        add_quiet_move(ml, from, to, Move::make_piece(color, piece), Move::FLAG_NONE);
    }
}

void Board::push_capture_moves(MoveList& ml, Bitboard attack, const int from)
{
    int to;

    while (attack) {
        to = BB::pop_lsb(attack);
        add_capture_move(ml, from, to, piece_square[from], piece_square[to], Move::FLAG_NONE);
    }
}


void Board::push_piece_capture_moves(MoveList& ml, Bitboard attack, const int from, Color color, PieceType piece)
{
    int to;

    while (attack) {
        to = BB::pop_lsb(attack);
        add_capture_move(ml, from, to, Move::make_piece(color, piece), piece_square[to], Move::FLAG_NONE);
    }
}

//--------------------------------------------------------------------
//  Promotions

void Board::push_quiet_promotions(MoveList& ml, Bitboard attack, const int dir, Color color)
{
    int to;

    while (attack) {
        to = BB::pop_lsb(attack);
        push_quiet_promotion(ml, to - dir, to, color);
    }
}
void Board::push_capture_promotions(MoveList& ml, Bitboard attack, const int dir, Color color)
{
    int to;

    while (attack) {
        to = BB::pop_lsb(attack);
        push_capture_promotion(ml, to - dir, to, color);
    }
}

/* Append promotions from the same move */
void Board::push_quiet_promotion(MoveList& ml, const int from, const int to, Color color)
{
    add_quiet_promotion(ml, from, to, color, Move::make_piece(color, PieceType::QUEEN));
    add_quiet_promotion(ml, from, to, color, Move::make_piece(color, PieceType::KNIGHT));
    add_quiet_promotion(ml, from, to, color, Move::make_piece(color, PieceType::ROOK));
    add_quiet_promotion(ml, from, to, color, Move::make_piece(color, PieceType::BISHOP));
}
void Board::push_capture_promotion(MoveList& ml, const int from, const int to,  Color color)
{
    add_capture_promotion(ml, from, to, color, piece_square[to], Move::make_piece(color, PieceType::QUEEN));
    add_capture_promotion(ml, from, to, color, piece_square[to], Move::make_piece(color, PieceType::KNIGHT));
    add_capture_promotion(ml, from, to, color, piece_square[to], Move::make_piece(color, PieceType::ROOK));
    add_capture_promotion(ml, from, to, color, piece_square[to], Move::make_piece(color, PieceType::BISHOP));
}

//--------------------------------------
//  Coups de pions

void Board::push_pawn_quiet_moves(MoveList& ml, Bitboard attack, const int dir, Color color, U32 flags)
{
    int to;

    while (attack) {
        to = BB::pop_lsb(attack);
        add_quiet_move(ml, to - dir, to, Move::make_piece(color, PieceType::PAWN), flags);
    }
}
void Board::push_pawn_capture_moves(MoveList& ml, Bitboard attack, const int dir, Color color)
{
    int to;

    while (attack) {
        to = BB::pop_lsb(attack);
        add_capture_move(ml, to - dir, to, Move::make_piece(color, PieceType::PAWN), piece_square[to], Move::FLAG_NONE);
    }
}



