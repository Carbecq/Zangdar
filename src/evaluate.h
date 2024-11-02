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
    P_MG =  102, P_EG =  221,
    N_MG =  412, N_EG =  676,
    B_MG =  427, B_EG =  701,
    R_MG =  551, R_EG = 1192,
    Q_MG = 1404, Q_EG = 2101,
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
    S( -18,   16), S(  -8,   16), S(  -1,    9), S(  -7,   12), S(   4,   27), S(  18,   12), S(  37,   -2), S( -17,   -8),
    S( -36,    7), S( -34,   -2), S( -19,   -7), S( -22,   -3), S(  -8,    0), S( -21,    4), S(  -6,  -14), S( -27,   -9),
    S( -27,   20), S( -30,   12), S( -10,   -9), S(  -1,  -18), S(  13,  -17), S( -10,   -8), S( -25,    1), S( -24,   -2),
    S( -17,   49), S( -10,   22), S(  -1,    4), S(  -6,  -15), S(  18,  -11), S(  16,   -6), S(  -4,   18), S(  -7,   26),
    S(  31,   64), S(  16,   79), S(  49,   36), S(  56,   12), S(  67,   18), S(  98,   30), S(  75,   77), S(  25,   85),
    S(  59,   -2), S(  63,   -1), S(  44,   61), S( 101,   15), S(  72,   41), S(  66,   31), S( -40,   80), S( -82,   88),
    S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0)
};

constexpr Score KnightPSQT[N_SQUARES] = { S( -64,  -18), S( -14,  -17), S( -21,   -2), S(   5,   10), S(  15,    7), S(  12,  -21), S(  -8,   -8), S( -10,   -7),
    S( -28,  -10), S( -15,    1), S(  -8,   -4), S(   9,    4), S(  11,   -4), S(   9,   -7), S(  13,  -21), S(  11,    3),
    S( -11,   -6), S(  -3,    3), S(  11,    9), S(  15,   32), S(  29,   25), S(  24,    0), S(  26,   -8), S(  19,   -1),
    S(  16,   36), S(  21,   16), S(  30,   43), S(  30,   39), S(  29,   58), S(  42,   32), S(  45,   24), S(  31,   35),
    S(   9,   39), S(  13,   23), S(  27,   37), S(  32,   50), S(  37,   44), S(  62,   29), S(  46,   19), S(  53,   15),
    S(  -9,   16), S( -10,   27), S(  -3,   44), S(  11,   48), S(  46,   35), S(  72,  -10), S(  17,    4), S(  21,   -7),
    S( -29,   -1), S( -11,   20), S(  -4,   21), S(  21,   27), S(  22,    7), S(  66,   -7), S(  11,    6), S(  22,  -38),
    S(-175,  -85), S(-130,  -12), S(-132,   29), S( -48,   -4), S( -10,    9), S(-111,    3), S( -96,  -21), S(-120, -125)
};

constexpr Score BishopPSQT[N_SQUARES] = { S(  10,  -25), S(  39,    5), S(  27,   18), S(  11,   15), S(  33,   11), S(  14,   23), S(  42,  -10), S(  31,  -59),
    S(  26,    2), S(  12,  -30), S(  31,   -8), S(   9,   21), S(  22,   17), S(  33,    3), S(  35,  -32), S(  35,  -26),
    S(  16,   13), S(  42,   26), S(  12,   10), S(  16,   33), S(  26,   39), S(  21,    8), S(  52,   13), S(  37,   -4),
    S(  20,   17), S(  13,   34), S(  25,   41), S(  44,   43), S(  32,   40), S(  31,   38), S(  20,   29), S(  41,    5),
    S(  -4,   28), S(  26,   38), S(  34,   33), S(  36,   64), S(  46,   39), S(  31,   41), S(  27,   38), S(   0,   25),
    S(   9,   33), S(  33,   24), S(  -5,   12), S(  45,   23), S(  31,   33), S(  55,   11), S(  34,   22), S(  48,   21),
    S(   7,   -1), S( -31,    6), S(   8,   22), S( -16,   38), S(  14,   21), S(   8,   26), S( -47,   10), S(   0,   -5),
    S( -43,   10), S( -53,   37), S( -88,   46), S(-111,   54), S(-109,   48), S( -84,   35), S( -20,   25), S( -84,  -10)
};

