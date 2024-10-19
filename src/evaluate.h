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
    Score kingAttackersWeight[N_COLORS];

    // kingAttacksCount[color] is the number of attacks by the given color to
    // squares directly adjacent to the enemy king. Pieces which attack more
    // than one square are counted multiple times. For instance, if there is
    // a white knight on g5 and black's king is on g8, this white knight adds 2
    // to kingAttacksCount[WHITE].
    int kingAttacksCount[N_COLORS];


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
    P_MG =   95, P_EG =  211,
    N_MG =  400, N_EG =  648,
    B_MG =  410, B_EG =  677,
    R_MG =  530, R_EG = 1149,
    Q_MG = 1407, Q_EG = 1985,
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
    S( -16,   14), S(  -4,   15), S(   1,    9), S(  -5,   14), S(   7,   27), S(  21,   12), S(  38,   -2), S( -14,   -8),
    S( -32,    6), S( -32,   -2), S( -17,   -6), S( -21,   -2), S(  -7,    1), S( -18,    5), S(  -5,  -13), S( -23,   -9),
    S( -26,   18), S( -28,   11), S( -11,   -8), S(  -3,  -16), S(  10,  -15), S( -10,   -6), S( -23,    1), S( -21,   -2),
    S( -15,   45), S( -10,   20), S(  -1,    3), S(  -6,  -15), S(  16,  -11), S(  14,   -5), S(  -2,   16), S(  -7,   23),
    S(  32,   60), S(  17,   74), S(  48,   34), S(  55,   12), S(  64,   17), S(  95,   28), S(  74,   73), S(  26,   81),
    S(  56,   -1), S(  59,    0), S(  42,   59), S(  92,   18), S(  72,   37), S(  67,   27), S( -39,   77), S( -73,   80),
    S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0)
};

constexpr Score KnightPSQT[N_SQUARES] = { S( -61,  -26), S( -13,  -16), S( -19,   -1), S(   6,   12), S(  15,    9), S(  12,  -18), S(  -8,   -6), S( -14,  -11),
    S( -25,  -12), S( -14,    2), S(  -8,   -2), S(   7,    7), S(   9,   -1), S(   8,   -4), S(  11,  -15), S(  11,    4),
    S( -10,   -5), S(  -4,    4), S(  11,   11), S(  16,   34), S(  28,   28), S(  22,    2), S(  23,   -5), S(  18,    0),
    S(   6,   22), S(  15,   10), S(  21,   36), S(  24,   34), S(  18,   50), S(  30,   24), S(  30,   19), S(  20,   20),
    S(   5,   28), S(  11,   20), S(  30,   33), S(  30,   46), S(  39,   40), S(  57,   27), S(  45,   18), S(  46,    7),
    S( -15,    6), S(  -5,   17), S(  -2,   41), S(  16,   47), S(  49,   30), S(  66,  -10), S(  19,   -6), S(   9,  -16),
    S( -18,   -5), S(  -5,   19), S(   7,   15), S(  28,   21), S(  27,    0), S(  69,  -14), S(   7,    8), S(  22,  -38),
    S(-177,  -75), S(-125,  -11), S(-134,   33), S( -51,    3), S(  -8,   10), S(-123,   10), S( -92,  -20), S(-125, -115)
};

constexpr Score BishopPSQT[N_SQUARES] = { S(  12,  -24), S(  39,    4), S(  29,   14), S(  13,   12), S(  37,    7), S(  15,   20), S(  42,   -9), S(  29,  -51),
    S(  28,    1), S(  13,  -29), S(  30,   -7), S(  11,   19), S(  21,   16), S(  34,    4), S(  33,  -32), S(  36,  -27),
    S(  17,   10), S(  40,   25), S(  15,    9), S(  16,   32), S(  28,   39), S(  21,    7), S(  51,   12), S(  37,   -6),
    S(  13,   11), S(  10,   28), S(  18,   37), S(  43,   44), S(  22,   39), S(  25,   27), S(  11,   23), S(  31,   -2),
    S(  -1,   23), S(  24,   30), S(  34,   30), S(  37,   64), S(  46,   39), S(  28,   38), S(  25,   26), S(  -1,   21),
    S(   8,   28), S(  34,   22), S(  -3,    7), S(  45,   20), S(  30,   29), S(  54,    8), S(  30,   21), S(  45,   17),
    S(   7,   -1), S( -33,    5), S(   8,   20), S( -18,   35), S(  11,   17), S(   4,   24), S( -49,    8), S( -13,    3),
    S( -49,   15), S( -55,   38), S(-101,   51), S(-117,   56), S(-115,   50), S(-107,   39), S( -27,   23), S( -73,   -8)
};

