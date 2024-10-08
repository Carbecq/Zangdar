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
    Bitboard passedPawns;

    // pions qui se bloquent mutuellement : pion blanc b5, pion noir b6
    // Bitboard rammedPawns[N_COLORS];

    // pions bloqués par une pièce amie ou ennemie
    // les rammedPawns sont aussi des blockedPawns
    Bitboard blockedPawns[N_COLORS];

    Bitboard outposts[N_COLORS];
    Bitboard mobilityArea[N_COLORS];
    Bitboard KingZone[N_COLORS];

    int attackPower[N_COLORS] = {0, 0};
    int attackCount[N_COLORS] = {0, 0};

    Bitboard attacks[N_COLORS][N_PIECES];
    Bitboard allAttacks[N_COLORS];

    int phase24;

};

//----------------------------------------------------------
//  Valeur des pièces
enum PieceValueEMG {
    P_MG =  107, P_EG =  210,
    N_MG =  440, N_EG =  637,
    B_MG =  451, B_EG =  672,
    R_MG =  598, R_EG = 1140,
    Q_MG = 1476, Q_EG = 2039,
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
    S( -15,   14), S(  -5,   15), S(  -2,    9), S( -10,   15), S(   1,   28), S(   9,   19), S(  28,    4), S( -15,   -6),
    S( -32,    6), S( -33,    0), S( -18,   -5), S( -23,   -4), S( -11,    2), S( -21,    6), S(  -9,  -10), S( -23,   -8),
    S( -24,   18), S( -27,   11), S(  -8,   -9), S(   0,  -18), S(  11,  -17), S(  -7,   -7), S( -24,    3), S( -21,   -1),
    S( -13,   45), S(  -7,   20), S(   2,    2), S(  -1,  -18), S(  23,  -14), S(  22,   -7), S(  -3,   18), S(  -5,   25),
    S(  39,   59), S(  22,   75), S(  56,   30), S(  64,   10), S(  82,   11), S( 123,   21), S(  93,   69), S(  38,   78),
    S(  53,    3), S(  58,    3), S(  42,   62), S(  88,   19), S(  73,   36), S(  79,   24), S( -39,   77), S( -63,   80),
    S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0)
};

constexpr Score KnightPSQT[N_SQUARES] = { S( -62,  -27), S( -11,  -16), S( -16,    2), S(   9,   15), S(  20,   16), S(  20,   -9), S(  -3,   -5), S( -12,  -10),
    S( -21,  -11), S( -11,    3), S(  -3,    1), S(  10,   11), S(  11,    2), S(   0,    7), S(   6,   -5), S(  15,   11),
    S(  -5,   -3), S(   2,    7), S(  17,   18), S(  22,   37), S(  35,   32), S(  30,    9), S(  29,    1), S(  26,    2),
    S(  14,   23), S(  24,   10), S(  30,   37), S(  36,   34), S(  41,   44), S(  54,   19), S(  59,    8), S(  41,   15),
    S(   9,   25), S(  19,   19), S(  36,   32), S(  45,   44), S(  38,   32), S(  75,   19), S(  30,   10), S(  45,   -1),
    S( -14,    8), S(   3,   14), S(  -3,   47), S(  13,   46), S(  40,   22), S(  50,   -4), S(  10,  -10), S( -11,  -17),
    S( -22,   -5), S(  -7,   18), S(   9,   13), S(  24,   15), S(  26,   -9), S(  54,  -18), S(  -1,    5), S(  -5,  -41),
    S(-193,  -79), S(-128,  -12), S(-140,   34), S( -62,   -3), S( -20,    5), S(-140,    5), S( -95,  -23), S(-140, -120)
};

constexpr Score BishopPSQT[N_SQUARES] = { S(  17,  -29), S(  48,    4), S(  39,   12), S(  19,   14), S(  42,   10), S(  22,   24), S(  45,    0), S(  37,  -51),
    S(  37,   -3), S(  18,  -35), S(  38,   -6), S(  18,   20), S(  24,   19), S(  27,   13), S(  30,  -24), S(  44,  -27),
    S(  20,    4), S(  53,   23), S(  20,    7), S(  25,   37), S(  38,   41), S(  27,   11), S(  60,   11), S(  46,   -7),
    S(  18,    9), S(   9,   28), S(  29,   37), S(  49,   40), S(  36,   40), S(  43,   24), S(  17,   23), S(  46,   -8),
    S(  -1,   22), S(  31,   31), S(  33,   30), S(  46,   61), S(  47,   33), S(  30,   35), S(  33,   20), S(   2,   19),
    S(   8,   30), S(  43,   21), S(   2,    7), S(  41,   20), S(  30,   26), S(  36,    7), S(  22,   25), S(  25,   13),
    S(   7,   -1), S( -28,    2), S(  10,   20), S( -18,   33), S(  -7,   16), S( -15,   27), S( -63,    5), S( -52,    9),
    S( -45,   15), S( -53,   34), S(-106,   49), S(-116,   51), S(-121,   47), S(-116,   33), S( -34,   18), S( -72,  -11)
};

