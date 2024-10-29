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

    // kingRing[color] are the squares adjacent to the king plus some other
    // very near squares, depending on king position.
    Bitboard KingRing[N_COLORS];

    // kingAttackersCount[color] is the number of pieces of the given color
    // which attack a square in the kingRing of the enemy king.
    // int kingAttackersCount[N_COLORS];

    // kingAttackersWeight[color] is the sum of the "weights" of the pieces of
    // the given color which attack a square in the kingRing of the enemy king.
    // The weights of the individual piece types are given by the elements in
    // the KingAttackWeights array.
    Score KingAttackersWeight[N_COLORS];

    // kingAttacksCount[color] is the number of attacks by the given color to
    // squares directly adjacent to the enemy king. Pieces which attack more
    // than one square are counted multiple times. For instance, if there is
    // a white knight on g5 and black's king is on g8, this white knight adds 2
    // to kingAttacksCount[WHITE].
    int KingAttacksCount[N_COLORS];


    // attackedBy[color][piece type] is a bitboard representing all squares
    // attacked by a given color and piece type.
    Bitboard attackedBy[N_COLORS][N_PIECES];

    // attacked[color] is a bitboard representing all squares
    // attacked by a given color for all pieces type.
    Bitboard attacked[N_COLORS];


    // attackedBy2[color] are the squares attacked by at least 2 units of a given
    // color, including x-rays. But diagonal x-rays through pawns are not computed.
    Bitboard attackedBy2[N_COLORS];

    int phase24;

};

//----------------------------------------------------------
//  Valeur des pièces
enum PieceValueEMG {
    P_MG =  103, P_EG =  223,
    N_MG =  422, N_EG =  685,
    B_MG =  419, B_EG =  702,
    R_MG =  547, R_EG = 1200,
    Q_MG = 1396, Q_EG = 2106,
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
    S( -19,   14), S(  -9,   14), S(  -1,    7), S(  -7,   10), S(   4,   25), S(  18,   10), S(  36,   -4), S( -18,  -10),
    S( -37,    5), S( -34,   -4), S( -20,   -9), S( -23,   -5), S(  -9,   -2), S( -21,    2), S(  -7,  -16), S( -27,  -11),
    S( -28,   18), S( -30,   10), S( -11,  -11), S(  -1,  -20), S(  12,  -19), S( -11,  -10), S( -25,   -1), S( -24,   -4),
    S( -17,   47), S( -11,   20), S(  -2,    2), S(  -6,  -17), S(  17,  -13), S(  16,   -8), S(  -4,   16), S(  -8,   24),
    S(  31,   62), S(  15,   77), S(  49,   34), S(  56,   11), S(  67,   16), S(  98,   28), S(  74,   76), S(  24,   84),
    S(  60,   -2), S(  65,   -3), S(  45,   60), S( 102,   14), S(  73,   40), S(  65,   33), S( -38,   79), S( -82,   88),
    S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0)
};

constexpr Score KnightPSQT[N_SQUARES] = { S( -66,  -18), S( -16,  -19), S( -23,   -3), S(   3,    9), S(  13,    6), S(  10,  -23), S( -10,  -10), S( -13,   -8),
    S( -31,  -11), S( -17,   -1), S( -10,   -6), S(   6,    2), S(   9,   -6), S(   6,   -9), S(  10,  -23), S(   9,    2),
    S( -13,   -7), S(  -6,    1), S(   9,    8), S(  13,   30), S(  26,   23), S(  22,   -1), S(  24,  -10), S(  17,   -2),
    S(  14,   35), S(  18,   15), S(  28,   41), S(  27,   38), S(  27,   57), S(  40,   31), S(  43,   23), S(  29,   34),
    S(   6,   39), S(  11,   22), S(  25,   36), S(  29,   49), S(  35,   43), S(  59,   28), S(  43,   18), S(  50,   16),
    S( -12,   17), S( -12,   25), S(  -5,   43), S(   8,   47), S(  44,   33), S(  70,  -12), S(  14,    3), S(  18,   -7),
    S( -31,   -2), S( -13,   19), S(  -7,   21), S(  18,   25), S(  19,    5), S(  64,   -9), S(  10,    4), S(  20,  -40),
    S(-176,  -88), S(-134,  -12), S(-132,   26), S( -49,   -6), S( -10,    7), S(-109,   -2), S( -98,  -23), S(-120, -128)
};

