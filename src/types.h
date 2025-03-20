#ifndef TYPES_H
#define TYPES_H

#include <string>
#include "defines.h"

/*******************************************************
 ** Les couleurs
 **---------------------------------------------------*/

#include <string>

constexpr int N_COLORS = 2;

enum Color : U08
{
    WHITE    = 0,
    BLACK    = 1
};

const std::string side_name[N_COLORS] = {"White", "Black"};
const std::string camp[2][N_COLORS] = {
    {"Blanche", "Noire"}, {"Blanc", "Noir"}
};

const std::string bool_name[2] = { "false", "true" };

//Inverts the color (WHITE -> BLACK) and (BLACK -> WHITE)
constexpr Color operator~(Color C) { return Color(C ^ Color::BLACK); }

/*******************************************************
 ** Les pièces
 **---------------------------------------------------*/

constexpr int N_PIECE     = 15;

enum class Piece : U08
{
    NONE           = 0, // 0000

    WHITE_PAWN     = 1, // 0001
    WHITE_KNIGHT   ,
    WHITE_BISHOP   ,
    WHITE_ROOK     ,
    WHITE_QUEEN    ,
    WHITE_KING     = 6,

    BLACK_PAWN     = 9, // 1001
    BLACK_KNIGHT   ,
    BLACK_BISHOP   ,
    BLACK_ROOK     ,
    BLACK_QUEEN    ,
    BLACK_KING     = 14,

 };

constexpr std::initializer_list<Piece> all_PIECE = {
    Piece::WHITE_PAWN, Piece::WHITE_KNIGHT, Piece::WHITE_BISHOP,
    Piece::WHITE_ROOK, Piece::WHITE_QUEEN,  Piece::WHITE_KING,
    Piece::BLACK_PAWN, Piece::BLACK_KNIGHT, Piece::BLACK_BISHOP,
    Piece::BLACK_ROOK, Piece::BLACK_QUEEN,  Piece::BLACK_KING
};

constexpr int N_PIECE_TYPE = 7;

enum class PieceType : U08
{
    NONE     = 0,
    PAWN     = 1,
    KNIGHT   = 2,
    BISHOP   = 3,
    ROOK     = 4,
    QUEEN    = 5,
    KING     = 6,
};

constexpr std::initializer_list<PieceType> all_PIECE_TYPE = {
    PieceType::PAWN, PieceType::KNIGHT, PieceType::BISHOP,
    PieceType::ROOK, PieceType::QUEEN,  PieceType::KING
};


//  Valeur des pièces

constexpr int EGPieceValue[N_PIECE_TYPE] = {
    0, 221, 676, 701, 1192, 2101, 0
};


[[nodiscard]] constexpr auto pieceFromChar(char c)
{
    switch (c)
    {
    case 'p': return Piece::  BLACK_PAWN;
    case 'P': return Piece::  WHITE_PAWN;
    case 'n': return Piece::BLACK_KNIGHT;
    case 'N': return Piece::WHITE_KNIGHT;
    case 'b': return Piece::BLACK_BISHOP;
    case 'B': return Piece::WHITE_BISHOP;
    case 'r': return Piece::  BLACK_ROOK;
    case 'R': return Piece::  WHITE_ROOK;
    case 'q': return Piece:: BLACK_QUEEN;
    case 'Q': return Piece:: WHITE_QUEEN;
    case 'k': return Piece::  BLACK_KING;
    case 'K': return Piece::  WHITE_KING;
    default : return Piece::       NONE;
    }
}

[[nodiscard]] constexpr auto pieceToChar(Piece piece)
{
    switch (piece)
    {
    case Piece::        NONE: return ' ';
    case Piece::  BLACK_PAWN: return 'p';
    case Piece::  WHITE_PAWN: return 'P';
    case Piece::BLACK_KNIGHT: return 'n';
    case Piece::WHITE_KNIGHT: return 'N';
    case Piece::BLACK_BISHOP: return 'b';
    case Piece::WHITE_BISHOP: return 'B';
    case Piece::  BLACK_ROOK: return 'r';
    case Piece::  WHITE_ROOK: return 'R';
    case Piece:: BLACK_QUEEN: return 'q';
    case Piece:: WHITE_QUEEN: return 'Q';
    case Piece::  BLACK_KING: return 'k';
    case Piece::  WHITE_KING: return 'K';
    default: return '?';
    }
}

[[nodiscard]] constexpr auto pieceTypeFromChar(char c)
{
    switch (c)
    {
    case 'p': return PieceType::PAWN;
    case 'n': return PieceType::KNIGHT;
    case 'b': return PieceType::BISHOP;
    case 'r': return PieceType::  ROOK;
    case 'q': return PieceType:: QUEEN;
    case 'k': return PieceType::  KING;
    default : return PieceType::  NONE;
    }
}

[[nodiscard]] constexpr auto pieceTypeToCharMin(PieceType piece)
{
    switch (piece)
    {
    case PieceType::  NONE: return ' ';
    case PieceType::  PAWN: return 'p';
    case PieceType::KNIGHT: return 'n';
    case PieceType::BISHOP: return 'b';
    case PieceType::  ROOK: return 'r';
    case PieceType:: QUEEN: return 'q';
    case PieceType::  KING: return 'k';
    default: return '?';
    }
}

[[nodiscard]] constexpr auto pieceTypeToCharMax(PieceType piece)
{
    switch (piece)
    {
    case PieceType::  NONE: return ' ';
    case PieceType::  PAWN: return 'P';
    case PieceType::KNIGHT: return 'N';
    case PieceType::BISHOP: return 'B';
    case PieceType::  ROOK: return 'R';
    case PieceType:: QUEEN: return 'Q';
    case PieceType::  KING: return 'K';
    default: return '?';
    }
}

