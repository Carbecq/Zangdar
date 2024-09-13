#ifndef EVALUATE_H
#define EVALUATE_H

#include "defines.h"
#include "types.h"

// Valeur des pièces
//  mg = middle game
//  eg = end game

// Idée de Stockfish, reprise par de nombreux codes

/// Score enum stores a middlegame and an endgame value in a single integer (enum).
/// The least significant 16 bits are used to store the middlegame value and the
/// upper 16 bits are used to store the endgame value. We have to take care to
/// avoid left-shifting a signed int to avoid undefined behavior.

#define MakeScore(mg, eg) ((int)((unsigned int)(eg) << 16) + (mg))
#define S(mg, eg) MakeScore((mg), (eg))
#define MgScore(s) ((int16_t)((uint16_t)((unsigned)((s)))))
#define EgScore(s) ((int16_t)((uint16_t)((unsigned)((s) + 0x8000) >> 16)))


struct EvalInfo {
    Bitboard occupied;
    Bitboard pawns[N_COLORS];
    Bitboard knights[N_COLORS];
    Bitboard bishops[N_COLORS];
    Bitboard rooks[N_COLORS];
    Bitboard queens[N_COLORS];
    Bitboard passedPawns;

    // pions qui se bloquent mutuellement : pion blanc b5, pion noir b6
    Bitboard rammedPawns[N_COLORS];
    // pions bloqués par une pièce amie ou ennemie
    // les rammedPawns sont aussi des blockedPawns
    Bitboard blockedPawns[N_COLORS];

    Bitboard mobilityArea[N_COLORS];
    Bitboard KingZone[N_COLORS];

    int attackPower[N_COLORS] = {0, 0};
    int attackCount[N_COLORS] = {0, 0};

    Bitboard attackedBy[N_COLORS][N_PIECES];
    Bitboard attacked[N_COLORS];

    int phase24;

};

//----------------------------------------------------------
//  Valeur des pièces
enum PieceValueEMG {
    P_MG =  107, P_EG =  210,
    N_MG =  440, N_EG =  637,
    B_MG =  448, B_EG =  671,
    R_MG =  598, R_EG = 1139,
    Q_MG = 1465, Q_EG = 2040,
    K_MG =    0, K_EG =    0
};


enum Phase { MG, EG };

//-----------------------------------------------------------------
//  Tables PeSTO
//  Valeurs de Weiss
//  Re-tunées ensuite
//-----------------------------------------------------------------
//http://www.talkchess.com/forum3/viewtopic.php?f=2&t=68311

//  Valeur des pièces
constexpr int MGPieceValue[N_PIECES] = {
    0, P_MG, N_MG, B_MG, R_MG, Q_MG, K_MG
};
constexpr int EGPieceValue[N_PIECES] = {
    0, P_EG, N_EG, B_EG, R_EG, Q_EG, K_EG
};

constexpr Score PawnValue   = S(P_MG, P_EG);
constexpr Score KnightValue = S(N_MG, N_EG);
constexpr Score BishopValue = S(B_MG, B_EG);
constexpr Score RookValue   = S(R_MG, R_EG);
constexpr Score QueenValue  = S(Q_MG, Q_EG);
constexpr Score KingValue   = S(K_MG, K_EG);

//========================================================== DEBUT TUNER

//----------------------------------------------------------
//  Bonus positionnel des pièces
//  Du point de vue des Blancs - Mes tables sont comme ça
constexpr Score PawnPSQT[N_SQUARES] = { S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0),
    S( -15,   14), S(  -5,   15), S(  -2,    9), S( -10,   15), S(   0,   28), S(   9,   19), S(  28,    4), S( -15,   -6),
    S( -32,    6), S( -33,   -1), S( -18,   -6), S( -23,   -4), S( -12,    2), S( -21,    5), S(  -9,  -11), S( -23,   -8),
    S( -24,   18), S( -27,   11), S(  -9,   -9), S(   0,  -19), S(  11,  -17), S(  -7,   -8), S( -24,    2), S( -21,   -1),
    S( -12,   44), S(  -7,   20), S(   2,    2), S(  -1,  -18), S(  23,  -14), S(  22,   -7), S(  -3,   18), S(  -5,   25),
    S(  40,   58), S(  23,   75), S(  57,   30), S(  64,   10), S(  82,   11), S( 123,   21), S(  94,   69), S(  38,   78),
    S(  54,    3), S(  61,    2), S(  42,   62), S(  87,   20), S(  73,   36), S(  77,   25), S( -36,   77), S( -64,   81),
    S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0)
};