constexpr Score RookPSQT[N_SQUARES] = { S(  -2,   28), S(  -6,   27), S(  -3,   30), S(   7,   14), S(  15,    4), S(  14,   11), S(  10,   15), S(   0,   10),
    S( -19,   24), S( -15,   37), S(   1,   31), S(   0,   29), S(  10,   17), S(  11,   11), S(  23,   -2), S( -11,    8),
    S( -24,   46), S( -18,   39), S( -13,   35), S( -18,   37), S(  -2,   28), S(   7,   17), S(  31,   -2), S(  13,   -1),
    S( -19,   65), S( -21,   72), S( -12,   62), S( -17,   58), S(  -5,   54), S( -11,   59), S(  16,   47), S(  -7,   41),
    S( -24,   85), S(  -2,   77), S(  -4,   82), S( -13,   70), S(   0,   50), S(  16,   48), S(  12,   59), S(   0,   51),
    S( -32,   83), S(   3,   76), S( -12,   81), S( -12,   71), S(  20,   56), S(  13,   49), S(  63,   41), S(  11,   45),
    S( -18,   70), S( -24,   84), S( -17,   91), S(  11,   65), S( -16,   69), S(  -3,   70), S(   5,   67), S(  32,   48),
    S(  -3,   72), S( -19,   86), S( -32,  102), S( -40,   91), S( -21,   83), S(   7,   83), S(  19,   87), S(  41,   66)
};

constexpr Score QueenPSQT[N_SQUARES] = { S( -27,   28), S( -34,   35), S( -28,   50), S( -18,   47), S( -22,   36), S( -31,   11), S(   2,  -43), S(   6,  -34),
    S(   0,   20), S(  -2,   29), S(   3,   36), S(   8,   63), S(   7,   60), S(  13,   -2), S(  33,  -64), S(  37,  -57),
    S(  11,   16), S(   4,   68), S(  -7,   98), S( -11,   92), S(  -1,   97), S(  22,   65), S(  35,   32), S(  27,   16),
    S(  22,   48), S(   6,   94), S(   2,  109), S(   1,  117), S(   7,  112), S(  25,   92), S(  35,   74), S(  32,   71),
    S(  -2,   77), S(  13,   86), S(  10,   96), S(   9,  110), S(  13,  113), S(  19,   95), S(  26,  129), S(  22,   76),
    S(  21,   49), S(  16,   58), S(  14,  102), S(  16,  112), S(  30,  118), S(  21,  102), S(  40,   60), S(  29,   59),
    S(   6,   38), S(  -4,   46), S( -10,  113), S( -51,  165), S( -56,  193), S(   4,  115), S(  -8,  111), S(  43,  104),
    S( -43,   79), S( -52,   84), S( -30,  109), S(   4,   97), S( -17,  110), S(  -7,  114), S(  12,   78), S(  -5,   87)
};