constexpr Score BishopPSQT[N_SQUARES] = { S(  11,  -25), S(  40,    5), S(  27,   18), S(  12,   15), S(  34,   11), S(  15,   24), S(  43,  -10), S(  32,  -59),
    S(  27,    2), S(  12,  -30), S(  32,   -8), S(   9,   21), S(  22,   17), S(  34,    3), S(  35,  -32), S(  35,  -26),
    S(  16,   13), S(  42,   26), S(  12,   10), S(  16,   33), S(  27,   39), S(  22,    8), S(  52,   13), S(  37,   -4),
    S(  20,   17), S(  14,   34), S(  26,   42), S(  45,   43), S(  33,   40), S(  32,   38), S(  21,   29), S(  42,    5),
    S(  -4,   28), S(  27,   39), S(  34,   33), S(  36,   64), S(  47,   40), S(  31,   41), S(  27,   38), S(   0,   25),
    S(  10,   33), S(  33,   25), S(  -4,   13), S(  45,   23), S(  31,   34), S(  55,   11), S(  34,   23), S(  48,   21),
    S(   7,   -1), S( -30,    7), S(   9,   22), S( -16,   38), S(  15,   20), S(   8,   26), S( -46,   10), S(   1,   -5),
    S( -42,   10), S( -52,   37), S( -85,   45), S(-110,   54), S(-108,   49), S( -79,   33), S( -18,   25), S( -84,  -10)
};

constexpr Score RookPSQT[N_SQUARES] = { S(  -1,   26), S(  -5,   25), S(  -2,   29), S(   8,   12), S(  15,    2), S(  15,   10), S(  11,   14), S(   1,    9),
    S( -19,   22), S( -14,   35), S(   2,   30), S(   1,   28), S(  10,   16), S(  11,   10), S(  24,   -3), S( -10,    6),
    S( -23,   45), S( -17,   38), S( -12,   33), S( -17,   36), S(  -1,   27), S(   8,   16), S(  31,   -3), S(  13,   -3),
    S( -18,   64), S( -20,   71), S( -11,   61), S( -17,   57), S(  -4,   52), S( -11,   58), S(  16,   45), S(  -6,   39),
    S( -23,   84), S(  -2,   76), S(  -4,   81), S( -12,   69), S(   1,   49), S(  16,   47), S(  12,   58), S(   1,   50),
    S( -31,   82), S(   3,   75), S( -12,   80), S( -12,   70), S(  20,   55), S(  13,   48), S(  62,   41), S(  11,   44),
    S( -17,   69), S( -24,   83), S( -17,   91), S(  11,   64), S( -16,   69), S(  -3,   69), S(   5,   66), S(  33,   47),
    S(  -3,   71), S( -21,   86), S( -33,  101), S( -40,   90), S( -21,   82), S(   6,   82), S(  17,   87), S(  40,   65)
};

constexpr Score QueenPSQT[N_SQUARES] = { S( -27,   29), S( -35,   39), S( -29,   54), S( -19,   50), S( -23,   38), S( -32,   15), S(   0,  -40), S(   6,  -34),
    S(  -1,   21), S(  -3,   32), S(   2,   39), S(   6,   66), S(   6,   63), S(  12,    1), S(  31,  -60), S(  36,  -56),
    S(  11,   18), S(   3,   71), S(  -8,  101), S( -12,   95), S(  -2,  100), S(  21,   67), S(  34,   34), S(  27,   16),
    S(  21,   50), S(   5,   97), S(   1,  112), S(   0,  119), S(   6,  114), S(  24,   94), S(  34,   76), S(  32,   73),
    S(  -3,   78), S(  12,   88), S(   9,   97), S(   9,  112), S(  13,  115), S(  20,   94), S(  25,  130), S(  22,   77),
    S(  20,   50), S(  15,   60), S(  13,  104), S(  15,  113), S(  30,  118), S(  23,   99), S(  41,   57), S(  29,   59),
    S(   6,   38), S(  -4,   46), S( -10,  113), S( -51,  165), S( -55,  193), S(   5,  113), S(  -7,  108), S(  43,  105),
    S( -42,   80), S( -53,   86), S( -29,  109), S(   5,   97), S( -16,  109), S(  -8,  115), S(  14,   76), S(  -5,   88)
};

