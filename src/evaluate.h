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
    P_MG =  100, P_EG =  100,
    N_MG =  300, N_EG =  300,
    B_MG =  300, B_EG =  300,
    R_MG =  500, R_EG = 500,
    Q_MG = 900, Q_EG = 900,
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
constexpr Score PawnPSQT[N_SQUARES] = {
    S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0),
    S( -14,   10), S(  -4,   13), S(   1,    7), S(  -6,   11), S(   8,   23), S(  11,   16), S(  29,    2), S( -15,   -7),
    S( -31,    3), S( -32,   -2), S( -19,   -7), S( -22,   -6), S(  -9,    0), S( -22,    5), S(  -8,  -12), S( -24,   -9),
    S( -24,   16), S( -28,   10), S( -10,   -9), S(  -5,  -18), S(   2,  -16), S(  -6,   -8), S( -25,    2), S( -21,   -2),
    S( -13,   43), S(  -9,   19), S(  -1,    2), S(   2,  -19), S(  28,  -16), S(  20,   -7), S(  -5,   18), S(  -6,   24),
    S(  35,   59), S(  19,   75), S(  55,   30), S(  62,   10), S(  79,   12), S( 122,   20), S(  90,   69), S(  34,   78),
    S(  54,    4), S(  58,    6), S(  41,   63), S(  84,   22), S(  71,   36), S(  80,   25), S( -34,   80), S( -61,   80),
    S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0)
};

constexpr Score KnightPSQT[N_SQUARES] = {
    S( -66,  -31), S( -16,  -18), S( -21,    0), S(   5,   13), S(  14,   14), S(  12,  -10), S( -10,   -6), S( -18,  -13),
    S( -26,  -14), S( -16,    1), S(  -8,   -1), S(   6,    9), S(   6,    1), S(  -6,    5), S(  -1,   -6), S(   9,    9),
    S( -11,   -6), S(  -1,    6), S(  14,   16), S(  16,   36), S(  30,   31), S(  26,    9), S(  27,   -1), S(  20,    0),
    S(  16,   23), S(  28,   11), S(  33,   41), S(  41,   40), S(  42,   47), S(  52,   24), S(  57,   11), S(  41,   15),
    S(  16,   22), S(  28,   22), S(  49,   38), S(  60,   49), S(  42,   39), S(  79,   28), S(  29,   15), S(  46,    0),
    S(  -6,    4), S(  26,    8), S(  15,   43), S(  37,   43), S(  57,   21), S(  66,    0), S(  21,  -11), S(  -5,  -20),
    S( -25,   -7), S( -10,   16), S(   8,    8), S(  23,   12), S(  24,  -13), S(  52,  -21), S(  -5,    3), S(  -7,  -42),
    S(-193,  -78), S(-125,  -13), S(-141,   33), S( -63,   -4), S( -19,    3), S(-141,    8), S( -93,  -23), S(-141, -120)
};

constexpr Score BishopPSQT[N_SQUARES] = {
    S(  30,  -15), S(  44,    1), S(  34,    9), S(  14,   12), S(  37,    7), S(  19,   19), S(  40,   -3), S(  52,  -40),
    S(  35,   -8), S(  31,  -21), S(  35,  -12), S(  13,   15), S(  20,   14), S(  24,    7), S(  45,  -12), S(  43,  -32),
    S(  15,   -1), S(  49,   19), S(  37,   24), S(  21,   31), S(  35,   36), S(  43,   30), S(  57,    6), S(  42,  -12),
    S(  15,    4), S(   5,   22), S(  26,   30), S(  42,   35), S(  28,   35), S(  40,   17), S(  15,   17), S(  49,  -16),
    S(  -3,   19), S(  27,   25), S(  28,   23), S(  41,   55), S(  42,   27), S(  24,   29), S(  36,   11), S(   0,   14),
    S(   5,   26), S(  37,   17), S(  19,   23), S(  34,   15), S(  22,   21), S(  52,   21), S(  16,   20), S(  22,    9),
    S(   1,   -2), S(  -3,   20), S(   5,   15), S( -23,   28), S( -13,   11), S( -21,   22), S( -40,   21), S( -59,    7),
    S( -21,   31), S( -57,   31), S(-109,   46), S(-120,   48), S(-124,   43), S(-120,   30), S( -36,   14), S( -58,    3)
};