[[nodiscard]] constexpr auto pieceTypeToChar(PieceType piece)
{
    switch (piece)
    {
    case PieceType::  NONE: return "None";
    case PieceType::  PAWN: return "Pawn";
    case PieceType::KNIGHT: return "Knight";
    case PieceType::BISHOP: return "Bishop";
    case PieceType::  ROOK: return "Rook";
    case PieceType:: QUEEN: return "Queen";
    case PieceType::  KING: return "King";
    default: return "???";
    }
}

/*******************************************************
 ** Les cases
 **---------------------------------------------------*/

constexpr int N_SQUARES = 64;

enum SquareType : int {
    A1, B1, C1, D1, E1, F1, G1, H1,
    A2, B2, C2, D2, E2, F2, G2, H2,
    A3, B3, C3, D3, E3, F3, G3, H3,
    A4, B4, C4, D4, E4, F4, G4, H4,
    A5, B5, C5, D5, E5, F5, G5, H5,
    A6, B6, C6, D6, E6, F6, G6, H6,
    A7, B7, C7, D7, E7, F7, G7, H7,
    A8, B8, C8, D8, E8, F8, G8, H8,
    SQUARE_NONE
};

const std::string square_name[N_SQUARES] = {
    "a1", "b1", "c1", "d1", "e1", "f1", "g1", "h1",
    "a2", "b2", "c2", "d2", "e2", "f2", "g2", "h2",
    "a3", "b3", "c3", "d3", "e3", "f3", "g3", "h3",
    "a4", "b4", "c4", "d4", "e4", "f4", "g4", "h4",
    "a5", "b5", "c5", "d5", "e5", "f5", "g5", "h5",
    "a6", "b6", "c6", "d6", "e6", "f6", "g6", "h6",
    "a7", "b7", "c7", "d7", "e7", "f7", "g7", "h7",
    "a8", "b8", "c8", "d8", "e8", "f8", "g8", "h8",
};

enum Rank : int { RANK_1, RANK_2, RANK_3, RANK_4, RANK_5, RANK_6, RANK_7, RANK_8, NO_RANK };
enum File : int { FILE_A, FILE_B, FILE_C, FILE_D, FILE_E, FILE_F, FILE_G, FILE_H, NO_FILE };

constexpr int N_RANKS = 8;
constexpr int N_FILES = 8;

// https://www.chessprogramming.org/General_Setwise_Operations#Shifting_Bitboards

enum Direction : int {
    NORTH       =   8,
    NORTH_EAST  =   9,
    EAST        =   1,
    SOUTH_EAST  =  -7,
    SOUTH       =  -8,
    SOUTH_WEST  =  -9,
    WEST        =  -1,
    NORTH_WEST  =   7,
    NORTH_NORTH =  16,
    SOUTH_SOUTH = -16,
} ;

struct PVariation {
    MOVE line[MAX_PLY+1];
    int  length = 0;
};


//! \brief  Détermination du signe d'une valeur
//! \return -1 ou +1
template <typename T>
[[nodiscard]] constexpr int Sign(T val) {
    return (T(0) < val) - (val < T(0));
}

//==================================================


//TODO : noisy : uniquement la promotion en dame
//                la mettre avant les captures dans le choix du coup

enum MoveGenType {
    NOISY = 1,              // captures, promotions, prise en passant
    QUIET = 2,              // déplacements, roque, pas de capture, pas de promotion
    ALL   = NOISY | QUIET
};

//==================================================
//  Roque
//--------------------------------------------------

/* This is the castle_mask array. We can use it to determine
the castling permissions after a move. What we do is
logical-AND the castle bits with the castle_mask bits for
both of the move's ints. Let's say castle is 1, meaning
that white can still castle kingside. Now we play a move
where the rook on h1 gets captured. We AND castle with
castle_mask[63], so we have 1&14, and castle becomes 0 and
white can't castle kingside anymore.
 (TSCP) */

constexpr U32 castle_mask[N_SQUARES] = {
    13, 15, 15, 15, 12, 15, 15, 14,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    7, 15, 15, 15,  3, 15, 15, 11
};

constexpr U32 CASTLE_NONE = 0;
constexpr U32 CASTLE_WK = 1;
constexpr U32 CASTLE_WQ = 2;
constexpr U32 CASTLE_BK = 4;
constexpr U32 CASTLE_BQ = 8;

enum class CastleSide {
    KING_SIDE,
    QUEEN_SIDE
};

constexpr static Bitboard F1G1_BB = 0x60;                   // petit roque ; cases entre le roi et la tour
constexpr static Bitboard F8G8_BB = 0x6000000000000000;

constexpr static Bitboard B1D1_BB = 0xE;                    // grand roque ; cases entre le roi et la tour
constexpr static Bitboard B8D8_BB = 0xE00000000000000;

constexpr static Bitboard C1D1_BB = 0xC;                    // grand roque ; cases qui ne doivent pas être attaquées
constexpr static Bitboard C8D8_BB = 0xC00000000000000;

//==================================================
//! Données initialisées à chaque début de recherche
//--------------------------------------------------

using PieceTo = I16[N_PIECE][N_SQUARES];

struct SearchInfo {
    MOVE        excluded{};   // coup à éviter
    int         eval{};       // évaluation statique
    MOVE        move{};       // coup cherché
    int         ply{};        // profondeur de recherche
    PVariation  pv{};         // Principale Variation
    int         doubleExtensions{};
    PieceTo*    continuation_history;
    MOVE        killer1{};      //TODO vérifier initialisation
    MOVE        killer2{};
}__attribute__((aligned(64)));





#endif // TYPES_H

