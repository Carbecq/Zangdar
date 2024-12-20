#include <sstream>
#include "Board.h"

//===================================================
//! \brief  Constructeur
//---------------------------------------------------
Board::Board() noexcept
{
}

Board::Board(const std::string &fen) noexcept
{
    set_fen(fen, false);
}

//===================================================
//! \brief  Affichage de l'échiquier
//---------------------------------------------------
std::string Board::display() const noexcept
{
    std::stringstream ss;

    int i = 56;
    U64 hh = get_hash();
    U64 ph = get_pawn_hash();
    ss << std::endl;

    ss << "  8 " ;

    while (i >= 0) {
        const auto sq = i;
        const auto bb = SQ::square_BB(sq);
        
        if (occupancy_cp<Color::WHITE, PAWN>() & bb) {
            ss << 'P';
        } else if (occupancy_cp<Color::WHITE, KNIGHT>() & bb) {
            ss << 'N';
        } else if (occupancy_cp<Color::WHITE, BISHOP>() & bb) {
            ss << 'B';
        } else if (occupancy_cp<Color::WHITE, ROOK>() & bb) {
            ss << 'R';
        } else if (occupancy_cp<Color::WHITE, QUEEN>() & bb) {
            ss << 'Q';
        } else if (occupancy_cp<Color::WHITE, KING>() & bb) {
            ss << 'K';
        } else if (occupancy_cp<Color::BLACK, PAWN>() & bb) {
            ss << 'p';
        } else if (occupancy_cp<Color::BLACK, KNIGHT>() & bb) {
            ss << 'n';
        } else if (occupancy_cp<Color::BLACK, BISHOP>() & bb) {
            ss << 'b';
        } else if (occupancy_cp<Color::BLACK, ROOK>() & bb) {
            ss << 'r';
        } else if (occupancy_cp<Color::BLACK, QUEEN>() & bb) {
            ss << 'q';
        } else if (occupancy_cp<Color::BLACK, KING>() & bb) {
            ss << 'k';
        } else {
            ss << '.';
        }
        ss << ' ';

        if (i % 8 == 7)
        {
            if (i/8 != 0)
                ss << "\n  " << i/8 << ' ';
            i -= 16;
        }

        i++;
    }
    ss << "\n    a b c d e f g h\n\n";

    ss << "Castling : ";
    ss << (can_castle<WHITE, CastleSide::KING_SIDE>()  ? "K" : "");
    ss << (can_castle<WHITE, CastleSide::QUEEN_SIDE>() ? "Q" : "");
    ss << (can_castle<BLACK, CastleSide::KING_SIDE>()  ? "k" : "");
    ss << (can_castle<BLACK, CastleSide::QUEEN_SIDE>() ? "q" : "");
    ss << '\n';
    if (ep() == NO_SQUARE) {
        ss << "EP       : -\n";
    } else {
        ss << "EP       : " << square_name[ep()] << '\n';
    }
    ss <<     "Turn     : " << side_name[turn()] << '\n';
    ss <<     "InCheck  : " << bool_name[is_in_check()] << "\n";
    ss <<     "Hash     : " << hh << "\n";
    ss <<     "PawnHash : " << ph << "\n";
    ss <<     "Fen      : " << get_fen() << "\n";

    return(ss.str());
}

void Board::clear() noexcept
{
    colorPiecesBB[0] = 0ULL;
    colorPiecesBB[1] = 0ULL;

    typePiecesBB[0] = 0ULL;
    typePiecesBB[1] = 0ULL;
    typePiecesBB[2] = 0ULL;
    typePiecesBB[3] = 0ULL;
    typePiecesBB[4] = 0ULL;
    typePiecesBB[5] = 0ULL;
    typePiecesBB[6] = 0ULL;
    
    pieceOn.fill(NO_TYPE);

    x_king[WHITE] = NO_SQUARE;                      // position des rois
    x_king[BLACK] = NO_SQUARE;                      // position des rois

    halfmove_counter = 0;
    fullmove_counter = 1;
    gamemove_counter = 0;
    hash             = 0;
    pawn_hash        = 0;

    ep_square    = NO_SQUARE;
    castling     = CASTLE_NONE;
    side_to_move = Color::WHITE;
    
    game_history.fill(UndoInfo{0, 0, 0, 0, 0, 0, 0, 0});
}

//==========================================================
//! \brief  Calcule la phase de la position
//! Cette phase dépend des pièces sur l'échiquier
//! La phase va de 0 à 24, dans le cas où aucun pion n'a été promu.
//!     phase =  0 : endgame
//!     phase = 24 : opening
//! Remarque : le code Fruit va à l'envers.
//----------------------------------------------------------
int Board::get_phase24()
{
    return(  4 * BB::count_bit(typePiecesBB[QUEEN])
           + 2 * BB::count_bit(typePiecesBB[ROOK])
           +     BB::count_bit(typePiecesBB[BISHOP] | typePiecesBB[KNIGHT]) );
}


//=============================================================
//! \brief  Calcule la valeur du matériel restant
//-------------------------------------------------------------
template <Color US>
int Board::get_material()
{
    return(
          P_MG * BB::count_bit(occupancy_cp<US, PAWN>())
        + N_MG * BB::count_bit(occupancy_cp<US, KNIGHT>())
        + B_MG * BB::count_bit(occupancy_cp<US, BISHOP>())
        + R_MG * BB::count_bit(occupancy_cp<US, ROOK>())
        + Q_MG * BB::count_bit(occupancy_cp<US, QUEEN>())
        );
}

template int Board::get_material<WHITE>();
template int Board::get_material<BLACK>();
