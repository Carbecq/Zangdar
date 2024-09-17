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

    Bitboard outposts[N_COLORS];
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
    N_MG =  440, N_EG =  640,
    B_MG =  450, B_EG =  672,
    R_MG =  598, R_EG = 1141,
    Q_MG = 1463, Q_EG = 2052,
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
    S( -15,   14), S(  -4,   15), S(  -2,   10), S( -11,   15), S(   1,   28), S(   9,   19), S(  28,    5), S( -15,   -6),
    S( -32,    6), S( -33,    0), S( -18,   -5), S( -23,   -4), S( -11,    2), S( -21,    6), S(  -9,  -10), S( -24,   -8),
    S( -24,   18), S( -27,   11), S(  -9,   -8), S(   0,  -18), S(  11,  -17), S(  -7,   -7), S( -24,    3), S( -21,    0),
    S( -12,   45), S(  -7,   20), S(   1,    2), S(  -1,  -18), S(  23,  -14), S(  22,   -7), S(  -3,   18), S(  -5,   26),
    S(  39,   58), S(  23,   75), S(  57,   30), S(  65,    9), S(  84,   11), S( 124,   21), S(  93,   69), S(  38,   78),
    S(  53,    2), S(  60,    2), S(  42,   61), S(  89,   17), S(  72,   36), S(  79,   24), S( -37,   76), S( -67,   82),
    S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0)
};

constexpr Score KnightPSQT[N_SQUARES] = { S( -61,  -23), S( -11,  -15), S( -16,    2), S(   9,   15), S(  19,   16), S(  19,   -9), S(  -2,   -5), S(  -9,   -7),
    S( -21,   -9), S(  -9,    3), S(  -3,    0), S(  10,   11), S(  11,    2), S(   0,    7), S(   7,   -5), S(  15,   11),
    S(  -4,   -3), S(   3,    8), S(  17,   18), S(  22,   36), S(  36,   31), S(  30,    9), S(  30,    1), S(  26,    3),
    S(  14,   25), S(  24,    9), S(  30,   37), S(  36,   33), S(  41,   43), S(  54,   19), S(  59,    6), S(  41,   16),
    S(  10,   26), S(  19,   18), S(  36,   30), S(  45,   43), S(  38,   31), S(  76,   18), S(  30,   10), S(  44,    1),
    S( -14,   10), S(   3,   15), S(  -3,   47), S(  13,   46), S(  37,   24), S(  50,   -6), S(  10,  -10), S( -11,  -15),
    S( -25,   -4), S(  -7,   19), S(   7,   14), S(  22,   16), S(  23,   -8), S(  54,  -17), S(   0,    4), S(  -7,  -38),
    S(-190,  -81), S(-128,  -10), S(-140,   35), S( -62,   -3), S( -22,    6), S(-140,    2), S( -97,  -22), S(-137, -122)
};

constexpr Score BishopPSQT[N_SQUARES] = { S(  17,  -29), S(  47,    4), S(  38,   13), S(  19,   16), S(  42,   11), S(  22,   24), S(  45,    0), S(  37,  -54),
    S(  37,   -2), S(  18,  -34), S(  37,   -5), S(  18,   21), S(  24,   19), S(  28,   13), S(  30,  -24), S(  45,  -27),
    S(  22,    4), S(  52,   25), S(  20,    8), S(  25,   38), S(  39,   42), S(  28,   11), S(  62,   11), S(  48,   -8),
    S(  19,    9), S(  11,   29), S(  29,   38), S(  49,   42), S(  36,   41), S(  45,   24), S(  20,   23), S(  54,  -13),
    S(   2,   22), S(  32,   31), S(  34,   31), S(  47,   62), S(  48,   34), S(  30,   37), S(  41,   15), S(   2,   18),
    S(   9,   31), S(  44,   22), S(   1,    8), S(  42,   21), S(  29,   28), S(  38,    7), S(  22,   26), S(  26,   14),
    S(   7,    0), S( -29,    4), S(  11,   20), S( -17,   34), S(  -7,   17), S( -16,   28), S( -63,    6), S( -52,    9),
    S( -44,   12), S( -52,   35), S(-103,   49), S(-115,   51), S(-119,   47), S(-113,   32), S( -35,   18), S( -76,  -14)
};

