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
    P_MG =  109, P_EG =  210,
    N_MG =  439, N_EG =  638,
    B_MG =  447, B_EG =  669,
    R_MG =  595, R_EG = 1118,
    Q_MG = 1475, Q_EG = 2030,
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
    S( -16,   10), S(  -6,   12), S(  -3,    7), S(  -9,   11), S(   5,   22), S(   8,   16), S(  27,    1), S( -16,   -8),
    S( -33,    2), S( -34,   -3), S( -21,   -8), S( -23,   -7), S( -12,   -1), S( -25,    4), S( -10,  -13), S( -25,  -10),
    S( -26,   15), S( -29,    8), S( -10,  -11), S(  -6,  -19), S(   1,  -17), S(  -7,  -10), S( -25,    0), S( -22,   -3),
    S( -15,   42), S(  -9,   18), S(  -2,    1), S(   2,  -21), S(  28,  -17), S(  18,   -8), S(  -5,   16), S(  -7,   23),
    S(  33,   59), S(  18,   75), S(  54,   30), S(  60,   11), S(  78,   12), S( 121,   20), S(  90,   68), S(  33,   78),
    S(  56,    4), S(  56,    9), S(  40,   65), S(  83,   24), S(  73,   36), S(  80,   26), S( -34,   81), S( -59,   79),
    S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0)
};

constexpr Score KnightPSQT[N_SQUARES] = { S( -66,  -34), S( -13,  -19), S( -18,    0), S(   7,   13), S(  18,   14), S(  18,  -10), S(  -6,   -7), S( -17,  -15),
    S( -21,  -17), S( -11,    2), S(  -5,    0), S(   9,   10), S(  10,    1), S(  -2,    6), S(   5,   -5), S(  14,    8),
    S(  -9,   -7), S(   1,    7), S(  16,   18), S(  21,   36), S(  34,   31), S(  27,   10), S(  29,    0), S(  23,    1),
    S(  11,   22), S(  23,   11), S(  30,   38), S(  35,   35), S(  40,   45), S(  53,   21), S(  58,   10), S(  39,   14),
    S(   8,   24), S(  18,   21), S(  35,   34), S(  45,   45), S(  38,   34), S(  74,   22), S(  28,   13), S(  42,    0),
    S( -14,    7), S(   5,   14), S(  -2,   47), S(  13,   46), S(  42,   22), S(  50,    0), S(  10,  -10), S( -12,  -18),
    S( -21,   -9), S(  -6,   15), S(  12,    6), S(  26,   13), S(  27,  -12), S(  56,  -22), S(  -4,    3), S(  -4,  -44),
    S(-195,  -78), S(-123,  -15), S(-142,   33), S( -61,   -4), S( -17,    3), S(-140,   12), S( -91,  -21), S(-144, -119)
};

constexpr Score BishopPSQT[N_SQUARES] = { S(  31,  -15), S(  43,    2), S(  35,    9), S(  16,   11), S(  39,    7), S(  19,   20), S(  41,   -1), S(  51,  -39),
    S(  33,   -6), S(  31,  -21), S(  34,  -11), S(  14,   16), S(  20,   14), S(  24,    7), S(  45,  -12), S(  41,  -32),
    S(  15,    0), S(  49,   19), S(  37,   24), S(  20,   32), S(  34,   36), S(  43,   30), S(  58,    7), S(  43,  -12),
    S(  15,    3), S(   5,   22), S(  25,   31), S(  41,   35), S(  28,   35), S(  40,   17), S(  15,   17), S(  49,  -16),
    S(  -3,   19), S(  26,   26), S(  27,   23), S(  41,   55), S(  41,   28), S(  25,   29), S(  34,   11), S(  -2,   16),
    S(   4,   26), S(  36,   18), S(  20,   22), S(  35,   14), S(  24,   20), S(  50,   21), S(  15,   21), S(  21,    9),
    S(   0,   -2), S(  -3,   18), S(   5,   15), S( -23,   28), S( -13,   11), S( -21,   21), S( -39,   21), S( -59,    8),
    S( -23,   32), S( -57,   31), S(-113,   48), S(-121,   49), S(-125,   44), S(-125,   32), S( -34,   15), S( -56,    4)
};