constexpr Score RookPSQT[N_SQUARES] = {
    S(   3,   34), S(   0,   30), S(   3,   32), S(  11,   13), S(  15,    6), S(  14,   17), S(  16,   18), S(   5,   15),
    S( -13,   26), S(  -8,   36), S(   9,   28), S(   8,   22), S(  16,   11), S(   2,   13), S(  30,   -2), S(  -6,    9),
    S( -19,   42), S( -12,   34), S(  -6,   29), S(  -8,   27), S(   1,   19), S(   7,    4), S(  31,  -14), S(  10,   -5),
    S( -15,   55), S( -20,   60), S( -12,   52), S(  -7,   43), S(  -9,   38), S( -21,   40), S(   5,   29), S(  -8,   25),
    S(  -9,   73), S(  11,   65), S(  11,   68), S(  -2,   58), S(  11,   36), S(  23,   33), S(  24,   40), S(  15,   36),
    S( -18,   70), S(  22,   63), S(   3,   68), S(   2,   56), S(  28,   41), S(  27,   38), S(  60,   32), S(  28,   28),
    S( -20,   66), S( -25,   84), S( -21,   98), S(  -7,   82), S( -28,   82), S( -16,   68), S(  -9,   62), S(  17,   43),
    S(   1,   60), S(  -8,   71), S( -20,   86), S( -31,   75), S( -13,   65), S(  -4,   70), S(   6,   70), S(  23,   59)
};

constexpr Score QueenPSQT[N_SQUARES] = {
    S(   2,  -12), S(  -8,   -3), S(   1,    5), S(  13,   -8), S(   9,   -7), S(  -5,  -18), S(  19,  -60), S(  23,  -45),
    S(  17,   -6), S(  22,   -3), S(  24,   11), S(  28,   36), S(  30,   30), S(  24,  -36), S(  50,  -88), S(  48,  -66),
    S(  15,   -2), S(  21,   36), S(  12,   62), S(   8,   61), S(  16,   65), S(  30,   44), S(  48,   20), S(  34,   16),
    S(  23,   28), S(   6,   71), S(   8,   78), S(   4,  108), S(   6,  109), S(  14,   88), S(  31,   80), S(  33,   76),
    S(   2,   57), S(  23,   66), S(   2,   94), S(  -8,  135), S(  -7,  159), S(   2,  146), S(  29,  146), S(  15,  109),
    S(  10,   52), S(   5,   59), S(   0,  113), S( -11,  142), S( -11,  171), S(  13,  160), S(  23,  104), S(  24,  109),
    S(   3,   48), S( -29,   86), S( -39,  141), S( -87,  212), S(-102,  247), S( -48,  173), S( -32,  156), S(  21,  141),
    S( -60,   92), S( -47,   97), S( -24,  133), S(  -2,  131), S( -24,  144), S(   3,  125), S(   2,  100), S( -24,   96)
};

constexpr Score KingPSQT[N_SQUARES] = {
    S(  56,  -51), S(  88,  -14), S(  68,   20), S( -43,   39), S(  33,   12), S( -10,   39), S(  63,   -3), S(  52,  -44),
    S( 118,   -1), S( 103,   35), S(  90,   51), S(  36,   71), S(  41,   75), S(  66,   61), S( 113,   31), S(  93,    5),
    S(  34,   17), S( 124,   46), S(  82,   78), S(  57,  100), S(  68,   96), S(  76,   84), S( 105,   54), S(  13,   44),
    S(  14,   29), S(  85,   69), S(  81,  111), S(  12,  146), S(  43,  139), S(  81,  113), S(  66,   82), S( -58,   70),
    S(  19,   47), S(  78,   96), S(  62,  141), S(  27,  175), S(  36,  176), S(  95,  151), S(  89,  117), S( -13,   81),
    S(  13,   61), S( 112,  117), S(  90,  144), S(  70,  174), S( 130,  181), S( 163,  163), S( 144,  145), S(  59,   77),
    S( -14,   45), S(  46,  116), S(  42,  133), S(  88,  125), S(  96,  130), S( 105,  157), S( 104,  153), S(  65,   84),
    S( -21,  -71), S(  21,    6), S(   9,   43), S(   1,   77), S(   5,   68), S(  30,   91), S(  56,  102), S(  23,  -59)
};

//----------------------------------------------------------
// Pions
constexpr Score PawnDoubled = S( -11,  -49);
constexpr Score PawnDoubled2 = S( -12,  -25);
constexpr Score PawnSupport = S(  21,   16);
constexpr Score PawnOpen = S( -15,  -22);
constexpr Score PawnPhalanx[N_RANKS] = {
    S(   0,    0), S(   8,   -2), S(  19,   14), S(  35,   36), S(  73,  120), S( 230,  375), S( 167,  409), S(   0,    0) };
constexpr Score PawnIsolated = S(  -7,  -14);
constexpr Score PawnPassed[N_RANKS] = {
    S(   0,    0), S( -12,   20), S( -18,   35), S( -73,  109), S( -42,  138), S(  61,  159), S( 311,  194), S(   0,    0) };