constexpr Score KingPSQT[N_SQUARES] = { S(  97,  -61), S( 124,  -14), S(  95,   28), S( -26,   51), S(  49,   24), S(   8,   52), S(  93,    3), S( 102,  -64),
    S( 155,   -8), S( 115,   35), S(  88,   55), S(  30,   75), S(  31,   82), S(  65,   67), S( 134,   30), S( 127,    1),
    S(  51,   18), S( 109,   48), S(  51,   83), S(  26,  102), S(  34,  102), S(  60,   86), S(  86,   60), S(  27,   45),
    S(  12,   37), S(  57,   73), S(  32,  111), S( -34,  142), S( -11,  137), S(  33,  118), S(  31,   94), S( -65,   82),
    S(  23,   52), S(  55,   93), S(  21,  136), S( -15,  157), S(  -7,  161), S(  48,  148), S(  55,  121), S( -34,   92),
    S(   4,   66), S(  95,  113), S(  64,  136), S(  41,  161), S( 100,  169), S( 140,  152), S( 116,  143), S(  62,   80),
    S( -20,   48), S(  38,  111), S(  26,  130), S(  91,  122), S(  91,  125), S(  87,  157), S(  88,  146), S(  63,   91),
    S( -14,  -73), S(  18,   -4), S(  10,   43), S(   4,   78), S(   6,   70), S(  20,   91), S(  48,   93), S(  26,  -64)
};

//----------------------------------------------------------
// Pions
constexpr Score PawnDoubled = S( -11,  -51);
constexpr Score PawnDoubled2 = S(  -8,  -27);
constexpr Score PawnSupport = S(  21,   15);
constexpr Score PawnOpen = S( -13,  -19);
constexpr Score PawnPhalanx[N_RANKS] = { S(   0,    0), S(   9,   -6), S(  19,   13), S(  32,   34), S(  64,  120), S( 206,  382), S( 165,  420), S(   0,    0)
};
constexpr Score PawnIsolated = S(  -6,  -15);
constexpr Score PawnPassed[N_RANKS] = { S(   0,    0), S(  -7,   20), S( -18,   38), S( -76,  112), S( -49,  145), S(  51,  168), S( 299,  190), S(   0,    0)
};
constexpr Score PassedDefended[N_RANKS] = { S(   0,    0), S(   0,    0), S(   5,  -16), S(   6,  -10), S(   4,   33), S(  34,  101), S( 156,  103), S(   0,    0)
};

//----------------------------------------------------------
// Pions passés
constexpr Score PassedSquare = S( -44,  440);
constexpr Score PassedDistUs[N_RANKS] = { S(   0,    0), S(   0,    0), S(   0,    0), S(  19,  -33), S(  21,  -45), S(   5,  -48), S( -18,  -33), S(   0,    0)
};
constexpr Score PassedDistThem = S(  -4,   21);
constexpr Score PassedBlocked[N_RANKS] = { S(   0,    0), S(   0,    0), S(   0,    0), S(   1,    2), S( -10,   11), S( -23,    1), S( -78,  -20), S(   0,    0)
};
constexpr Score PassedFreeAdv[N_RANKS] = { S(   0,    0), S(   0,    0), S(   0,    0), S( -16,   28), S( -28,   63), S( -51,  196), S( -90,  315), S(   0,    0)
};
constexpr Score PassedRookBack = S(  19,   48);

//----------------------------------------------------------
// Cavaliers
constexpr Score KnightBehindPawn = S(   3,   28);
constexpr Score KnightOutpost[2][2] = {   { S(  12,  -16), S(  43,   14) },
    { S(   9,  -21), S(  14,  -14) }
};

// Fous
constexpr Score BishopPair = S(  29,  109);
constexpr Score BishopBadPawn = S(   0,   -6);
constexpr Score BishopBehindPawn = S(  11,   23);
constexpr Score BishopLongDiagonal = S(  33,   23);

// Tours
constexpr Score RookOnOpenFile[2] = { S(   9,   14), S(  31,   13)
};
constexpr Score RookOnBlockedFile = S(  -6,   -5);