constexpr Score RookPSQT[N_SQUARES] = { S(   3,   35), S(   0,   30), S(   2,   33), S(  11,   14), S(  15,    7), S(  13,   18), S(  16,   17), S(   5,   16),
    S( -14,   26), S(  -9,   36), S(   8,   28), S(   7,   23), S(  15,   11), S(   1,   13), S(  29,   -2), S(  -6,   10),
    S( -18,   41), S( -12,   34), S(  -6,   29), S( -10,   27), S(   1,   19), S(   6,    4), S(  29,  -14), S(   9,   -5),
    S( -16,   54), S( -20,   59), S( -13,   53), S(  -8,   43), S( -11,   39), S( -22,   41), S(   4,   29), S( -10,   26),
    S( -10,   73), S(  11,   65), S(   9,   69), S(  -1,   57), S(  10,   36), S(  22,   32), S(  24,   39), S(  14,   35),
    S( -17,   69), S(  23,   62), S(   3,   68), S(   3,   55), S(  29,   39), S(  27,   36), S(  63,   30), S(  27,   28),
    S( -20,   66), S( -26,   85), S( -20,   96), S(  -6,   81), S( -28,   80), S( -13,   67), S(  -8,   62), S(  17,   42),
    S(   4,   60), S(  -4,   69), S( -19,   86), S( -29,   74), S( -12,   64), S(  -2,   69), S(   9,   69), S(  25,   57)
};

constexpr Score QueenPSQT[N_SQUARES] = { S(   2,  -16), S(  -7,   -8), S(   1,    1), S(  13,  -11), S(   9,  -10), S(  -3,  -22), S(  19,  -63), S(  21,  -47),
    S(  17,   -9), S(  21,   -7), S(  23,    8), S(  28,   34), S(  30,   26), S(  23,  -42), S(  50,  -92), S(  47,  -67),
    S(  15,   -5), S(  21,   32), S(  11,   58), S(   8,   58), S(  16,   62), S(  29,   41), S(  48,   15), S(  36,   13),
    S(  22,   24), S(   5,   69), S(   7,   75), S(   3,  106), S(   6,  107), S(  12,   87), S(  31,   77), S(  32,   74),
    S(   3,   54), S(  22,   65), S(   0,   93), S(  -8,  135), S(  -9,  160), S(  -1,  149), S(  25,  146), S(  14,  109),
    S(   9,   51), S(   5,   56), S(   1,  109), S( -11,  140), S( -11,  170), S(  13,  157), S(  22,  103), S(  22,  111),
    S(   2,   47), S( -30,   87), S( -36,  136), S( -87,  210), S(-102,  243), S( -49,  171), S( -35,  157), S(  20,  142),
    S( -60,   90), S( -46,   93), S( -23,  131), S(  -1,  131), S( -23,  143), S(   7,  125), S(   4,  102), S( -23,   95)
};

constexpr Score KingPSQT[N_SQUARES] = { S(  54,  -51), S(  87,  -13), S(  66,   21), S( -45,   39), S(  32,   12), S( -12,   40), S(  62,   -2), S(  51,  -44),
    S( 117,    2), S( 102,   36), S(  91,   52), S(  36,   72), S(  41,   76), S(  64,   63), S( 113,   32), S(  92,    6),
    S(  32,   17), S( 122,   47), S(  82,   79), S(  57,  101), S(  68,   97), S(  75,   85), S( 105,   55), S(  12,   45),
    S(  14,   29), S(  90,   68), S(  85,  110), S(  14,  148), S(  45,  139), S(  84,  113), S(  70,   82), S( -56,   70),
    S(  22,   48), S(  83,   95), S(  67,  140), S(  30,  174), S(  40,  176), S( 100,  150), S(  92,  117), S( -11,   81),
    S(  16,   61), S( 114,  115), S(  93,  141), S(  74,  172), S( 131,  181), S( 164,  164), S( 145,  145), S(  58,   76),
    S( -13,   43), S(  47,  116), S(  44,  131), S(  87,  124), S(  95,  130), S( 104,  154), S( 103,  151), S(  63,   82),
    S( -22,  -72), S(  21,    7), S(   9,   43), S(   2,   79), S(   4,   68), S(  31,   90), S(  54,  100), S(  21,  -59)
};

//----------------------------------------------------------
// Pions
constexpr Score PawnDoubled = S( -10,  -48);
constexpr Score PawnDoubled2 = S( -12,  -24);
constexpr Score PawnSupport = S(  21,   16);
constexpr Score PawnOpen = S( -14,  -21);
constexpr Score PawnPhalanx[N_RANKS] = { S(   0,    0), S(   8,   -2), S(  18,   15), S(  33,   37), S(  72,  121), S( 230,  372), S( 167,  406), S(   0,    0)
};
constexpr Score PawnIsolated = S(  -7,  -15);
constexpr Score PawnPassed[N_RANKS] = { S(   0,    0), S( -12,   22), S( -20,   36), S( -74,  110), S( -42,  139), S(  60,  160), S( 310,  196), S(   0,    0)
};
constexpr Score PassedDefended[N_RANKS] = { S(   0,    0), S(   0,    0), S(   4,  -14), S(   6,  -10), S(   5,   34), S(  37,  102), S( 158,   94), S(   0,    0)
};