constexpr Score RookPSQT[N_SQUARES] = { S(   3,   21), S(  -1,   20), S(   2,   25), S(  10,   10), S(  15,    0), S(  16,    4), S(  17,    6), S(   5,    3),
    S( -14,   18), S(  -8,   28), S(   8,   23), S(   6,   20), S(  15,    6), S(   3,    3), S(  29,  -11), S(  -5,    2),
    S( -18,   39), S(  -8,   28), S(  -6,   25), S( -11,   24), S(   1,   14), S(  10,    0), S(  45,  -19), S(  22,   -9),
    S( -15,   58), S( -18,   59), S( -12,   51), S( -11,   43), S( -11,   38), S( -21,   40), S(  16,   27), S(  -1,   28),
    S( -10,   78), S(  10,   68), S(   8,   72), S(  -3,   61), S(   7,   40), S(  23,   37), S(  27,   46), S(  19,   41),
    S( -18,   78), S(  22,   68), S(   2,   73), S(   1,   61), S(  28,   46), S(  24,   44), S(  64,   40), S(  30,   36),
    S( -21,   77), S( -24,   92), S( -20,  103), S(  -6,   85), S( -26,   85), S( -14,   77), S(  -4,   72), S(  20,   53),
    S(  -1,   70), S( -10,   78), S( -21,   91), S( -31,   78), S( -12,   69), S(  -1,   79), S(   9,   80), S(  26,   69)
};

constexpr Score QueenPSQT[N_SQUARES] = { S(   0,  -11), S( -10,    0), S(  -2,    8), S(  11,   -5), S(   6,   -5), S(  -7,  -17), S(  15,  -59), S(  19,  -46),
    S(  17,   -6), S(  20,   -1), S(  21,   15), S(  27,   39), S(  29,   32), S(  23,  -34), S(  47,  -87), S(  48,  -66),
    S(  26,  -11), S(  20,   35), S(  11,   64), S(   8,   64), S(  14,   68), S(  35,   42), S(  47,   19), S(  40,   10),
    S(  36,   11), S(   5,   72), S(   4,   82), S(   4,  109), S(   6,  111), S(  18,   85), S(  32,   76), S(  35,   71),
    S(   6,   50), S(  21,   66), S(  -1,   96), S(  -9,  136), S( -11,  161), S(   4,  145), S(  25,  146), S(  17,  102),
    S(  12,   47), S(   3,   59), S(  -2,  114), S( -13,  143), S( -12,  172), S(  15,  160), S(  22,  105), S(  23,  110),
    S(  -2,   49), S( -33,   87), S( -43,  143), S( -89,  212), S(-102,  245), S( -50,  174), S( -35,  156), S(  20,  143),
    S( -57,   91), S( -48,   96), S( -24,  133), S(  -1,  132), S( -23,  144), S(   3,  125), S(   4,  101), S( -22,   95)
};

constexpr Score KingPSQT[N_SQUARES] = { S(  55,  -53), S(  90,  -16), S(  69,   19), S( -44,   39), S(  33,   11), S( -11,   39), S(  64,   -3), S(  52,  -46),
    S( 119,   -1), S( 104,   33), S(  90,   51), S(  36,   71), S(  41,   75), S(  66,   62), S( 114,   31), S(  94,    5),
    S(  35,   17), S( 123,   46), S(  82,   79), S(  57,  101), S(  68,   98), S(  77,   84), S( 105,   55), S(  13,   43),
    S(  15,   29), S(  85,   69), S(  80,  112), S(  12,  149), S(  43,  140), S(  80,  114), S(  66,   82), S( -58,   70),
    S(  20,   45), S(  78,   96), S(  62,  141), S(  28,  175), S(  37,  178), S(  95,  152), S(  90,  116), S( -12,   79),
    S(  15,   60), S( 113,  117), S(  90,  144), S(  72,  174), S( 130,  182), S( 163,  163), S( 143,  145), S(  58,   75),
    S( -14,   43), S(  45,  116), S(  42,  133), S(  88,  125), S(  96,  130), S( 104,  157), S( 103,  152), S(  64,   82),
    S( -22,  -72), S(  21,    7), S(   9,   42), S(   2,   77), S(   4,   68), S(  30,   91), S(  55,  102), S(  22,  -58)
};

//----------------------------------------------------------
// Pions
constexpr Score PawnDoubled = S(  -8,  -50);
constexpr Score PawnDoubled2 = S( -10,  -26);
constexpr Score PawnSupport = S(  21,   17);
constexpr Score PawnOpen = S( -14,  -21);
constexpr Score PawnPhalanx[N_RANKS] = { S(   0,    0), S(   9,   -2), S(  20,   14), S(  35,   37), S(  72,  121), S( 231,  376), S( 167,  409), S(   0,    0)
};
constexpr Score PawnIsolated = S(  -7,  -14);
constexpr Score PawnPassed[N_RANKS] = { S(   0,    0), S( -12,   22), S( -18,   36), S( -71,  107), S( -38,  137), S(  61,  159), S( 312,  197), S(   0,    0)
};
constexpr Score PassedDefended[N_RANKS] = { S(   0,    0), S(   0,    0), S(   4,  -13), S(   5,   -9), S(   4,   35), S(  37,  104), S( 158,   97), S(   0,    0)
};

