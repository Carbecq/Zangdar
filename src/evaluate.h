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
    P_MG =  102, P_EG =  219,
    N_MG =  409, N_EG =  673,
    B_MG =  427, B_EG =  709,
    R_MG =  552, R_EG = 1199,
    Q_MG = 1402, Q_EG = 2101,
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
    S( -18,   17), S(  -8,   17), S(  -1,   10), S(  -6,   14), S(   4,   28), S(  19,   13), S(  37,   -1), S( -17,   -7),
    S( -36,    9), S( -34,   -1), S( -19,   -5), S( -22,   -1), S(  -8,    2), S( -20,    5), S(  -6,  -12), S( -26,   -8),
    S( -27,   21), S( -29,   13), S( -10,   -8), S(  -1,  -16), S(  13,  -15), S( -10,   -6), S( -25,    3), S( -24,    0),
    S( -16,   50), S( -10,   23), S(  -1,    5), S(  -5,  -14), S(  18,  -10), S(  17,   -5), S(  -3,   19), S(  -7,   27),
    S(  32,   65), S(  16,   81), S(  50,   37), S(  57,   14), S(  68,   19), S(  99,   31), S(  75,   78), S(  25,   87),
    S(  60,   -3), S(  63,   -3), S(  45,   59), S( 102,   13), S(  71,   40), S(  64,   31), S( -41,   79), S( -83,   87),
    S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0)
};

constexpr Score KnightPSQT[N_SQUARES] = { S( -62,  -17), S( -12,  -17), S( -20,   -1), S(   6,   11), S(  17,    8), S(  14,  -21), S(  -7,   -8), S(  -9,   -6),
    S( -27,   -9), S( -13,    1), S(  -7,   -4), S(  10,    4), S(  12,   -4), S(  10,   -7), S(  14,  -21), S(  13,    4),
    S(  -9,   -5), S(  -2,    3), S(  13,   10), S(  17,   32), S(  30,   26), S(  26,    1), S(  27,   -8), S(  20,    0),
    S(  18,   36), S(  22,   16), S(  32,   43), S(  31,   40), S(  31,   59), S(  43,   33), S(  47,   25), S(  33,   35),
    S(  10,   39), S(  14,   23), S(  28,   38), S(  33,   50), S(  38,   45), S(  63,   30), S(  47,   20), S(  54,   16),
    S(  -8,   18), S(  -8,   27), S(  -1,   45), S(  12,   49), S(  47,   35), S(  74,  -10), S(  17,    5), S(  22,   -7),
    S( -29,    0), S( -10,   21), S(  -4,   23), S(  20,   28), S(  22,    8), S(  66,   -5), S(  12,    7), S(  22,  -37),
    S(-174,  -84), S(-132,  -10), S(-131,   29), S( -47,   -3), S(  -9,    9), S(-107,    1), S( -97,  -22), S(-119, -125)
};

constexpr Score BishopPSQT[N_SQUARES] = { S(   9,  -25), S(  39,    4), S(  26,   17), S(  11,   14), S(  33,   10), S(  14,   22), S(  42,  -11), S(  30,  -59),
    S(  26,    1), S(  11,  -31), S(  30,   -9), S(   8,   20), S(  21,   16), S(  33,    2), S(  34,  -33), S(  34,  -28),
    S(  15,   12), S(  41,   25), S(  11,    9), S(  15,   32), S(  26,   38), S(  21,    7), S(  51,   12), S(  36,   -5),
    S(  19,   16), S(  13,   33), S(  25,   40), S(  44,   42), S(  32,   39), S(  31,   37), S(  20,   28), S(  41,    4),
    S(  -5,   27), S(  26,   37), S(  33,   31), S(  35,   63), S(  46,   38), S(  30,   40), S(  26,   37), S(  -1,   24),
    S(   9,   32), S(  32,   23), S(  -5,   12), S(  44,   22), S(  30,   32), S(  55,    9), S(  33,   21), S(  47,   20),
    S(   6,   -2), S( -31,    6), S(   7,   21), S( -17,   37), S(  13,   20), S(   7,   25), S( -48,   10), S(  -1,   -5),
    S( -44,   10), S( -53,   36), S( -86,   44), S(-112,   53), S(-109,   47), S( -81,   32), S( -19,   24), S( -84,  -11)
};