constexpr Score RookPSQT[N_SQUARES] = { S(  -5,   19), S(  -8,   17), S(  -6,   21), S(   4,    7), S(  10,   -3), S(  10,    4), S(   6,    6), S(  -4,    3),
    S( -20,   15), S( -14,   26), S(   1,   21), S(   0,   21), S(   9,    9), S(   9,    3), S(  21,   -9), S( -15,    3),
    S( -23,   38), S( -14,   29), S( -11,   25), S( -13,   26), S(   0,   18), S(   8,   10), S(  31,   -9), S(  12,   -5),
    S( -20,   58), S( -23,   60), S( -14,   52), S( -14,   45), S( -11,   41), S( -18,   47), S(   7,   34), S( -11,   34),
    S( -18,   82), S(   3,   73), S(   1,   77), S(  -6,   65), S(   3,   45), S(  18,   46), S(  14,   54), S(   3,   49),
    S( -27,   83), S(  11,   75), S(  -6,   81), S(  -3,   69), S(  27,   54), S(  23,   48), S(  70,   40), S(  12,   45),
    S( -21,   76), S( -20,   90), S( -12,   99), S(  16,   75), S(  -9,   78), S(  -1,   73), S(   5,   69), S(  27,   49),
    S(  -6,   64), S(  -9,   71), S( -29,   87), S( -31,   74), S( -15,   66), S(   4,   73), S(  19,   75), S(  31,   61)
};

constexpr Score QueenPSQT[N_SQUARES] = { S( -11,   -6), S( -19,    1), S( -13,   13), S(  -4,    9), S(  -7,   -1), S( -18,  -18), S(   8,  -60), S(  11,  -46),
    S(   9,   -7), S(   8,    2), S(  12,   11), S(  16,   38), S(  16,   31), S(  23,  -34), S(  40,  -88), S(  42,  -70),
    S(  19,   -5), S(   9,   45), S(  -3,   73), S(  -5,   68), S(   3,   72), S(  26,   43), S(  35,   17), S(  32,   -2),
    S(  23,   21), S(  -1,   76), S(  -7,   90), S(  -5,  100), S(  -7,   94), S(  15,   74), S(  24,   62), S(  32,   56),
    S(  -2,   56), S(   7,   71), S(  -3,   85), S( -13,  112), S( -17,  125), S(  -1,  114), S(  15,  125), S(  16,   79),
    S(   3,   52), S(  -1,   56), S( -12,  104), S( -10,  115), S(  -8,  134), S(  -4,  116), S(  18,   76), S(   8,   90),
    S(  -9,   43), S( -38,   68), S( -39,  120), S( -86,  180), S( -91,  210), S( -32,  138), S( -34,  131), S(  25,  120),
    S( -49,   75), S( -54,   72), S( -39,  102), S( -16,  100), S( -34,  113), S(  -4,  108), S(   3,   88), S( -12,   89)
};

constexpr Score KingPSQT[N_SQUARES] = { S(  78,  -43), S( 106,   -3), S(  80,   35), S( -35,   59), S(  36,   32), S(  -3,   59), S(  76,   13), S(  84,  -50),
    S( 131,    5), S(  99,   42), S(  74,   61), S(  19,   79), S(  21,   85), S(  52,   72), S( 115,   37), S( 108,   10),
    S(  42,   24), S( 101,   51), S(  49,   82), S(  26,  100), S(  33,  100), S(  52,   86), S(  74,   64), S(  17,   50),
    S(  17,   38), S(  76,   66), S(  65,   99), S(  -9,  132), S(  21,  123), S(  58,  104), S(  56,   81), S( -59,   78),
    S(  25,   54), S(  72,   90), S(  52,  125), S(  15,  146), S(  25,  147), S(  78,  135), S(  76,  112), S( -20,   88),
    S(  17,   66), S( 107,  110), S(  84,  127), S(  62,  150), S( 115,  159), S( 148,  146), S( 129,  139), S(  57,   82),
    S( -11,   49), S(  44,  113), S(  39,  127), S(  87,  120), S(  90,  121), S(  94,  150), S(  92,  144), S(  59,   83),
    S( -17,  -67), S(  22,    7), S(  11,   45), S(   5,   80), S(   5,   68), S(  24,   87), S(  48,   94), S(  22,  -58)
};

//----------------------------------------------------------
// Pions
constexpr Score PawnDoubled = S( -10,  -49);
constexpr Score PawnDoubled2 = S(  -7,  -27);
constexpr Score PawnSupport = S(  20,   17);
constexpr Score PawnOpen = S( -14,  -21);
constexpr Score PawnPhalanx[N_RANKS] = { S(   0,    0), S(   8,   -4), S(  18,   13), S(  35,   35), S(  66,  121), S( 217,  374), S( 166,  407), S(   0,    0)
};
constexpr Score PawnIsolated = S(  -6,  -14);
constexpr Score PawnPassed[N_RANKS] = { S(   0,    0), S(  -7,   19), S( -17,   35), S( -79,  111), S( -57,  143), S(  41,  163), S( 293,  189), S(   0,    0)
};
constexpr Score PassedDefended[N_RANKS] = { S(   0,    0), S(   0,    0), S(   5,  -14), S(   5,   -9), S(   2,   35), S(  29,  105), S( 155,  103), S(   0,    0)
};