//----------------------------------------------------------
// Pions passés
constexpr Score PassedSquare = S( -35,  424);
constexpr Score PassedDistUs[N_RANKS] = { S(   0,    0), S(   0,    0), S(   0,    0), S(  17,  -31), S(  17,  -43), S(   1,  -46), S( -16,  -36), S(   0,    0)
};
constexpr Score PassedDistThem = S(  -3,   20);
constexpr Score PassedBlocked[N_RANKS] = { S(   0,    0), S(   0,    0), S(   0,    0), S(   2,    3), S(  -1,   10), S( -11,    0), S( -70,  -31), S(   0,    0)
};
constexpr Score PassedFreeAdv[N_RANKS] = { S(   0,    0), S(   0,    0), S(   0,    0), S(  -5,   23), S( -13,   56), S( -23,  183), S( -82,  292), S(   0,    0)
};
constexpr Score PassedRookBack = S(  24,   44);

//----------------------------------------------------------
// Cavaliers
constexpr Score KnightBehindPawn = S(   3,   27);
constexpr Score KnightOutpost[2][2] = {   { S(  12,   -9), S(  46,   23) },
    { S(  15,   -7), S(  22,    6) }
};

// Fous
constexpr Score BishopPair = S(  31,  113);
constexpr Score BishopBadPawn = S(  -1,   -7);
constexpr Score BishopBehindPawn = S(  11,   23);
constexpr Score BishopLongDiagonal = S(  33,   24);

// Tours
constexpr Score RookOnOpenFile[2] = { S(  14,   20), S(  32,   10)
};
constexpr Score RookOnBlockedFile = S(  -6,   -6);

// Reines
constexpr Score QueenRelativePin = S( -34,   17);

//----------------------------------------------------------
// Roi
constexpr Score KingLineDanger[28] = { S(   0,    0), S(   0,    0), S(  30,  -29), S(  14,   19), S(  -8,   35), S( -15,   31), S( -19,   30), S( -28,   39),
    S( -40,   42), S( -56,   42), S( -59,   42), S( -75,   47), S( -79,   47), S( -94,   47), S(-107,   50), S(-110,   47),
    S(-119,   45), S(-132,   42), S(-138,   38), S(-146,   32), S(-158,   31), S(-185,   29), S(-182,   26), S(-203,   10),
    S(-171,  -11), S(-143,  -26), S(-130,  -32), S(-135,  -34)
};
constexpr Score KingAttackPawn = S(  -9,   38);
constexpr Score PawnShelter = S(  31,  -11);

//----------------------------------------------------------
// Menaces
constexpr Score PawnThreat = S(  81,   36);
constexpr Score PushThreat = S(  22,    4);
constexpr Score ThreatByMinor[N_PIECES] = { S(   0,    0), S(   0,    0), S(  27,   37), S(  39,   45), S(  76,   15), S(  64,   -4), S(   0,    0)
};
constexpr Score ThreatByRook[N_PIECES] = { S(   0,    0), S(   0,    0), S(  25,   45), S(  32,   55), S( -14,   46), S(  99,  -65), S(   0,    0)
};

//----------------------------------------------------------
// Mobilité
constexpr Score KnightMobility[9] = { S( -66, -125), S( -31,   -8), S(  -6,   52), S(   8,   71), S(  21,   84),
    S(  31,  104), S(  43,  108), S(  56,  111), S(  68,   94)
};

constexpr Score BishopMobility[14] = { S( -49,  -40), S( -21,  -10), S(  -4,   34), S(   7,   60), S(  18,   72),
    S(  28,   84), S(  32,   96), S(  40,   99), S(  41,  104), S(  44,  104),
    S(  53,   98), S(  68,   89), S(  56,   98), S(  82,   68)
};

constexpr Score RookMobility[15] = { S( -98, -149), S( -10,   37), S(   2,   83), S(  13,   87), S(   9,  118),
    S(  16,  128), S(  11,  143), S(  21,  144), S(  21,  153), S(  27,  157),
    S(  33,  165), S(  30,  176), S(  28,  185), S(  37,  190), S(  45,  184)
};

constexpr Score QueenMobility[28] = { S( -65,  -48), S(-101,  -56), S( -80, -105), S(  -9, -116), S(  -4,  -14),
    S( -12,  115), S(  -5,  178), S(  -5,  220), S(  -2,  259), S(   3,  264),
    S(   5,  275), S(   8,  285), S(  14,  285), S(  14,  296), S(  16,  307),
    S(  19,  309), S(  14,  324), S(  13,  333), S(   5,  352), S(   3,  365),
    S(  12,  358), S(  14,  358), S(  50,  338), S(  65,  316), S( 135,  254),
    S( 154,  220), S( 163,  181), S( 133,  172)
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