constexpr Score RookPSQT[N_SQUARES] = { S(  -1,   28), S(  -6,   26), S(  -3,   30), S(   7,   13), S(  15,    3), S(  14,   10), S(  10,   15), S(   1,   10),
    S( -19,   23), S( -15,   36), S(   2,   31), S(   0,   29), S(  10,   17), S(  11,   10), S(  23,   -2), S( -10,    7),
    S( -23,   46), S( -17,   39), S( -13,   34), S( -17,   37), S(  -2,   28), S(   7,   17), S(  31,   -2), S(  13,   -2),
    S( -19,   65), S( -21,   72), S( -12,   62), S( -17,   58), S(  -4,   53), S( -11,   58), S(  16,   46), S(  -7,   40),
    S( -24,   85), S(  -2,   77), S(  -4,   82), S( -12,   69), S(   0,   49), S(  17,   47), S(  12,   59), S(   1,   51),
    S( -32,   83), S(   3,   76), S( -12,   81), S( -12,   71), S(  20,   55), S(  13,   49), S(  62,   42), S(  11,   44),
    S( -18,   69), S( -24,   84), S( -17,   91), S(  11,   64), S( -16,   69), S(  -2,   70), S(   6,   66), S(  33,   47),
    S(  -4,   72), S( -20,   86), S( -33,  102), S( -40,   91), S( -21,   83), S(   7,   83), S(  17,   87), S(  40,   66)
};

constexpr Score QueenPSQT[N_SQUARES] = { S( -27,   29), S( -34,   38), S( -28,   52), S( -18,   48), S( -22,   36), S( -32,   14), S(   1,  -40), S(   6,  -33),
    S(   0,   20), S(  -2,   31), S(   3,   37), S(   7,   64), S(   7,   61), S(  13,    0), S(  32,  -61), S(  37,  -55),
    S(  11,   16), S(   4,   69), S(  -8,  100), S( -11,   93), S(  -2,   98), S(  22,   66), S(  35,   33), S(  27,   16),
    S(  21,   49), S(   6,   95), S(   1,  111), S(   1,  118), S(   7,  113), S(  25,   93), S(  35,   75), S(  32,   73),
    S(  -2,   77), S(  13,   87), S(  10,   96), S(   9,  110), S(  13,  114), S(  19,   95), S(  26,  130), S(  22,   78),
    S(  21,   50), S(  15,   60), S(  14,  103), S(  16,  112), S(  30,  117), S(  22,  101), S(  41,   58), S(  28,   60),
    S(   6,   39), S(  -4,   46), S( -10,  114), S( -51,  166), S( -56,  194), S(   5,  113), S(  -7,  108), S(  43,  106),
    S( -43,   80), S( -52,   84), S( -29,  109), S(   4,   98), S( -17,  111), S(  -7,  114), S(  14,   76), S(  -5,   89)
};

constexpr Score KingPSQT[N_SQUARES] = { S(  94,  -60), S( 122,  -14), S(  93,   28), S( -29,   52), S(  47,   24), S(   6,   52), S(  90,    4), S(  99,  -64),
    S( 153,   -8), S( 113,   36), S(  86,   56), S(  28,   76), S(  29,   83), S(  63,   68), S( 131,   30), S( 124,    2),
    S(  50,   18), S( 108,   48), S(  50,   83), S(  25,  102), S(  33,  102), S(  58,   86), S(  84,   61), S(  25,   45),
    S(  11,   38), S(  57,   72), S(  33,  111), S( -33,  142), S(  -9,  136), S(  34,  117), S(  31,   94), S( -66,   82),
    S(  21,   52), S(  55,   93), S(  22,  135), S( -14,  156), S(  -6,  160), S(  48,  148), S(  55,  120), S( -34,   92),
    S(   4,   65), S(  95,  113), S(  65,  135), S(  41,  161), S( 100,  169), S( 139,  152), S( 117,  142), S(  61,   80),
    S( -20,   47), S(  38,  111), S(  27,  130), S(  91,  121), S(  92,  124), S(  87,  156), S(  88,  146), S(  64,   91),
    S( -14,  -74), S(  18,   -3), S(  10,   43), S(   4,   77), S(   6,   71), S(  20,   91), S(  49,   94), S(  26,  -63)
};

//----------------------------------------------------------
// Pions
constexpr Score PawnDoubled = S( -11,  -50);
constexpr Score PawnDoubled2 = S(  -8,  -27);
constexpr Score PawnSupport = S(  21,   15);
constexpr Score PawnOpen = S( -13,  -19);
constexpr Score PawnPhalanx[N_RANKS] = { S(   0,    0), S(   9,   -5), S(  19,   13), S(  32,   34), S(  64,  120), S( 207,  383), S( 165,  421), S(   0,    0)
};
constexpr Score PawnIsolated = S(  -6,  -15);
constexpr Score PawnPassed[N_RANKS] = { S(   0,    0), S(  -7,   20), S( -17,   38), S( -76,  113), S( -49,  145), S(  50,  169), S( 298,  194), S(   0,    0)
};
constexpr Score PassedDefended[N_RANKS] = { S(   0,    0), S(   0,    0), S(   6,  -15), S(   6,  -10), S(   4,   33), S(  34,  101), S( 156,  104), S(   0,    0)
};