constexpr Score KingPSQT[N_SQUARES] = { S(  95,  -60), S( 122,  -13), S(  93,   28), S( -28,   52), S(  47,   24), S(   6,   52), S(  90,    4), S( 100,  -64),
    S( 153,   -8), S( 113,   36), S(  86,   56), S(  28,   76), S(  29,   83), S(  63,   68), S( 131,   31), S( 125,    2),
    S(  49,   18), S( 108,   48), S(  49,   83), S(  25,  102), S(  33,  102), S(  58,   86), S(  84,   61), S(  25,   46),
    S(  11,   38), S(  60,   72), S(  35,  110), S( -33,  142), S( -10,  136), S(  35,  118), S(  33,   93), S( -66,   82),
    S(  22,   52), S(  58,   93), S(  25,  135), S( -15,  157), S(  -7,  160), S(  52,  147), S(  58,  120), S( -34,   92),
    S(   4,   66), S(  97,  112), S(  68,  135), S(  41,  161), S( 100,  169), S( 142,  152), S( 119,  141), S(  61,   80),
    S( -19,   48), S(  39,  110), S(  29,  129), S(  91,  122), S(  91,  125), S(  89,  156), S(  89,  145), S(  64,   91),
    S( -15,  -74), S(  18,   -2), S(  10,   43), S(   4,   78), S(   6,   69), S(  21,   90), S(  49,   94), S(  26,  -63)
};

//----------------------------------------------------------
// Pions
constexpr Score PawnDoubled = S( -11,  -50);
constexpr Score PawnDoubled2 = S(  -8,  -27);
constexpr Score PawnSupport = S(  21,   15);
constexpr Score PawnOpen = S( -13,  -19);
constexpr Score PawnPhalanx[N_RANKS] = { S(   0,    0), S(   9,   -6), S(  19,   13), S(  32,   34), S(  64,  120), S( 209,  381), S( 165,  419), S(   0,    0)
};
constexpr Score PawnIsolated = S(  -6,  -15);
constexpr Score PawnPassed[N_RANKS] = { S(   0,    0), S(  -7,   20), S( -18,   38), S( -77,  113), S( -50,  145), S(  50,  169), S( 299,  192), S(   0,    0)
};
constexpr Score PassedDefended[N_RANKS] = { S(   0,    0), S(   0,    0), S(   5,  -16), S(   6,  -10), S(   4,   33), S(  35,  101), S( 155,  103), S(   0,    0)
};

//----------------------------------------------------------
// Pions passés
constexpr Score PassedSquare = S( -43,  438);
constexpr Score PassedDistUs[N_RANKS] = { S(   0,    0), S(   0,    0), S(   0,    0), S(  19,  -33), S(  21,  -45), S(   5,  -48), S( -18,  -33), S(   0,    0)
};
constexpr Score PassedDistThem = S(  -4,   21);
constexpr Score PassedBlocked[N_RANKS] = { S(   0,    0), S(   0,    0), S(   0,    0), S(   1,    2), S( -10,   11), S( -23,    2), S( -77,  -21), S(   0,    0)
};
constexpr Score PassedFreeAdv[N_RANKS] = { S(   0,    0), S(   0,    0), S(   0,    0), S( -16,   28), S( -28,   63), S( -51,  195), S( -87,  313), S(   0,    0)
};
constexpr Score PassedRookBack = S(  19,   48);

//----------------------------------------------------------
// Cavaliers
constexpr Score KnightBehindPawn = S(   3,   28);
constexpr Score KnightOutpost[2][2] = {   { S(  12,  -16), S(  43,   15) },
    { S(   9,  -20), S(  14,  -13) }
};

// Fous
constexpr Score BishopPair = S(  29,  109);
constexpr Score BishopBadPawn = S(   0,   -6);
constexpr Score BishopBehindPawn = S(  11,   23);
constexpr Score BishopLongDiagonal = S(  32,   23);

// Tours
constexpr Score RookOnOpenFile[2] = { S(   9,   14), S(  31,   13)
};
constexpr Score RookOnBlockedFile = S(  -6,   -5);

// Dames
constexpr Score QueenRelativePin = S( -33,   24);

//----------------------------------------------------------
// Roi
constexpr Score KingAttackPawn = S(  12,   15);
constexpr Score PawnShelter = S(  35,  -14);