// Dames
constexpr Score QueenRelativePin = S( -33,   24);

//----------------------------------------------------------
// Roi
constexpr Score KingAttackPawn = S(  13,   14);
constexpr Score PawnShelter = S(  35,  -14);

//----------------------------------------------------------
// Menaces
constexpr Score ThreatByKnight[N_PIECES] = { S(   0,    0), S(  -1,   22), S(  -2,  104), S(  36,   40), S(  89,   -7), S(  68,  -85), S(   0,    0)
};
constexpr Score ThreatByBishop[N_PIECES] = { S(   0,    0), S(   3,   21), S(  26,   37), S(   0,    0), S(  66,   11), S(  54,   -5), S( 123, 2990)
};
constexpr Score ThreatByRook[N_PIECES] = { S(   0,    0), S(   5,   27), S(  39,   49), S(  45,   54), S(  10,   18), S(  59,  -92), S( 322, 2161)
};
constexpr Score ThreatByKing = S(   1,   31);
constexpr Score HangingThreat = S(   1,   13);
constexpr Score PawnThreat = S(  83,   34);
constexpr Score PawnPushThreat = S(  25,   30);
constexpr Score PawnPushPinnedThreat = S(  -4,  163);
constexpr Score KnightCheckQueen = S(  16,    3);
constexpr Score BishopCheckQueen = S(  22,   31);
constexpr Score RookCheckQueen = S(  22,   16);

//----------------------------------------------------------
// Attaques sur le roi ennemi
constexpr Score SafeCheck[N_PIECES] = { S(   0,    0), S(   0,    0), S( 129,   -3), S(  36,   23), S( 105,    3), S(  35,   43), S(   0,    0)
};
constexpr Score UnsafeCheck[N_PIECES] = { S(   0,    0), S(   0,    0), S(  11,    1), S(  20,   21), S(  41,   -1), S(  11,   19), S(   0,    0)
};
constexpr Score KingAttackersWeight[N_PIECES] = { S(   0,    0), S(   0,    0), S(  30,    4), S(  23,    5), S(  31,  -25), S(   6,   14), S(   0,    0)
};
constexpr Score KingAttacksCount[14] = { S( -45,   31), S( -62,   24), S( -74,   15), S( -81,   22), S( -82,   20), S( -73,   14), S( -50,    4), S( -19,  -11),
    S(  37,  -42), S(  70,  -37), S( 106,  -36), S( 131,   12), S( 160,  -81), S( 116,   74)
};

//----------------------------------------------------------
// Mobilité
constexpr Score KnightMobility[9] = { S( -81, -113), S( -46,   -6), S( -20,   56), S(  -6,   76), S(   8,   92),
    S(  18,  111), S(  28,  116), S(  40,  121), S(  51,  107)
};

constexpr Score BishopMobility[14] = { S( -49,  -22), S( -20,    2), S(  -5,   46), S(   5,   73), S(  16,   85),
    S(  25,   96), S(  28,  110), S(  36,  114), S(  37,  121), S(  42,  121),
    S(  50,  118), S(  70,  108), S(  73,  115), S(  98,   90)
};

constexpr Score RookMobility[15] = { S( -91, -150), S( -19,   54), S(  -6,  102), S(   6,  102), S(   3,  133),
    S(   9,  143), S(   4,  159), S(  13,  162), S(  13,  171), S(  19,  176),
    S(  25,  185), S(  24,  197), S(  27,  204), S(  36,  211), S(  47,  205)
};

constexpr Score QueenMobility[28] = { S( -66,  -49), S(-104,  -57), S( -83, -104), S( -20, -102), S( -23,   28),
    S( -32,  166), S( -24,  233), S( -24,  275), S( -20,  310), S( -13,  309),
    S(  -8,  316), S(  -4,  321), S(   4,  317), S(   7,  323), S(  10,  328),
    S(  15,  324), S(  15,  330), S(  20,  326), S(  18,  333), S(  23,  329),
    S(  34,  307), S(  40,  291), S(  59,  280), S(  64,  248), S( 102,  210),
    S( 118,  169), S( 147,  159), S( 109,  150)
};





//============================================================== FIN TUNER


//------------------------------------------------------------
//  Bonus car on a le trait
constexpr int Tempo = 18;


#endif // EVALUATE_H