constexpr Score RookPSQT[N_SQUARES] = { S(   3,   21), S(   0,   19), S(   3,   25), S(  12,    9), S(  16,    1), S(  16,    4), S(  18,    6), S(   6,    3),
    S( -14,   18), S(  -8,   28), S(   8,   23), S(   7,   20), S(  15,    7), S(   2,    3), S(  30,  -12), S(  -3,    0),
    S( -17,   40), S(  -9,   29), S(  -5,   25), S(  -9,   24), S(   2,   14), S(  10,    0), S(  44,  -19), S(  23,  -10),
    S( -15,   58), S( -18,   60), S( -10,   51), S(  -9,   43), S( -10,   38), S( -20,   39), S(  16,   27), S(   0,   27),
    S(  -9,   79), S(  10,   68), S(   9,   71), S(  -3,   62), S(   8,   40), S(  23,   37), S(  27,   46), S(  19,   42),
    S( -18,   79), S(  22,   68), S(   2,   73), S(   1,   62), S(  27,   47), S(  23,   46), S(  63,   41), S(  32,   35),
    S( -19,   76), S( -24,   92), S( -21,  104), S(  -7,   86), S( -28,   87), S( -14,   78), S(  -3,   71), S(  21,   53),
    S(  -2,   71), S( -12,   80), S( -21,   91), S( -33,   80), S( -13,   70), S(  -2,   79), S(   8,   81), S(  26,   70)
};

constexpr Score QueenPSQT[N_SQUARES] = { S(   0,   -7), S(  -8,    1), S(   1,    8), S(  13,   -6), S(   9,   -5), S(  -5,  -16), S(  18,  -58), S(  21,  -44),
    S(  17,   -3), S(  22,    0), S(  23,   14), S(  28,   39), S(  31,   31), S(  23,  -33), S(  49,  -86), S(  50,  -66),
    S(  18,   -3), S(  21,   38), S(  11,   67), S(   9,   64), S(  16,   69), S(  31,   47), S(  49,   22), S(  36,   16),
    S(  23,   29), S(   5,   74), S(   7,   81), S(   3,  112), S(   6,  113), S(  14,   89), S(  32,   81), S(  34,   76),
    S(   1,   58), S(  22,   67), S(   1,   97), S(  -9,  138), S(  -9,  161), S(   2,  147), S(  27,  146), S(  15,  110),
    S(   9,   52), S(   4,   60), S(   0,  114), S( -11,  144), S( -11,  174), S(  11,  162), S(  24,  106), S(  25,  108),
    S(   2,   46), S( -29,   86), S( -40,  143), S( -87,  213), S(-100,  247), S( -48,  176), S( -32,  155), S(  23,  142),
    S( -61,   94), S( -48,   99), S( -25,  134), S(  -2,  133), S( -25,  146), S(   2,  127), S(   3,  101), S( -26,   96)
};

constexpr Score KingPSQT[N_SQUARES] = { S(  56,  -53), S(  90,  -15), S(  69,   19), S( -43,   37), S(  33,   11), S(  -9,   38), S(  65,   -4), S(  54,  -46),
    S( 121,   -3), S( 105,   33), S(  92,   51), S(  38,   71), S(  42,   75), S(  67,   61), S( 116,   30), S(  95,    4),
    S(  36,   17), S( 125,   46), S(  83,   79), S(  57,  101), S(  68,   98), S(  78,   84), S( 106,   54), S(  14,   43),
    S(  14,   29), S(  84,   70), S(  79,  113), S(  10,  149), S(  41,  140), S(  79,  114), S(  64,   82), S( -60,   70),
    S(  19,   45), S(  78,   96), S(  61,  142), S(  26,  176), S(  34,  178), S(  94,  152), S(  89,  117), S( -13,   80),
    S(  13,   60), S( 112,  117), S(  90,  145), S(  69,  175), S( 127,  182), S( 163,  163), S( 143,  145), S(  58,   74),
    S( -14,   43), S(  45,  116), S(  42,  133), S(  89,  126), S(  96,  131), S( 104,  158), S( 103,  152), S(  65,   84),
    S( -21,  -72), S(  21,    6), S(   9,   43), S(   1,   77), S(   5,   68), S(  30,   91), S(  56,  102), S(  23,  -58)
};

//----------------------------------------------------------
// Pions
constexpr Score PawnDoubled = S(  -8,  -50);
constexpr Score PawnDoubled2 = S( -10,  -26);
constexpr Score PawnSupport = S(  21,   16);
constexpr Score PawnOpen = S( -13,  -21);
constexpr Score PawnPhalanx[N_RANKS] = { S(   0,    0), S(   8,   -2), S(  20,   14), S(  35,   37), S(  72,  121), S( 230,  377), S( 167,  409), S(   0,    0)
};
constexpr Score PawnIsolated = S(  -7,  -15);
constexpr Score PawnPassed[N_RANKS] = { S(   0,    0), S( -11,   22), S( -18,   36), S( -70,  107), S( -38,  137), S(  62,  159), S( 313,  195), S(   0,    0)
};
constexpr Score PassedDefended[N_RANKS] = { S(   0,    0), S(   0,    0), S(   5,  -13), S(   5,   -9), S(   4,   35), S(  37,  104), S( 158,   98), S(   0,    0)
};