//----------------------------------------------------------
// Pions passés
constexpr Score PassedSquare = S( -34,  422);
constexpr Score PassedDistUs[N_RANKS] = { S(   0,    0), S(   0,    0), S(   0,    0), S(  17,  -32), S(  17,  -43), S(   1,  -46), S( -17,  -35), S(   0,    0)
};
constexpr Score PassedDistThem = S(  -3,   19);
constexpr Score PassedBlocked[N_RANKS] = { S(   0,    0), S(   0,    0), S(   0,    0), S(   1,   -2), S(  -2,    6), S( -10,   -6), S( -72,  -38), S(   0,    0)
};
constexpr Score PassedFreeAdv[N_RANKS] = { S(   0,    0), S(   0,    0), S(   0,    0), S(  -4,   23), S( -14,   58), S( -23,  184), S( -81,  288), S(   0,    0)
};
constexpr Score PassedRookBack = S(  22,   47);

//----------------------------------------------------------
// Cavaliers
constexpr Score KnightBehindPawn = S(   3,   26);
constexpr Score KnightOutpost[2][2] = {   { S(  13,  -11), S(  46,   21) },
    { S(  16,   -7), S(  24,    5) }
};

// Fous
constexpr Score BishopPair = S(  30,  113);
constexpr Score BishopBehindPawn = S(  11,   23);
constexpr Score BishopBadPawn = S(  -1,   -7);

// Tours
constexpr Score OpenForward = S(  30,   32);
constexpr Score SemiForward = S(  17,   19);

//----------------------------------------------------------
// Roi
constexpr Score KingLineDanger[28] = { S(   0,    0), S(   0,    0), S(  28,  -28), S(  13,   19), S(  -8,   35), S( -16,   30), S( -19,   30), S( -29,   38),
    S( -41,   42), S( -56,   41), S( -59,   42), S( -77,   47), S( -79,   46), S( -92,   46), S(-105,   49), S(-109,   46),
    S(-120,   44), S(-132,   41), S(-136,   37), S(-145,   32), S(-157,   31), S(-185,   29), S(-179,   26), S(-201,   10),
    S(-169,  -12), S(-143,  -27), S(-130,  -32), S(-135,  -34)
};
constexpr Score KingAttackPawn = S(  -9,   37);
constexpr Score PawnShelter = S(  31,  -11);

//----------------------------------------------------------
// Menaces
constexpr Score PawnThreat = S(  79,   38);
constexpr Score PushThreat = S(  22,    5);
constexpr Score ThreatByMinor[N_PIECES] = { S(   0,    0), S(   0,    0), S(  31,   38), S(  38,   46), S(  76,   15), S(  63,   -5), S(   0,    0)
};
constexpr Score ThreatByRook[N_PIECES] = { S(   0,    0), S(   0,    0), S(  26,   41), S(  36,   51), S( -13,   51), S(  99,  -66), S(   0,    0)
};

//----------------------------------------------------------
// Mobilité
constexpr Score KnightMobility[9] = { S( -63, -125), S( -30,   -9), S(  -6,   52), S(   8,   70), S(  21,   82),
    S(  31,  101), S(  43,  105), S(  55,  109), S(  67,   91)
};

constexpr Score BishopMobility[14] = { S( -45,  -39), S( -15,   -8), S(   2,   35), S(  12,   62), S(  25,   72),
    S(  37,   87), S(  41,  100), S(  49,  103), S(  50,  109), S(  53,  111),
    S(  60,  105), S(  78,   95), S(  63,  104), S(  89,   74)
};

constexpr Score RookMobility[15] = { S( -99, -149), S(  -8,   39), S(   5,   83), S(  15,   85), S(  11,  119),
    S(  17,  128), S(  13,  142), S(  22,  143), S(  23,  150), S(  29,  154),
    S(  35,  161), S(  31,  172), S(  28,  180), S(  38,  184), S(  45,  182)
};

constexpr Score QueenMobility[28] = { S( -65,  -48), S(-101,  -56), S( -82, -104), S( -12, -116), S(  -6,  -17),
    S( -13,  113), S(  -6,  178), S(  -6,  217), S(  -3,  256), S(   3,  262),
    S(   5,  271), S(   8,  280), S(  14,  282), S(  14,  295), S(  15,  305),
    S(  17,  309), S(  14,  322), S(  13,  332), S(   4,  351), S(   2,  363),
    S(  15,  355), S(  15,  356), S(  49,  337), S(  65,  314), S( 135,  252),
    S( 152,  218), S( 162,  180), S( 132,  171)
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