constexpr Score KnightPSQT[N_SQUARES] = { S( -61,  -27), S( -11,  -16), S( -16,    3), S(   9,   15), S(  20,   16), S(  19,   -9), S(  -3,   -5), S( -12,  -10),
    S( -22,  -11), S( -10,    3), S(  -3,    1), S(  10,   11), S(  11,    3), S(   0,    6), S(   6,   -5), S(  15,   11),
    S(  -4,   -4), S(   2,    8), S(  17,   18), S(  22,   37), S(  35,   31), S(  30,    9), S(  29,    1), S(  26,    3),
    S(  14,   23), S(  23,   10), S(  30,   37), S(  36,   34), S(  40,   44), S(  53,   19), S(  59,    8), S(  41,   16),
    S(  10,   24), S(  19,   19), S(  36,   31), S(  45,   44), S(  38,   32), S(  75,   19), S(  29,   10), S(  44,    0),
    S( -14,    8), S(   3,   14), S(  -3,   47), S(  13,   46), S(  40,   23), S(  50,   -4), S(  10,  -10), S( -10,  -17),
    S( -24,   -4), S(  -7,   18), S(   8,   13), S(  24,   15), S(  25,   -9), S(  54,  -18), S(  -1,    4), S(  -6,  -40),
    S(-192,  -80), S(-128,  -12), S(-140,   34), S( -62,   -3), S( -20,    5), S(-140,    5), S( -95,  -23), S(-139, -120)
};

constexpr Score BishopPSQT[N_SQUARES] = { S(  16,  -29), S(  47,    5), S(  37,   13), S(  18,   16), S(  42,   11), S(  22,   24), S(  44,    0), S(  37,  -52),
    S(  36,   -2), S(  18,  -34), S(  37,   -5), S(  17,   21), S(  24,   19), S(  27,   13), S(  29,  -24), S(  45,  -27),
    S(  21,    4), S(  52,   24), S(  19,    8), S(  25,   38), S(  39,   41), S(  27,   11), S(  61,   10), S(  47,   -7),
    S(  19,    9), S(  10,   28), S(  29,   38), S(  49,   41), S(  35,   41), S(  45,   23), S(  19,   23), S(  53,  -12),
    S(   1,   22), S(  31,   31), S(  33,   30), S(  46,   62), S(  47,   34), S(  30,   36), S(  40,   16), S(   2,   19),
    S(   8,   30), S(  43,   22), S(   2,    7), S(  41,   21), S(  29,   27), S(  37,    6), S(  21,   26), S(  26,   13),
    S(   6,    0), S( -29,    3), S(  10,   20), S( -17,   33), S(  -7,   17), S( -15,   28), S( -63,    5), S( -52,   10),
    S( -46,   14), S( -53,   35), S(-105,   49), S(-116,   52), S(-121,   47), S(-114,   33), S( -34,   19), S( -74,  -13)
};

constexpr Score RookPSQT[N_SQUARES] = { S(   2,   22), S(   0,   19), S(   2,   25), S(  11,    9), S(  15,    0), S(  16,    4), S(  17,    6), S(   6,    3),
    S( -14,   18), S(  -9,   28), S(   8,   23), S(   7,   20), S(  15,    6), S(   3,    3), S(  29,  -11), S(  -4,    1),
    S( -18,   39), S(  -9,   29), S(  -5,   25), S( -10,   24), S(   1,   15), S(  10,    0), S(  44,  -19), S(  21,   -9),
    S( -16,   58), S( -18,   59), S( -11,   51), S(  -9,   43), S( -11,   38), S( -21,   40), S(  15,   27), S(  -1,   28),
    S( -10,   78), S(  10,   68), S(   9,   71), S(  -3,   61), S(   7,   40), S(  23,   36), S(  27,   46), S(  18,   42),
    S( -18,   78), S(  22,   68), S(   1,   73), S(   1,   62), S(  27,   47), S(  23,   45), S(  63,   41), S(  31,   35),
    S( -19,   75), S( -24,   92), S( -21,  103), S(  -7,   85), S( -27,   86), S( -13,   77), S(  -3,   71), S(  21,   53),
    S(  -1,   70), S( -11,   78), S( -22,   91), S( -32,   79), S( -13,   69), S(  -2,   79), S(   9,   80), S(  26,   69)
};

