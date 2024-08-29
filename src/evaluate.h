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

enum PieceValueEMG {
    P_MG =  104, P_EG =  204,
    N_MG =  420, N_EG =  632,
    B_MG =  427, B_EG =  659,
    R_MG =  569, R_EG = 1111,
    Q_MG = 1485, Q_EG = 1963,
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
    S( -19,   12), S(  -9,   13), S( -11,    8), S( -11,   16), S(  -4,   31), S(  15,    9), S(  31,   -2), S(  -6,  -13),
    S( -34,    4), S( -35,   -2), S( -27,   -5), S( -24,   -7), S( -14,    0), S( -13,   -1), S(   2,  -16), S( -12,  -14),
    S( -27,   15), S( -32,   10), S( -17,   -8), S(  -8,  -18), S(  -2,  -15), S(   3,  -15), S( -15,   -5), S( -10,   -7),
    S( -15,   43), S( -13,   20), S(  -8,    2), S(  -1,  -22), S(  21,  -15), S(  29,  -14), S(   5,   13), S(   8,   18),
    S(  15,   77), S(  14,   84), S(  42,   43), S(  47,   17), S(  64,   21), S( 125,   22), S(  98,   68), S(  43,   76),
    S(  57,    1), S(  49,   22), S(  34,   65), S(  74,   31), S(  76,   36), S(  85,   27), S( -34,   91), S( -49,   63),
    S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0),
};

constexpr Score KnightPSQT[N_SQUARES] = {
    S( -82,  -53), S( -18,  -37), S( -30,   -9), S(  -1,   12), S(   5,   14), S(   5,  -16), S( -14,  -12), S( -45,  -30),
    S( -32,  -29), S( -30,    1), S( -13,   -5), S(   0,   16), S(   0,    9), S( -10,   -5), S( -15,  -10), S(  -1,    0),
    S( -13,  -31), S(  -5,    6), S(   9,   22), S(  12,   44), S(  24,   40), S(  17,   16), S(  21,    4), S(  11,   -8),
    S(  10,   12), S(  22,   14), S(  27,   52), S(  36,   49), S(  35,   54), S(  44,   41), S(  48,   11), S(  30,   14),
    S(  13,    6), S(  23,   23), S(  45,   50), S(  52,   62), S(  40,   57), S(  72,   39), S(  25,   17), S(  39,   -7),
    S( -15,   -7), S(  23,    7), S(  24,   46), S(  43,   45), S(  84,   20), S(  69,   23), S(  33,  -15), S(  -3,  -25),
    S(  -9,  -24), S(  -6,    3), S(  38,  -11), S(  46,   16), S(  52,   -2), S(  69,  -34), S( -12,    0), S(   8,  -47),
    S(-201,  -70), S(-116,  -15), S(-145,   31), S( -58,    1), S(  -7,    9), S(-133,   34), S( -83,  -15), S(-153, -115),
};

constexpr Score BishopPSQT[N_SQUARES] = {
    S(  26,  -16), S(  41,    4), S(  25,   17), S(   6,   15), S(  21,   14), S(  15,   13), S(  29,   -2), S(  39,  -27),
    S(  30,   -6), S(  24,  -23), S(  29,  -12), S(   5,   12), S(  14,   12), S(  16,   -3), S(  36,  -19), S(  35,  -44),
    S(   9,   -3), S(  43,   21), S(  28,   18), S(  18,   27), S(  27,   32), S(  33,   20), S(  47,    9), S(  37,   -9),
    S(  13,    0), S(  10,    5), S(  23,   23), S(  39,   29), S(  29,   25), S(  34,   11), S(  16,    7), S(  41,  -11),
    S(  -7,   16), S(  29,   14), S(  27,   15), S(  49,   40), S(  39,   25), S(  31,   19), S(  33,    7), S(  -5,   17),
    S(   2,   26), S(  28,   17), S(  43,   12), S(  32,    9), S(  37,   12), S(  52,   24), S(  19,   24), S(  19,   12),
    S( -14,   15), S(  10,   18), S(   1,   19), S( -25,   30), S(  -2,   13), S( -15,   25), S( -28,   27), S( -56,   23),
    S( -35,   47), S( -57,   38), S(-128,   54), S(-126,   56), S(-128,   52), S(-143,   43), S( -28,   21), S( -44,   18),
};