//----------------------------------------------------------
// Pions passés
constexpr Score PassedSquare = S( -35,  425);
constexpr Score PassedDistUs[N_RANKS] = { S(   0,    0), S(   0,    0), S(   0,    0), S(  17,  -31), S(  17,  -43), S(   1,  -46), S( -16,  -36), S(   0,    0)
};
constexpr Score PassedDistThem = S(  -4,   20);
constexpr Score PassedBlocked[N_RANKS] = { S(   0,    0), S(   0,    0), S(   0,    0), S(   2,    3), S(   0,   10), S( -10,   -1), S( -71,  -31), S(   0,    0)
};
constexpr Score PassedFreeAdv[N_RANKS] = { S(   0,    0), S(   0,    0), S(   0,    0), S(  -5,   22), S( -13,   56), S( -23,  183), S( -81,  292), S(   0,    0)
};
constexpr Score PassedRookBack = S(  27,   42);

//----------------------------------------------------------
// Cavaliers
constexpr Score KnightBehindPawn = S(   2,   28);
constexpr Score KnightOutpost[2][2] = {   { S(  13,   -9), S(  46,   23) },
    { S(  15,   -8), S(  22,    5) }
};

// Fous
constexpr Score BishopPair = S(  32,  112);
constexpr Score BishopBadPawn = S(  -1,   -7);
constexpr Score BishopBehindPawn = S(  11,   23);
constexpr Score BishopLongDiagonal = S(  33,   25);

// Tours
constexpr Score RookOnOpenFile[2] = { S(  13,   20), S(  32,   10)
};
constexpr Score RookOnBlockedFile = S(  -7,   -5);

//----------------------------------------------------------
// Roi
constexpr Score KingLineDanger[28] = { S(   0,    0), S(   0,    0), S(  30,  -29), S(  14,   18), S(  -8,   35), S( -15,   30), S( -19,   30), S( -28,   39),
    S( -40,   42), S( -56,   42), S( -58,   42), S( -75,   47), S( -80,   46), S( -94,   47), S(-107,   50), S(-111,   46),
    S(-118,   44), S(-132,   41), S(-138,   38), S(-147,   32), S(-158,   31), S(-185,   28), S(-182,   26), S(-203,   10),
    S(-171,  -12), S(-143,  -27), S(-130,  -32), S(-135,  -34)
};
constexpr Score KingAttackPawn = S(  -8,   37);
constexpr Score PawnShelter = S(  32,  -11);

//----------------------------------------------------------
// Menaces
constexpr Score PawnThreat = S(  80,   36);
constexpr Score PushThreat = S(  21,    4);
constexpr Score ThreatByMinor[N_PIECES] = { S(   0,    0), S(   0,    0), S(  28,   36), S(  39,   46), S(  76,   15), S(  64,   -4), S(   0,    0)
};
constexpr Score ThreatByRook[N_PIECES] = { S(   0,    0), S(   0,    0), S(  27,   44), S(  33,   54), S( -15,   46), S(  99,  -66), S(   0,    0)
};

//----------------------------------------------------------
// Mobilité
constexpr Score KnightMobility[9] = { S( -69, -125), S( -32,  -10), S(  -7,   51), S(   7,   70), S(  20,   84),
    S(  30,  103), S(  42,  107), S(  54,  111), S(  67,   93)
};

constexpr Score BishopMobility[14] = { S( -49,  -38), S( -20,  -10), S(  -3,   35), S(   8,   62), S(  19,   72),
    S(  29,   85), S(  33,   98), S(  40,  100), S(  41,  105), S(  44,  105),
    S(  54,   99), S(  69,   89), S(  58,   98), S(  83,   70)
};

constexpr Score RookMobility[15] = { S( -98, -149), S(  -9,   38), S(   3,   85), S(  15,   87), S(  10,  119),
    S(  17,  128), S(  12,  144), S(  22,  145), S(  22,  153), S(  28,  157),
    S(  34,  166), S(  29,  178), S(  27,  187), S(  36,  191), S(  44,  185)
};

constexpr Score QueenMobility[28] = { S( -65,  -48), S(-101,  -56), S( -81, -105), S( -11, -115), S(  -5,  -14),
    S( -14,  114), S(  -6,  179), S(  -6,  221), S(  -3,  259), S(   2,  265),
    S(   5,  274), S(   7,  284), S(  14,  284), S(  14,  296), S(  15,  306),
    S(  18,  309), S(  14,  324), S(  13,  333), S(   5,  352), S(   2,  365),
    S(  13,  357), S(  16,  358), S(  51,  338), S(  65,  317), S( 135,  254),
    S( 154,  220), S( 164,  181), S( 134,  172)
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