constexpr Score QueenPSQT[N_SQUARES] = { S(   1,   -9), S(  -8,    0), S(   1,    7), S(  14,   -7), S(   9,   -6), S(  -5,  -17), S(  19,  -58), S(  22,  -44),
    S(  17,   -4), S(  22,   -1), S(  24,   13), S(  28,   38), S(  31,   30), S(  23,  -33), S(  49,  -86), S(  49,  -65),
    S(  18,   -3), S(  21,   38), S(  11,   65), S(   9,   63), S(  16,   67), S(  31,   46), S(  49,   21), S(  36,   15),
    S(  24,   28), S(   5,   75), S(   7,   81), S(   3,  111), S(   6,  112), S(  14,   89), S(  32,   80), S(  34,   75),
    S(   2,   57), S(  23,   67), S(   1,   96), S(  -9,  137), S(  -9,  161), S(   2,  146), S(  28,  146), S(  15,  108),
    S(   9,   52), S(   5,   60), S(   0,  114), S( -10,  143), S( -11,  173), S(  12,  162), S(  24,  106), S(  24,  108),
    S(   3,   46), S( -29,   85), S( -40,  143), S( -86,  213), S(-100,  248), S( -48,  175), S( -31,  155), S(  23,  141),
    S( -61,   94), S( -49,   99), S( -25,  135), S(  -1,  133), S( -24,  146), S(   1,  126), S(   2,   99), S( -24,   96)
};

constexpr Score KingPSQT[N_SQUARES] = { S(  55,  -53), S(  90,  -16), S(  69,   19), S( -43,   38), S(  33,   11), S( -10,   38), S(  65,   -4), S(  53,  -46),
    S( 120,   -2), S( 105,   33), S(  91,   51), S(  37,   71), S(  42,   75), S(  67,   61), S( 115,   30), S(  95,    4),
    S(  35,   17), S( 124,   46), S(  82,   79), S(  57,  101), S(  69,   98), S(  78,   84), S( 107,   54), S(  14,   43),
    S(  13,   29), S(  83,   70), S(  78,  113), S(  10,  149), S(  41,  140), S(  78,  114), S(  64,   83), S( -59,   70),
    S(  19,   46), S(  76,   96), S(  59,  142), S(  25,  176), S(  34,  178), S(  93,  152), S(  87,  116), S( -13,   80),
    S(  13,   60), S( 111,  117), S(  88,  145), S(  69,  175), S( 128,  182), S( 162,  164), S( 142,  145), S(  59,   75),
    S( -14,   44), S(  45,  117), S(  41,  134), S(  89,  126), S(  96,  131), S( 105,  158), S( 103,  152), S(  65,   84),
    S( -21,  -72), S(  21,    6), S(   9,   43), S(   1,   78), S(   5,   69), S(  30,   92), S(  57,  102), S(  23,  -58)
};

//----------------------------------------------------------
// Pions
constexpr Score PawnDoubled = S(  -8,  -50);
constexpr Score PawnDoubled2 = S( -10,  -27);
constexpr Score PawnSupport = S(  21,   17);
constexpr Score PawnOpen = S( -13,  -21);
constexpr Score PawnPhalanx[N_RANKS] = { S(   0,    0), S(   8,   -2), S(  20,   14), S(  35,   37), S(  72,  121), S( 231,  376), S( 167,  410), S(   0,    0)
};
constexpr Score PawnIsolated = S(  -7,  -14);
constexpr Score PawnPassed[N_RANKS] = { S(   0,    0), S( -11,   22), S( -18,   36), S( -71,  107), S( -38,  137), S(  61,  158), S( 311,  192), S(   0,    0)
};
constexpr Score PassedDefended[N_RANKS] = { S(   0,    0), S(   0,    0), S(   4,  -13), S(   5,   -9), S(   5,   34), S(  38,  103), S( 159,   96), S(   0,    0)
};

//----------------------------------------------------------
// Pions passés
constexpr Score PassedSquare = S( -36,  424);
constexpr Score PassedDistUs[N_RANKS] = { S(   0,    0), S(   0,    0), S(   0,    0), S(  17,  -31), S(  17,  -42), S(   0,  -46), S( -17,  -35), S(   0,    0)
};
constexpr Score PassedDistThem = S(  -4,   20);
constexpr Score PassedBlocked[N_RANKS] = { S(   0,    0), S(   0,    0), S(   0,    0), S(   2,    2), S(   0,   10), S( -10,   -1), S( -73,  -28), S(   0,    0)
};
constexpr Score PassedFreeAdv[N_RANKS] = { S(   0,    0), S(   0,    0), S(   0,    0), S(  -5,   22), S( -13,   56), S( -23,  182), S( -83,  292), S(   0,    0)
};
constexpr Score PassedRookBack = S(  25,   37);