//----------------------------------------------------------
// Pions passés
constexpr Score PassedSquare = S( -45,  439);
constexpr Score PassedDistUs[N_RANKS] = { S(   0,    0), S(   0,    0), S(   0,    0), S(  19,  -33), S(  21,  -45), S(   5,  -48), S( -17,  -33), S(   0,    0)
};
constexpr Score PassedDistThem = S(  -4,   21);
constexpr Score PassedBlocked[N_RANKS] = { S(   0,    0), S(   0,    0), S(   0,    0), S(   1,    2), S( -10,   11), S( -24,    2), S( -78,  -20), S(   0,    0)
};
constexpr Score PassedFreeAdv[N_RANKS] = { S(   0,    0), S(   0,    0), S(   0,    0), S( -16,   28), S( -28,   63), S( -51,  195), S( -90,  315), S(   0,    0)
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
constexpr Score BishopLongDiagonal = S(  33,   23);

// Tours
constexpr Score RookOnOpenFile[2] = { S(   9,   14), S(  31,   13)
};
constexpr Score RookOnBlockedFile = S(  -6,   -5);

// Dames
constexpr Score QueenRelativePin = S( -33,   24);

//----------------------------------------------------------
// Roi
constexpr Score KingAttackPawn = S(  13,   15);
constexpr Score PawnShelter = S(  35,  -14);

//----------------------------------------------------------
// Menaces
constexpr Score ThreatByKnight[N_PIECES] = { S(   0,    0), S(  -1,   22), S(  -2,  104), S(  36,   40), S(  90,   -7), S(  68,  -86), S(   0,    0)
};
constexpr Score ThreatByBishop[N_PIECES] = { S(   0,    0), S(   3,   21), S(  26,   37), S(   0,    0), S(  66,   10), S(  53,   -4), S( 124, 2990)
};
constexpr Score ThreatByRook[N_PIECES] = { S(   0,    0), S(   5,   27), S(  39,   49), S(  45,   54), S(  10,   18), S(  60,  -92), S( 322, 2161)
};
constexpr Score ThreatByKing = S(   1,   31);

constexpr Score HangingThreat = S(   1,   14);

constexpr Score PawnThreat = S(  83,   33);
constexpr Score PawnPushThreat = S(  25,   30);
constexpr Score PawnPushPinnedThreat = S(  -3,  164);

constexpr Score KnightCheckQueen = S(  16,    4);
constexpr Score BishopCheckQueen = S(  23,   31);
constexpr Score RookCheckQueen = S(  22,   17);

//----------------------------------------------------------
// Attaques sur le roi ennemi
constexpr Score SafeCheck[N_PIECES] = { S(   0,    0), S(   0,    0), S( 129,   -3), S(  36,   23), S( 105,    3), S(  35,   43), S(   0,    0)
};
constexpr Score UnsafeCheck[N_PIECES] = { S(   0,    0), S(   0,    0), S(  11,    1), S(  20,   21), S(  42,   -1), S(  11,   19), S(   0,    0)
};
constexpr Score KingAttackersWeight[N_PIECES] = { S(   0,    0), S(   0,    0), S(  30,    4), S(  23,    5), S(  31,  -25), S(   6,   13), S(   0,    0)
};
constexpr Score KingAttacksCount[14] = { S( -45,   31), S( -62,   24), S( -74,   15), S( -81,   21), S( -82,   20), S( -73,   14), S( -50,    4), S( -20,  -11),
    S(  37,  -42), S(  69,  -36), S( 106,  -36), S( 129,   12), S( 160,  -80), S( 115,   74)
};

//----------------------------------------------------------
// Mobilité
constexpr Score KnightMobility[9] = { S( -70, -110), S( -35,    3), S(  -9,   65), S(   5,   85), S(  19,  100),
    S(  28,  120), S(  39,  125), S(  50,  129), S(  62,  115)
};

constexpr Score BishopMobility[14] = { S( -55,  -29), S( -25,   -6), S( -10,   38), S(   0,   66), S(  11,   78),
    S(  20,   89), S(  23,  103), S(  30,  107), S(  31,  114), S(  37,  114),
    S(  44,  111), S(  65,  101), S(  68,  108), S(  94,   83)
};

constexpr Score RookMobility[15] = { S( -92, -150), S( -22,   52), S(  -9,  100), S(   4,  100), S(   0,  131),
    S(   6,  141), S(   1,  157), S(  10,  160), S(  11,  169), S(  17,  174),
    S(  23,  183), S(  21,  195), S(  25,  201), S(  33,  209), S(  44,  203)
};

constexpr Score QueenMobility[28] = { S( -66,  -49), S(-104,  -57), S( -83, -104), S( -20, -102), S( -22,   27),
    S( -31,  165), S( -24,  232), S( -24,  273), S( -20,  308), S( -12,  307),
    S(  -8,  314), S(  -4,  319), S(   5,  315), S(   7,  321), S(  11,  326),
    S(  16,  322), S(  16,  328), S(  21,  324), S(  18,  331), S(  24,  328),
    S(  35,  305), S(  40,  291), S(  60,  278), S(  64,  248), S( 102,  209),
    S( 118,  168), S( 147,  158), S( 108,  150)
};



//============================================================== FIN TUNER


//------------------------------------------------------------
//  Bonus car on a le trait
constexpr int Tempo = 18;


#endif // EVALUATE_H
