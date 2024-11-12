#include "Board.h"
#include "Move.h"

/* Le roque nécessite plusieurs conditions :

    1) Toutes les cases qui séparent le roi de la tour doivent être vides.
        Par conséquent, il n'est pas permis de prendre une pièce adverse en roquant ;
    2) Ni le roi, ni la tour ne doivent avoir quitté leur position initiale.
        Par conséquent, le roi et la tour doivent être dans la première rangée du joueur,
        et chaque camp ne peut roquer qu'une seule fois par partie ;
        en revanche, le roi peut déjà avoir subi des échecs, s'il s'est soustrait à ceux-ci sans se déplacer.
    3) Aucune des cases (de départ, de passage ou d'arrivée) par lesquelles transite le roi lors du roque
        ne doit être contrôlée par une pièce adverse.
        Par conséquent, le roque n'est pas possible si le roi est en échec.
    4) En revanche la tour, ainsi que sa case adjacente dans le cas du grand roque, peuvent être menacées.


    A B C D E F G H
    T       R     T
    T         T R
        R T

    */

//=======================================================================
//! \brief  Ajoute les coups du roque
//-----------------------------------------------------------------------
template <Color C, CastleSide side>
constexpr inline void Board::gen_castle(MoveList& ml)
{
    if (   can_castle<C, side>()
        && BB::empty(get_rook_path<C, side>() & occupancy_all())
        && !(squares_attacked<~C>() & get_king_path<C, side>()) )
    {
        add_quiet_move(ml, get_king_from<C>(), get_king_dest<C, side>(), KING, Move::FLAG_CASTLE_MASK);
    }
}

//=================================================================
//! \brief  Ajoute un coup tranquille à la liste des coups
//!
//! \param[in]  ml      MoveList de stockage des coups
//! \param[in]  from    position de départ de la pièce
//! \param[in]  dest    position d'arrivée de la pièce
//! \param[in]  piece   type de la pièce jouant
//! \param[in]  flags   flags du coup (avance double, prise en-passant, roque)
//-----------------------------------------------------------------
void Board::add_quiet_move(MoveList& ml, int from, int dest, PieceType piece, U32 flags)  const noexcept
{
    ml.mlmoves[ml.count++].move = Move::CODE(from, dest, piece, NO_TYPE, NO_TYPE, flags);
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
void Board::add_capture_move(MoveList& ml, int from, int dest, PieceType piece, PieceType captured, U32 flags) const noexcept
{
    ml.mlmoves[ml.count++].move  = Move::CODE(from, dest, piece, captured, NO_TYPE, flags);
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
void Board::add_quiet_promotion(MoveList& ml, int from, int dest, PieceType promo) const noexcept
{
    ml.mlmoves[ml.count++].move = Move::CODE(from, dest, PAWN, PieceType::NO_TYPE, promo, Move::FLAG_NONE);
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
void Board::add_capture_promotion(MoveList& ml, int from, int dest, PieceType captured, PieceType promo) const noexcept
{
    ml.mlmoves[ml.count++].move  = Move::CODE(from, dest, PAWN, captured, promo, Move::FLAG_NONE);
}






//===================================================================
//! \brief  Ajoute une série de coups
//-------------------------------------------------------------------
void Board::push_quiet_moves(MoveList& ml, Bitboard attack, const int from)
{
    int to;

    while (attack) {
        to = BB::pop_lsb(attack);
        add_quiet_move(ml, from, to, pieceOn[from], Move::FLAG_NONE);
    }
}
void Board::push_capture_moves(MoveList& ml, Bitboard attack, const int from)
{
    int to;

    while (attack) {
        to = BB::pop_lsb(attack);
        add_capture_move(ml, from, to, pieceOn[from], pieceOn[to], Move::FLAG_NONE);
    }
}

void Board::push_piece_quiet_moves(MoveList& ml, Bitboard attack, const int from, PieceType piece)
{
    int to;

    while (attack) {
        to = BB::pop_lsb(attack);
        add_quiet_move(ml, from, to, piece, Move::FLAG_NONE);
    }
}

void Board::push_piece_capture_moves(MoveList& ml, Bitboard attack, const int from, PieceType piece)
{
    int to;

    while (attack) {
        to = BB::pop_lsb(attack);
        add_capture_move(ml, from, to, piece, pieceOn[to], Move::FLAG_NONE);
    }
}

//--------------------------------------------------------------------
//  Promotions

void Board::push_quiet_promotions(MoveList& ml, Bitboard attack, const int dir) {
    int to;

    while (attack) {
        to = BB::pop_lsb(attack);
        push_quiet_promotion(ml, to - dir, to);
    }
}
void Board::push_capture_promotions(MoveList& ml, Bitboard attack, const int dir) {
    int to;

    while (attack) {
        to = BB::pop_lsb(attack);
        push_capture_promotion(ml, to - dir, to);
    }
}

/* Append promotions from the same move */
void Board::push_quiet_promotion(MoveList& ml, const int from, const int to) {
    add_quiet_promotion(ml, from, to, QUEEN);
    add_quiet_promotion(ml, from, to, KNIGHT);
    add_quiet_promotion(ml, from, to, ROOK);
    add_quiet_promotion(ml, from, to, BISHOP);
}
void Board::push_capture_promotion(MoveList& ml, const int from, const int to) {
    add_capture_promotion(ml, from, to, pieceOn[to], QUEEN);
    add_capture_promotion(ml, from, to, pieceOn[to], KNIGHT);
    add_capture_promotion(ml, from, to, pieceOn[to], ROOK);
    add_capture_promotion(ml, from, to, pieceOn[to], BISHOP);
}

//--------------------------------------
//  Coups de pions

void Board::push_pawn_quiet_moves(MoveList& ml, Bitboard attack, const int dir, U32 flags)
{
    int to;

    while (attack) {
        to = BB::pop_lsb(attack);
        add_quiet_move(ml, to - dir, to, PAWN, flags);
    }
}
void Board::push_pawn_capture_moves(MoveList& ml, Bitboard attack, const int dir)
{
    int to;

    while (attack) {
        to = BB::pop_lsb(attack);
        add_capture_move(ml, to - dir, to, PAWN, pieceOn[to], Move::FLAG_NONE);
    }
}



template void Board::gen_castle<Color::WHITE, CastleSide::KING_SIDE>(MoveList& ml);
template void Board::gen_castle<Color::WHITE, CastleSide::QUEEN_SIDE>(MoveList& ml);

template void Board::gen_castle<Color::BLACK, CastleSide::KING_SIDE>(MoveList& ml);
template void Board::gen_castle<Color::BLACK, CastleSide::QUEEN_SIDE>(MoveList& ml);