//----------------------------------------------------------
// Cavaliers
constexpr Score KnightBehindPawn = S(   2,   27);
constexpr Score KnightOutpost[2][2] = {   { S(  12,   -9), S(  46,   23) },
    { S(  15,   -7), S(  22,    6) }
};

// Fous
constexpr Score BishopPair = S(  32,  112);
constexpr Score BishopBadPawn = S(  -1,   -6);
constexpr Score BishopBehindPawn = S(  11,   23);
constexpr Score BishopLongDiagonal = S(  33,   25);

// Tours
constexpr Score RookOnOpenFile[2] = { S(  14,   20), S(  32,   10)
};
constexpr Score RookOnBlockedFile = S(  -7,   -5);

//----------------------------------------------------------
// Roi
constexpr Score KingLineDanger[28] = { S(   0,    0), S(   0,    0), S(  30,  -29), S(  14,   19), S(  -8,   35), S( -15,   31), S( -19,   30), S( -28,   39),
    S( -40,   42), S( -56,   42), S( -58,   42), S( -75,   47), S( -79,   46), S( -93,   47), S(-106,   50), S(-110,   47),
    S(-119,   44), S(-132,   42), S(-137,   38), S(-146,   32), S(-158,   31), S(-186,   29), S(-183,   26), S(-204,   11),
    S(-172,  -11), S(-144,  -26), S(-130,  -32), S(-135,  -34)
};
constexpr Score KingAttackPawn = S(  -8,   37);
constexpr Score PawnShelter = S(  31,  -11);

//----------------------------------------------------------
// Menaces
constexpr Score PawnThreat = S(  80,   36);
constexpr Score PushThreat = S(  22,    4);
constexpr Score ThreatByMinor[N_PIECES] = { S(   0,    0), S(   0,    0), S(  28,   36), S(  39,   46), S(  75,   15), S(  64,   -4), S(   0,    0)
};
constexpr Score ThreatByRook[N_PIECES] = { S(   0,    0), S(   0,    0), S(  27,   44), S(  33,   54), S( -15,   47), S(  99,  -65), S(   0,    0)
};

//----------------------------------------------------------
// Mobilité
constexpr Score KnightMobility[9] = { S( -68, -123), S( -32,   -8), S(  -7,   52), S(   6,   71), S(  20,   84),
    S(  30,  104), S(  41,  108), S(  54,  111), S(  67,   94)
};

constexpr Score BishopMobility[14] = { S( -48,  -39), S( -19,  -11), S(  -2,   33), S(   9,   60), S(  20,   71),
    S(  30,   84), S(  34,   96), S(  41,   99), S(  41,  104), S(  45,  104),
    S(  54,   98), S(  70,   88), S(  59,   97), S(  82,   69)
};

constexpr Score RookMobility[15] = { S( -97, -149), S( -10,   36), S(   3,   83), S(  14,   86), S(  10,  117),
    S(  16,  127), S(  12,  142), S(  21,  144), S(  22,  152), S(  27,  156),
    S(  33,  165), S(  29,  177), S(  26,  185), S(  35,  190), S(  43,  184)
};

constexpr Score QueenMobility[28] = { S( -65,  -48), S(-102,  -56), S( -80, -104), S( -11, -114), S(  -5,  -12),
    S( -14,  118), S(  -7,  182), S(  -7,  223), S(  -4,  261), S(   2,  267),
    S(   4,  278), S(   7,  287), S(  13,  287), S(  14,  299), S(  15,  309),
    S(  18,  312), S(  13,  326), S(  13,  335), S(   5,  355), S(   2,  368),
    S(  12,  360), S(  15,  360), S(  49,  341), S(  65,  319), S( 136,  256),
    S( 155,  222), S( 165,  183), S( 135,  173)
};



//============================================================== FIN TUNER

//------------------------------------------------------------
//  Sécurité du Roi
constexpr Score AttackPower[7]   = { 0, 0, 36, 22, 23, 78, 0 };
constexpr Score CheckPower[7]    = { 0, 0, 68, 44, 88, 92, 0 };
constexpr Score CountModifier[8] = { 0, 0, 63, 126, 96, 124, 124, 128 };

//------------------------------------------------------------
//  Bonus car on a le trait
constexpr int Tempo = 18;


#endif // EVALUATE_H