constexpr Score RookPSQT[N_SQUARES] = {
    S( -11,   38), S( -13,   35), S( -11,   39), S(  -1,   21), S(   4,   14), S(   2,   21), S(   5,   20), S( -11,   20),
    S( -35,   33), S( -16,   31), S(  -5,   33), S(  -4,   25), S(   2,   17), S( -14,   11), S(   5,    4), S( -31,   21),
    S( -25,   32), S( -23,   36), S( -21,   35), S( -18,   30), S( -13,   26), S( -11,   11), S(  14,   -3), S(  -8,    5),
    S( -22,   51), S( -26,   58), S( -23,   55), S( -16,   47), S( -18,   41), S( -30,   38), S(  -1,   29), S( -15,   26),
    S( -13,   67), S(   8,   59), S(  14,   59), S(  32,   37), S(  27,   31), S(  33,   25), S(  35,   27), S(  16,   34),
    S( -13,   62), S(  39,   47), S(  15,   58), S(  39,   35), S(  57,   24), S(  52,   32), S(  81,   17), S(  23,   35),
    S( -13,   63), S( -26,   81), S(  -1,   83), S(   7,   76), S(  -4,   72), S(   8,   52), S(  -3,   56), S(  19,   42),
    S(  24,   56), S(  24,   67), S( -12,   88), S(  -8,   76), S(   3,   75), S(   7,   77), S(  25,   73), S(  40,   64),
};

constexpr Score QueenPSQT[N_SQUARES] = {
    S(  -2,  -39), S(  -9,  -38), S(   1,  -31), S(  11,  -35), S(   7,  -37), S(  -9,  -44), S(   7,  -77), S(  14,  -55),
    S(  13,  -31), S(  16,  -22), S(  22,  -26), S(  23,    8), S(  26,   -1), S(  22,  -83), S(  40, -115), S(  33,  -73),
    S(  10,  -21), S(  18,   10), S(   7,   37), S(   7,   32), S(  13,   35), S(  18,   32), S(  43,    1), S(  31,   -7),
    S(  20,    5), S(   5,   50), S(   5,   57), S(   0,   99), S(  -1,  104), S(   6,   82), S(  26,   70), S(  29,   69),
    S(   6,   25), S(  13,   65), S(  -1,   79), S( -11,  130), S( -14,  162), S(  -9,  171), S(  27,  155), S(   9,  123),
    S(  -3,   50), S(   5,   40), S(   1,   89), S( -10,  128), S(  -3,  159), S(  29,  157), S(  49,  114), S(  13,  140),
    S(  -9,   54), S( -48,  103), S( -28,  113), S( -84,  202), S( -88,  236), S( -28,  170), S( -43,  163), S(  11,  146),
    S( -42,   81), S( -21,   93), S( -12,  125), S(   4,  130), S(  -3,  148), S(  35,  138), S(  33,  125), S(  18,  115),
};

constexpr Score KingPSQT[N_SQUARES] = {
    S(  37,  -41), S(  89,   -1), S(  64,   30), S( -39,   43), S(  33,   17), S( -18,   50), S(  66,    3), S(  36,  -41),
    S(  85,   25), S(  84,   50), S(  72,   66), S(  19,   86), S(  31,   86), S(  47,   75), S(  84,   50), S(  60,   25),
    S(  32,   25), S( 117,   55), S(  86,   83), S(  63,  103), S(  81,   99), S(  69,   90), S(  86,   67), S(  -3,   52),
    S(  25,   31), S( 114,   68), S( 119,  107), S(  36,  146), S(  79,  134), S( 114,  108), S( 111,   74), S( -41,   68),
    S(  33,   59), S( 102,   93), S(  95,  135), S(  58,  164), S(  71,  164), S( 123,  139), S( 114,  111), S(   0,   79),
    S(  31,   63), S( 129,  112), S( 114,  133), S(  91,  148), S( 133,  144), S( 165,  148), S( 150,  133), S(  49,   72),
    S(  -7,   34), S(  52,  112), S(  53,  118), S(  81,  107), S(  85,  108), S(  94,  125), S(  98,  139), S(  47,   54),
    S( -24,  -68), S(  23,   15), S(  10,   45), S(   7,   82), S(   1,   62), S(  26,   76), S(  45,   91), S(  16,  -57),
};

//----------------------------------------------------------
// Pions
constexpr Score PawnDoubled  = S(-11,-48);
constexpr Score PawnDoubled2 = S(-10,-25);
constexpr Score PawnSupport  = S( 22, 17);
constexpr Score PawnOpen     = S(-14,-19);
constexpr Score PawnPhalanx[N_RANKS] = {
    S(  0,  0), S( 10, -2), S( 20, 14), S( 35, 37),
    S( 72,121), S(231,367), S(168,394), S(  0,  0)
};
constexpr Score PawnIsolated = S( -8,-16);