//----------------------------------------------------------
// Pions passés
constexpr Score PassedSquare = S( -35,  410);
constexpr Score PassedDistUs[N_RANKS] = { S(   0,    0), S(   0,    0), S(   0,    0), S(  19,  -32), S(  21,  -44), S(   4,  -46), S( -18,  -33), S(   0,    0)
};
constexpr Score PassedDistThem = S(  -4,   20);
constexpr Score PassedBlocked[N_RANKS] = { S(   0,    0), S(   0,    0), S(   0,    0), S(   1,    0), S(  -3,   10), S( -11,   -1), S( -75,  -29), S(   0,    0)
};
constexpr Score PassedFreeAdv[N_RANKS] = { S(   0,    0), S(   0,    0), S(   0,    0), S( -10,   23), S( -19,   57), S( -33,  184), S( -82,  294), S(   0,    0)
};
constexpr Score PassedRookBack = S(  17,   45);

//----------------------------------------------------------
// Cavaliers
constexpr Score KnightBehindPawn = S(   2,   26);
constexpr Score KnightOutpost[2][2] = {   { S(  17,  -11), S(  45,   22) },
    { S(  18,   -6), S(  23,    7) }
};

// Fous
constexpr Score BishopPair = S(  27,  105);
constexpr Score BishopBadPawn = S(  -1,   -6);
constexpr Score BishopBehindPawn = S(   9,   22);
constexpr Score BishopLongDiagonal = S(  32,   23);

// Tours
constexpr Score RookOnOpenFile[2] = { S(  10,   25), S(  30,   11)
};
constexpr Score RookOnBlockedFile = S(  -5,   -5);

// Reines
constexpr Score QueenRelativePin = S( -32,   29);

//----------------------------------------------------------
// Roi
constexpr Score KingAttackPawn = S(   4,   40);
constexpr Score PawnShelter = S(  34,  -13);

//----------------------------------------------------------
// Menaces
constexpr Score PawnThreat = S(  76,   36);
constexpr Score PushThreat = S(  21,    5);
constexpr Score ThreatByMinor[N_PIECES] = { S(   0,    0), S(   0,    0), S(  24,   35), S(  35,   45), S(  68,   16), S(  57,    2), S(   0,    0)
};
constexpr Score ThreatByRook[N_PIECES] = { S(   0,    0), S(   0,    0), S(  24,   46), S(  29,   53), S(  -5,   37), S(  81,  -39), S(   0,    0)
};

//----------------------------------------------------------
// Attaques sur le roi ennemi
constexpr Score SafeCheck[N_PIECES] = { S(   0,    0), S(   0,    0), S( 123,   -4), S(  36,   22), S( 101,    5), S(  34,   39), S(   0,    0)
};
constexpr Score UnsafeCheck[N_PIECES] = { S(   0,    0), S(   0,    0), S(  10,    0), S(  19,   22), S(  39,   -1), S(  12,   16), S(   0,    0)
};
constexpr Score KingAttackWeights[N_PIECES] = { S(   0,    0), S(   0,    0), S(  28,    5), S(  22,    9), S(  27,  -19), S(   7,   11), S(   0,    0)
};
constexpr Score KingAttacks[14] = { S( -43,   41), S( -60,   30), S( -72,   22), S( -79,   25), S( -79,   21), S( -69,    9), S( -46,   -1), S( -16,  -20),
    S(  36,  -50), S(  64,  -40), S(  96,  -36), S( 114,    5), S( 144,  -86), S(  98,   65)
};

//----------------------------------------------------------
// Mobilité
constexpr Score KnightMobility[9] = { S( -71, -125), S( -42,   -8), S( -18,   52), S(  -5,   71), S(   9,   85),
    S(  19,  106), S(  30,  110), S(  42,  116), S(  54,  101)
};

constexpr Score BishopMobility[14] = { S( -57,  -36), S( -30,   -8), S( -14,   35), S(  -4,   62), S(   7,   72),
    S(  17,   84), S(  22,   97), S(  30,  100), S(  32,  106), S(  38,  105),
    S(  48,  100), S(  68,   89), S(  65,   98), S(  96,   68)
};

constexpr Score RookMobility[15] = { S( -99, -149), S( -23,   47), S( -11,   93), S(   1,   92), S(  -2,  121),
    S(   4,  130), S(   0,  145), S(   8,  149), S(   9,  157), S(  15,  161),
    S(  22,  169), S(  24,  179), S(  28,  185), S(  38,  189), S(  53,  178)
};

constexpr Score QueenMobility[28] = { S( -65,  -48), S(-101,  -56), S( -86, -106), S( -23, -116), S( -24,  -15),
    S( -35,  115), S( -28,  180), S( -28,  219), S( -25,  254), S( -18,  256),
    S( -15,  265), S( -12,  272), S(  -5,  270), S(  -3,  277), S(  -1,  284),
    S(   3,  282), S(   2,  289), S(   5,  288), S(   2,  296), S(   4,  298),
    S(  12,  281), S(  11,  277), S(  31,  267), S(  34,  244), S(  90,  196),
    S( 108,  166), S( 142,  159), S( 114,  154)
};


//============================================================== FIN TUNER


//------------------------------------------------------------
//  Bonus car on a le trait
constexpr int Tempo = 18;


#endif // EVALUATE_H