constexpr Score PassedDefended[N_RANKS] = {
    S(   0,    0), S(   0,    0), S(   2,  -14), S(   3,  -10), S(   3,   34), S(  35,  102), S( 157,   95), S(   0,    0) };

//----------------------------------------------------------
// Pions passés
constexpr Score PassedSquare = S( -36,  422);
constexpr Score PassedDistUs[N_RANKS] = {
    S(   0,    0), S(   0,    0), S(   0,    0), S(  17,  -32), S(  17,  -43), S(   0,  -46), S( -17,  -35), S(   0,    0) };
constexpr Score PassedDistThem = S(  -3,   19);
constexpr Score PassedBlocked[N_RANKS] = {
    S(   0,    0), S(   0,    0), S(   0,    0), S(   4,   -3), S(   0,    6), S(  -9,   -5), S( -74,  -36), S(   0,    0) };
constexpr Score PassedFreeAdv[N_RANKS] = {
    S(   0,    0), S(   0,    0), S(   0,    0), S(  -4,   24), S( -12,   57), S( -23,  184), S( -83,  290), S(   0,    0) };
constexpr Score PassedRookBack = S(  23,   46);

//----------------------------------------------------------
// Cavaliers
constexpr Score KnightBehindPawn = S(   7,   24);
constexpr Score KnightOutpost[2][2] = {
    {S(  12, -32), S(  40,   0)},
    {S(   7, -24), S(  21,  -3)},
    };

// Fous
constexpr Score BishopPair = S(  32,  112);
constexpr Score BishopBehindPawn = S(   7,   24);
constexpr Score BishopBadPawn = S(  -1,   -7);

// Tours
constexpr Score OpenForward = S(  29,   31);
constexpr Score SemiForward = S(  17,   18);

//----------------------------------------------------------
// Roi
constexpr Score KingLineDanger[28] = {
    S(   0,    0), S(   0,    0), S(  28,  -29), S(  13,   19), S( -10,   35), S( -16,   30), S( -20,   29), S( -29,   38),
    S( -40,   41), S( -57,   42), S( -58,   42), S( -75,   47), S( -78,   46), S( -93,   47), S(-105,   49), S(-109,   46),
    S(-116,   44), S(-131,   41), S(-135,   38), S(-144,   32), S(-156,   31), S(-184,   30), S(-180,   27), S(-203,   12),
    S(-171,   -9), S(-143,  -25), S(-130,  -32), S(-135,  -34)
};
constexpr Score KingAttackPawn = S(  -9,   36);
constexpr Score PawnShelter = S(  31,  -12);

//----------------------------------------------------------
// Menaces
constexpr Score PawnThreat = S(  82,   38);
constexpr Score PushThreat = S(  24,    5);
constexpr Score ThreatByMinor[N_PIECES] = {
    S(   0,    0), S(   0,    0), S(  31,   36), S(  40,   45), S(  75,   14), S(  62,   -5), S(   0,    0) };
constexpr Score ThreatByRook[N_PIECES] = {
    S(   0,    0), S(   0,    0), S(  27,   45), S(  36,   50), S( -15,   49), S(  99,  -66), S(   0,    0) };

//----------------------------------------------------------
// Mobilité
constexpr Score KnightMobility[9] = {
    S( -64, -123), S( -31,   -7), S(  -6,   53), S(   8,   73), S(  21,   86),
    S(  31,  105), S(  42,  109), S(  55,  111), S(  67,   93)
};

constexpr Score BishopMobility[14] = {
    S( -50,  -40), S( -18,  -13), S(  -1,   30), S(   9,   57), S(  22,   67),
    S(  34,   82), S(  38,   95), S(  46,   98), S(  46,  105), S(  50,  105),
    S(  57,  100), S(  74,   90), S(  61,  100), S(  85,   71)
};

constexpr Score RookMobility[15] = {
    S( -98, -150), S( -11,   38), S(   3,   83), S(  13,   86), S(   9,  118),
    S(  15,  127), S(  11,  142), S(  20,  143), S(  21,  150), S(  27,  155),
    S(  33,  162), S(  29,  173), S(  27,  181), S(  35,  186), S(  42,  183)
};

constexpr Score QueenMobility[28] = {
    S( -65,  -48), S(-102,  -56), S( -80, -104), S( -11, -115), S(  -6,  -13),
    S( -15,  117), S(  -7,  180), S(  -7,  220), S(  -4,  258), S(   2,  264),
    S(   4,  274), S(   7,  284), S(  13,  286), S(  13,  299), S(  14,  309),
    S(  16,  313), S(  12,  327), S(  12,  336), S(   4,  355), S(   1,  368),
    S(  12,  360), S(  14,  359), S(  49,  341), S(  63,  319), S( 136,  256),
    S( 154,  221), S( 165,  182), S( 134,  173)
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