//----------------------------------------------------------
// Pions passés
constexpr Score PawnPassed[N_RANKS] = {
    S(  0,  0), S(-14, 25), S(-19, 40), S(-72,115),
    S(-38,146), S( 60,175), S(311,218), S(  0,  0)
};
constexpr Score PassedDefended[N_RANKS] = {
    S(  0,  0), S(  0,  0), S(  3,-14), S(  3,-10),
    S(  0, 33), S( 32,103), S(158, 96), S(  0,  0)
};
constexpr Score PassedBlocked[4] = {
    S(  1, -4), S( -6,  6), S(-11,-11), S(-54,-52)
};
constexpr Score PassedFreeAdv[4] = {
    S( -4, 25), S(-12, 58), S(-23,181), S(-70,277)
};
constexpr Score PassedDistUs[4] = {
    S( 18,-32), S( 17,-44), S(  2,-49), S(-19,-37)
};
constexpr Score PassedDistThem = S( -4, 19);
constexpr Score PassedRookBack = S( 21, 46);
constexpr Score PassedSquare   = S(-26,422);

//----------------------------------------------------------
// Bonus divers
constexpr Score BishopPair       = S( 33,110);
constexpr Score BishopBadPawn    = S(  -1,   -5);
constexpr Score MinorBehindPawn  = S(  9, 32);
constexpr Score KnightOutpost[2] = { S(  19,  -28), S(  41,   17) };
constexpr Score KingAttackPawn   = S(-16, 45);
constexpr Score OpenForward      = S( 28, 31);
constexpr Score SemiForward      = S( 17, 15);
constexpr Score PawnShelter      = S( 31,-12);



//----------------------------------------------------------
// Mobilité :
constexpr Score KnightMobility[9] = {
    S( -44, -139), S( -31,   -7), S( -10,   60), S(   0,   86), S(  13,   89), S(  22,  102), S(  32,  102), S(  43,  101),
    S(  54,   81),
};

constexpr Score BishopMobility[14] = {
    S( -51,  -81), S( -26,  -40), S( -11,   19), S(  -3,   53), S(   9,   65), S(  21,   83), S(  26,   98), S(  32,  104),
    S(  32,  114), S(  35,  116), S(  41,  115), S(  57,  106), S(  50,  115), S( 100,   76),
};

constexpr Score RookMobility[15] = {
    S(-105, -146), S( -15,   18), S(  -1,   82), S(   5,   88), S(   2,  121), S(   6,  133), S(   5,  144), S(  12,  146),
    S(  14,  152), S(  19,  157), S(  24,  164), S(  23,  171), S(  25,  177), S(  36,  177), S(  72,  154),
};

constexpr Score QueenMobility[28] = {
    S( -63,  -48), S( -97,  -54), S( -89, -107), S( -17, -127), S(   0,  -52), S(  -8,   72), S(  -2,  142), S(  -2,  184),
    S(   1,  215), S(   5,  230), S(   7,  243), S(   9,  254), S(  15,  255), S(  15,  268), S(  16,  279), S(  18,  283),
    S(  16,  294), S(  16,  302), S(  12,  313), S(  13,  321), S(  23,  314), S(  22,  318), S(  48,  298), S(  58,  279),
    S( 122,  221), S( 135,  193), S( 146,  166), S( 125,  162),
};

//----------------------------------------------------------
// Menaces
constexpr Score PawnThreat   = S( 80, 34);
constexpr Score PushThreat   = S( 25,  6);
constexpr Score ThreatByMinor[8] = {
    S(  0,  0), S(  0,  0), S( 27, 42), S( 39, 42),
    S( 67, 11), S( 59,-17), S(  0,  0), S(  0,  0)
};
constexpr Score ThreatByRook[8] = {
    S(  0,  0), S(  0,  0), S( 24, 43), S( 31, 45),
    S(-17, 47), S( 93,-73), S(  0,  0), S(  0,  0)
};

// Sécurité du roi
constexpr Score KingLineDanger[28] = {
    S(  0,  0), S(  0,  0), S( 15,  0), S( 11, 21),
    S(-16, 35), S(-25, 30), S(-29, 29), S(-37, 38),
    S(-48, 41), S(-67, 43), S(-67, 40), S(-80, 43),
    S(-85, 43), S(-97, 44), S(-109, 46), S(-106, 41),
    S(-116, 41), S(-123, 37), S(-126, 34), S(-131, 29),
    S(-138, 28), S(-155, 26), S(-149, 23), S(-172,  9),
    S(-148, -8), S(-134,-26), S(-130,-32), S(-135,-34)
};

constexpr Score AttackPower[7]   = { 0, 0, 36, 22, 23, 78, 0 };
constexpr Score CheckPower[7]    = { 0, 0, 68, 44, 88, 92, 0 };
constexpr Score CountModifier[8] = { 0, 0, 63, 126, 96, 124, 124, 128 };

//============================================================== FIN TUNER

//  Sécurité du Roi

//------------------------------------------------------------
//  Bonus car on a le trait
constexpr int Tempo = 18;


#endif // EVALUATE_H
