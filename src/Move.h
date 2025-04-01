#ifndef LIBCHESS_MOVE_HPP
#define LIBCHESS_MOVE_HPP

#include <ostream>
#include <sstream>
#include "defines.h"
#include "bitmask.h"

namespace Move
{

/* Codage des coups selon une idée de Weiss

0000 0000 0000 0000 0011 1111 -> From             <<  0
0000 0000 0000 1111 1100 0000 -> Dest             <<  6
0000 0000 1111 0000 0000 0000 -> Moving Piece     << 12
0000 1111 0000 0000 0000 0000 -> Captured Piece   << 16
1111 0000 0000 0000 0000 0000 -> Promotion Piece  << 20

Flags : bits 24 à 26

0001 -> Double
0010 -> En-Passant
0100 -> Castle

uuuu uzzz pppp cccc mmmm dddd ddff ffff
        |    |    |    |    |    |    |
0000 0000 0000 0000 0000 0000 0000 0000
      <---  -- move 27 bits        --->



*/

/*  Notes Importantes :
 *      1- la prise enpassant est aussi une capture
 *      2- une promotion avec capture est aussi une capture
 */

constexpr int     SHIFT_FROM  =  0;
constexpr int     SHIFT_DEST  =  6;
constexpr int     SHIFT_PIECE = 12;
constexpr int     SHIFT_CAPT  = 16;
constexpr int     SHIFT_PROMO = 20;
constexpr int     SHIFT_FLAGS = 24;

constexpr U32 MOVE_NONE = 0;

// Fields
//                                                dest  from
//                                  0b0fffPPPPCCCCMMMMDDDDDDFFFFFF
constexpr U32 MOVE_FROM_MASK      = 0b0000000000000000000000111111;

//                                  0b0fffPPPPCCCCMMMMDDDDDDFFFFFF
constexpr U32 MOVE_DEST_MASK      = 0b0000000000000000111111000000;

//                                  0b0fffPPPPCCCCMMMMDDDDDDFFFFFF
constexpr U32 MOVE_FROMDEST_MASK  = 0b0000000000000000111111111111;

//                                      0b0fffPPPPCCCCMMMMDDDDDDFFFFFF
constexpr U32 MOVE_PIECE_MASK         = 0b0000000000001111000000000000;
constexpr U32 MOVE_PIECETYPE_MASK     = 0b0000000000000111000000000000;

//                                      0b0fffPPPPCCCCMMMMDDDDDDFFFFFF
constexpr U32 MOVE_CAPT_MASK          = 0b0000000011110000000000000000;
constexpr U32 MOVE_CAPTTYPE_MASK      = 0b0000000001110000000000000000;

//                                      0b0fffPPPPCCCCMMMMDDDDDDFFFFFF
constexpr U32 MOVE_PROMO_MASK         = 0b0000111100000000000000000000;
constexpr U32 MOVE_PROMOTYPE_MASK     = 0b0000011100000000000000000000;

//                                  0b0fffPPPPCCCCMMMMDDDDDDFFFFFF
constexpr U32 MOVE_FLAGS_MASK     = 0b0111000000000000000000000000;

// Special move flags
constexpr U32 FLAG_NONE           = 0;

//                                  0b0fffPPPPCCCCMMMMDDDDDDFFFFFF
constexpr U32 FLAG_DOUBLE_MASK    = 0b0001000000000000000000000000;
constexpr U32 FLAG_ENPASSANT_MASK = 0b0010000000000000000000000000;
constexpr U32 FLAG_CASTLE_MASK    = 0b0100000000000000000000000000;

//                                  0b0fffPPPPCCCCMMMMDDDDDDFFFFFF
constexpr U32 MOVE_DEPL_MASK      = 0b0111111111110000000000000000;

// NULL_MOVE

// move  = (b1b1)
// from  = b1
// dest  = b1
// piece = NONE
// capt  = NONE
// promo = NONE
// flags = 0

constexpr U32 MOVE_NULL           = 65; // 1000001


[[nodiscard]] constexpr MOVE CODE(
    const int   _from,
    const int   _dest,
    const Piece _piece,
    const Piece _captured,
    const Piece _promotion,
    const U32   _flags)
{
    return( static_cast<U32>(_from)      << SHIFT_FROM  |
            static_cast<U32>(_dest)      << SHIFT_DEST  |
            static_cast<U32>(_piece)     << SHIFT_PIECE |
            static_cast<U32>(_captured)  << SHIFT_CAPT  |
            static_cast<U32>(_promotion) << SHIFT_PROMO |
            _flags
            );
}

//! \brief  Retourne la case de départ
[[nodiscard]] constexpr inline int from(const MOVE move) noexcept {
    return (move & MOVE_FROM_MASK);
}

//! \brief  Retourne la case d'arrivée
[[nodiscard]] constexpr inline int dest(const MOVE move) noexcept {
    return ((move & MOVE_DEST_MASK) >> SHIFT_DEST);
}

//! \brief  Retourne la case d'arrivée et de départ combinées
//! Est utilisé comme indice
[[nodiscard]] constexpr inline int fromdest(const MOVE move) noexcept {
    return (move & MOVE_FROMDEST_MASK);
}

//! \brief  Retourne la pièce se déplaçant
[[nodiscard]] constexpr inline Piece piece(const MOVE move) noexcept {
    return static_cast<Piece>((move & MOVE_PIECE_MASK) >> SHIFT_PIECE);
}

//! \brief  Retourne le type de la pièce se déplaçant
[[nodiscard]] constexpr inline PieceType piece_type(const MOVE move) noexcept {
    return static_cast<PieceType>((move & MOVE_PIECETYPE_MASK) >> SHIFT_PIECE);
}

//! \brief  Retourne la pièce prise
[[nodiscard]] constexpr inline Piece captured(const MOVE move) noexcept {
    return static_cast<Piece>((move & MOVE_CAPT_MASK) >> SHIFT_CAPT);
}

//! \brief  Retourne le type de la pièce prise
[[nodiscard]] constexpr inline PieceType captured_type(const MOVE move) noexcept {
    return static_cast<PieceType>((move & MOVE_CAPTTYPE_MASK) >> SHIFT_CAPT);
}

//! \brief  Retourne la pièce de promotion
[[nodiscard]] constexpr inline Piece promoted(const MOVE move)  noexcept {
    return static_cast<Piece>(((move & MOVE_PROMO_MASK) >> SHIFT_PROMO));
}

//! \brief  Retourne le type de la pièce de promotion
[[nodiscard]] constexpr inline PieceType promoted_type(const MOVE move)  noexcept {
    return static_cast<PieceType>(((move & MOVE_PROMOTYPE_MASK) >> SHIFT_PROMO));
}

//! \brief  Retourne les flags du coup
[[nodiscard]] constexpr inline U32 flags(const MOVE move)  noexcept {
    return static_cast<U32>(move & MOVE_FLAGS_MASK);
}

//! \brief  retourne la couleur de la pièce
[[nodiscard]] constexpr inline Color color(Piece piece) {
    return static_cast<Color>(static_cast<U32>(piece) >> 3);
}

//! \brief  retourne le type de la pièce
[[nodiscard]] constexpr inline PieceType type (Piece piece)
{
    return static_cast<PieceType>(static_cast<U32>(piece) & 7);
}

//! \brief  retourne une pièce à partir de son type et de sa couleur
[[nodiscard]] constexpr inline Piece make_piece(Color color, PieceType pt)
{
    return static_cast<Piece>((color << 3) + static_cast<U32>(pt));
}


//! \brief Détermine si le coup est un déplacement simple
[[nodiscard]] constexpr inline bool is_depl(const MOVE move) noexcept {
    return ((move & MOVE_DEPL_MASK)==MOVE_NONE);
}

//! \brief Détermine si le coup est une poussée double de pion
[[nodiscard]] constexpr inline bool is_double(const MOVE move) noexcept {
    return (move & FLAG_DOUBLE_MASK);
}

//! \brief Détermine si le coup est un roque
[[nodiscard]] constexpr inline bool is_castling(const MOVE move) noexcept {
    return (move & FLAG_CASTLE_MASK);
}

//! \brief Détermine si le coup est une capture, y compris promotion avec capture et prise enpassant
[[nodiscard]] constexpr inline bool is_capturing(const MOVE move) noexcept {
    return (move & MOVE_CAPT_MASK);
}

//! \brief Détermine si le coup est une promotion
[[nodiscard]] constexpr inline bool is_promoting(const MOVE move) noexcept {
    return (move & MOVE_PROMO_MASK);
}

//! \brief Détermine si le coup est une prise en passant
[[nodiscard]] constexpr inline bool is_enpassant(const MOVE move) noexcept {
    return (move & FLAG_ENPASSANT_MASK);
}

//! \brief Détermine si le coup est tactique :
//!     + prise
//!     + promotion (avec ou sans prise)
//!     + prise en passant
[[nodiscard]] constexpr inline bool is_tactical(const MOVE move) noexcept {
    return (move & (MOVE_CAPT_MASK | MOVE_PROMO_MASK | FLAG_ENPASSANT_MASK));
}

[[nodiscard]] constexpr inline bool is_ok(const MOVE m) noexcept {
  return m != MOVE_NONE && m != MOVE_NULL;
}

//=================================================
//! \brief Affichage du coup
//-------------------------------------------------
// exemples :
//  std::string s = Move::name(move) ;
//  std::cout << Move::name(move) ;
//-------------------------------------------------
[[nodiscard]] inline std::string name(U32 move) noexcept
{
    std::string str;
    str += square_name[Move::from(move)];
    str += square_name[Move::dest(move)];
    if (Move::is_promoting(move))
    {
        str += pieceTypeToCharMin(Move::promoted_type(move));
    }

    return str;
}

//========================================
//! \brief  Affichage d'un coup
//! \param[in]  mode    détermine la manière d'écrire le coup
//!
//! mode = 0 : Ff1-c4
//!        1 : Fc4      : résultat Win At Chess
//!        2 : Nbd7     : résultat Win At Chess
//!        3 : R1a7     : résultat Win At Chess
//!        4 : f1c3     : communication UCI
//!
//----------------------------------------
[[nodiscard]] inline std::string show(MOVE move, int mode) noexcept
{
    std::stringstream ss;
    std::string s;

    // ZZ   if ((CASTLE_WK(move) || CASTLE_BK(move) || CASTLE_WQ(move) || CASTLE_BQ(move) ) && (mode != 4))
    //    {
    //            if (CASTLE_WK(move) || CASTLE_BK(move))
    //                ss << "0-0";
    //            else if (CASTLE_WQ(move) || CASTLE_BQ(move))
    //                ss << "0-0-0";
    //    }
    //    else
    {
        int file_from = SQ::file(Move::from(move));
        int file_to   = SQ::file(Move::dest(move));

        int rank_from = SQ::rank(Move::from(move));
        int rank_to   = SQ::rank(Move::dest(move));

        char cfile_from = 'a' + file_from;
        char crank_from = '1' + rank_from;

        char cfile_to = 'a' + file_to;
        char crank_to = '1' + rank_to;

        PieceType ptype = Move::piece_type(move);

        if (mode != 4)
        {
            ss << pieceTypeToCharMax(ptype);
        }

        if (ptype != PieceType::PAWN)
        {
            if (mode == 2)
                ss << cfile_from;
            else if (mode == 3)
                ss << crank_from;
        }

        if (mode == 0 || mode == 4)
        {
            ss << cfile_from;
            ss << crank_from;
        }
        else if (mode == 1)
        {
            if (ptype == PieceType::PAWN && Move::is_capturing(move))
                ss << cfile_from;
        }

        if (mode !=4 && Move::is_capturing(move))
            ss << "x";
        else if (mode == 0)
            ss << "-";

        ss << cfile_to;
        ss << crank_to;

        if (Move::is_promoting(move))
        {
            if (mode == 4)
                ss << pieceTypeToCharMin(Move::promoted_type(move));
            else
                ss << "=" << pieceTypeToCharMax(Move::promoted_type(move));
        }
    }

    ss >> s;

    return(s);
}

inline std::ostream &operator<<(std::ostream &os, const U32 move) noexcept
{
    os << name(move);
    return os;
}

}

#endif