//----------------------------------------------------------
// Menaces
constexpr Score ThreatByKnight[N_PIECES] = { S(   0,    0), S(  -1,   22), S(  -2,  104), S(  36,   40), S(  89,   -7), S(  68,  -85), S(   0,    0)
};
constexpr Score ThreatByBishop[N_PIECES] = { S(   0,    0), S(   3,   21), S(  26,   37), S(   0,    0), S(  66,   11), S(  53,   -5), S( 122, 2989)
};
constexpr Score ThreatByRook[N_PIECES] = { S(   0,    0), S(   5,   27), S(  39,   49), S(  45,   54), S(  10,   18), S(  60,  -93), S( 320, 2160)
};
constexpr Score ThreatByKing = S(   1,   31);
constexpr Score HangingThreat = S(   1,   13);
constexpr Score PawnThreat = S(  83,   33);
constexpr Score PawnPushThreat = S(  25,   30);
constexpr Score PawnPushPinnedThreat = S(  -3,  163);
constexpr Score KnightCheckQueen = S(  16,    4);
constexpr Score BishopCheckQueen = S(  22,   32);
constexpr Score RookCheckQueen = S(  22,   17);

//----------------------------------------------------------
// Attaques sur le roi ennemi
constexpr Score SafeCheck[N_PIECES] = { S(   0,    0), S(   0,    0), S( 129,   -3), S(  36,   23), S( 105,    3), S(  35,   43), S(   0,    0)
};
constexpr Score UnsafeCheck[N_PIECES] = { S(   0,    0), S(   0,    0), S(  11,    1), S(  20,   21), S(  41,   -1), S(  11,   19), S(   0,    0)
};
constexpr Score KingAttackersWeight[N_PIECES] = { S(   0,    0), S(   0,    0), S(  30,    3), S(  23,    5), S(  31,  -25), S(   6,   13), S(   0,    0)
};
constexpr Score KingAttacksCount[14] = { S( -45,   32), S( -62,   24), S( -74,   16), S( -81,   22), S( -83,   20), S( -73,   15), S( -50,    4), S( -20,  -10),
    S(  37,  -42), S(  70,  -37), S( 107,  -37), S( 130,   13), S( 159,  -81), S( 115,   73)
};

//----------------------------------------------------------
// Mobilité
constexpr Score KnightMobility[9] = { S( -72, -112), S( -37,    0), S( -11,   62), S(   3,   82), S(  17,   97),
    S(  26,  117), S(  37,  122), S(  48,  126), S(  60,  112)
};

constexpr Score BishopMobility[14] = { S( -55,  -24), S( -26,    0), S( -11,   44), S(  -1,   72), S(  10,   84),
    S(  19,   95), S(  22,  109), S(  30,  113), S(  30,  120), S(  36,  120),
    S(  43,  117), S(  64,  108), S(  68,  113), S(  94,   88)
};

constexpr Score RookMobility[15] = { S( -92, -149), S( -20,   57), S(  -7,  106), S(   5,  106), S(   1,  137),
    S(   7,  147), S(   3,  163), S(  12,  166), S(  12,  175), S(  18,  180),
    S(  24,  189), S(  23,  201), S(  26,  207), S(  34,  215), S(  45,  209)
};

constexpr Score QueenMobility[28] = { S( -66,  -49), S(-104,  -57), S( -83, -104), S( -21, -102), S( -22,   24),
    S( -31,  162), S( -24,  232), S( -24,  273), S( -20,  306), S( -13,  306),
    S(  -8,  313), S(  -4,  318), S(   4,  314), S(   7,  321), S(  10,  326),
    S(  15,  322), S(  15,  327), S(  20,  323), S(  18,  330), S(  23,  327),
    S(  34,  305), S(  39,  290), S(  59,  278), S(  64,  247), S( 102,  209),
    S( 117,  168), S( 146,  158), S( 108,  150)
};



//============================================================== FIN TUNER


//------------------------------------------------------------
//  Bonus car on a le trait
constexpr int Tempo = 18;


#endif // EVALUATE_H
