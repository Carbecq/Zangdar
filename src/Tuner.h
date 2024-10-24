#ifndef TUNER_H
#define TUNER_H
#include "Board.h"

// Le fichier doit avoir les fins de linue en UNIX
//                 être UTF8 simple

// https://www.talkchess.com/forum3/viewtopic.php?f=7&t=75350


// #define DATASET "D:/Echecs/Programmation/Zangdar/tuner/AndrewGrant/E12.41-1M-D12-Resolved.book"
// #define NPOSITIONS   (9996883) // Total FENS in the book

// #define DATASET "D:/Echecs/Programmation/Zangdar/tuner/AndrewGrant/E12.33-12.41-1M-D12-Resolved.book"
// #define NPOSITIONS   (19996623) // Total FENS in the book

// #define DATASET "D:/Echecs/Programmation/Zangdar/tuner/AndrewGrant/E12.52-1M-D12-Resolved.book"
// #define NPOSITIONS   (9998762) // Total FENS in the book



#if defined USE_TUNER


struct EvalTrace {
    Score eval;
    int   scale;

    Score PawnValue[N_COLORS];
    Score KnightValue[N_COLORS];
    Score BishopValue[N_COLORS];
    Score RookValue[N_COLORS];
    Score QueenValue[N_COLORS];
//5
    Score PawnPSQT[N_SQUARES][N_COLORS];
    Score KnightPSQT[N_SQUARES][N_COLORS];
    Score BishopPSQT[N_SQUARES][N_COLORS];
    Score RookPSQT[N_SQUARES][N_COLORS];
    Score QueenPSQT[N_SQUARES][N_COLORS];
    Score KingPSQT[N_SQUARES][N_COLORS];
//5 + 6*64 = 389
    Score PawnDoubled[N_COLORS];
    Score PawnDoubled2[N_COLORS];
    Score PawnSupport[N_COLORS];
    Score PawnOpen[N_COLORS];
    Score PawnPhalanx[N_RANKS][N_COLORS];
    Score PawnIsolated[N_COLORS];
    Score PawnPassed[N_RANKS][N_COLORS];
    Score PassedDefended[N_RANKS][N_COLORS];
// 389 + 29 = 418
    Score PassedSquare[N_COLORS];
    Score PassedDistUs[N_RANKS][N_COLORS];
    Score PassedDistThem[N_COLORS];
    Score PassedBlocked[N_RANKS][N_COLORS];
    Score PassedFreeAdv[N_RANKS][N_COLORS];
    Score PassedRookBack[N_COLORS];
// 418 + 27+27 = 445
    Score KnightBehindPawn[N_COLORS];
    Score KnightOutpost[2][2][N_COLORS];
// 445 + 5 = 450
    Score BishopPair[N_COLORS];
    Score BishopBadPawn[N_COLORS];
    Score BishopBehindPawn[N_COLORS];
    Score BishopLongDiagonal[N_COLORS];
// 450 + 4 = 454
    Score RookOnOpenFile[2][N_COLORS];
    Score RookOnBlockedFile[N_COLORS];
// 454 + 3 = 457
    Score QueenRelativePin[N_COLORS];
// 457 + 1 = 458
    Score KingAttackPawn[N_COLORS];
    Score PawnShelter[N_COLORS];
// 458 + 2 = 460
    Score ThreatByKnight[N_PIECES][N_COLORS];
    Score ThreatByBishop[N_PIECES][N_COLORS];
    Score ThreatByRook[N_PIECES][N_COLORS];
    Score ThreatByKing[N_COLORS];
    Score HangingThreat[N_COLORS];
    Score PawnThreat[N_COLORS];
    Score PawnPushThreat[N_COLORS];
    Score PawnPushPinnedThreat[N_COLORS];
    Score KnightCheckQueen[N_COLORS];
    Score BishopCheckQueen[N_COLORS];
    Score RookCheckQueen[N_COLORS];
// 460 + 29 = 489
    Score SafeCheck[N_PIECES][N_COLORS];
    Score UnsafeCheck[N_PIECES][N_COLORS];
    Score KingAttackersWeight[N_PIECES][N_COLORS];
    Score KingAttacksCount[14][N_COLORS];
// 489 + 35 = 524
    Score KnightMobility[9][N_COLORS];
    Score BishopMobility[14][N_COLORS];
    Score RookMobility[15][N_COLORS];
    Score QueenMobility[28][N_COLORS];
// 524 + 66 = 590
};

struct TexelTuple {
    int    index;
    double coeff;
} ;


struct TexelEntry {
    int    seval;       // static eval
    int    phase256;
    int    turn;
    int    ntuples;
    Score  eval;
    double result;
    double scale;
    double pfactors[2];
    TexelTuple *tuples;
} ;




class Tuner
{
public:
    [[nodiscard]] Tuner() noexcept;
    void runTexelTuning();

    EvalTrace Trace;    // Paramètres initialisés lors de l'évaluation

private:
    // constexpr static char DATASET[] = "D:/Echecs/Programmation/Zangdar/tuner/Zurichess/quiet-labeled.epd";
    // constexpr static int NPOSITIONS = 725000; // Total FENS in the book

    // constexpr static char DATASET[] = "D:/Echecs/Programmation/Zangdar/tuner/AndrewGrant/E12.33-1M-D12-Resolved.book";
    // constexpr static int    NPOSITIONS    = 9999740; // Total FENS in the book

    constexpr static char DATASET[]     = "/tuner/Lichess/lichess-big3-resolved.book";
    constexpr static int  NPOSITIONS    =  7153653; // Total FENS in the book

    constexpr static double NPOSITIONS_d  = NPOSITIONS; // Total FENS in the book

constexpr static int    N_PHASES    = 2;
constexpr static int    NTERMS      = 590;     // Number of terms being tuned
constexpr static double NTERMS_d    = 590;     // Number of terms being tuned

constexpr static int    MAXEPOCHS   =   20000; // Max number of epochs allowed
constexpr static int    REPORTING   =     1000; // How often to print the new parameters
constexpr static int    NPARTITIONS =      64; // Total thread partitions
constexpr static double LRRATE      =    0.01; // Learning rate
constexpr static double LRDROPRATE  =    1.00; // Cut LR by this each LR-step
constexpr static int    LRSTEPRATE  =     250; // Cut LR after this many epochs
constexpr static double BETA_1      =     0.9; // ADAM Momemtum Coefficient
constexpr static double BETA_2      =   0.999; // ADAM Velocity Coefficient

constexpr static int    STACKSIZE   = static_cast<int>(NPOSITIONS_d * NTERMS_d / 64.0);

    TexelTuple* TupleStack;
    int TupleStackSize = STACKSIZE;
    EvalTrace EmptyTrace;

    double TexelCoeffs[NTERMS];
    double TexelVector[NTERMS][N_PHASES];

    double TunedEvaluationErrors(const TexelEntry *entries, double params[NTERMS][N_PHASES], double K);
    void   ComputeGradient(const TexelEntry *entries, double gradient[NTERMS][N_PHASES], double params[NTERMS][N_PHASES], double K);
    void   UpdateSingleGradient(const TexelEntry &entry, double gradient[NTERMS][N_PHASES], double params[NTERMS][N_PHASES], double K);
    double LinearEvaluation(const TexelEntry &entry, double params[NTERMS][N_PHASES]);
    double ComputeOptimalK(const TexelEntry *entries);
    double StaticEvaluationErrors(const TexelEntry *entries, double K);
    double Sigmoid(double K, double E);

    void   InitTunerEntries(TexelEntry *entries);
    void   InitTunerEntry(TexelEntry &entry, Board *board);
    void   InitTunerTuples(TexelEntry &entry, const double coeffs[NTERMS]);

    void   InitCoefficients(double coeffs[NTERMS]);
    void   InitCoeffSingle(double coeffs[NTERMS], Score score[2], int& index);
    void   InitCoeffArray(double coeffs[NTERMS], Score score[][2], int imax, int& index);

    void   PrintParameters(double params[NTERMS][N_PHASES], double current[NTERMS][N_PHASES]);
    void   PrintPieceValues(double params[NTERMS][N_PHASES], int &index);
    void   PrintPSQT(double params[NTERMS][N_PHASES], int& index);
    void   PrintMobility(double params[NTERMS][N_PHASES], int &index);

    void   InitBaseParams(double tparams[NTERMS][N_PHASES]);
    void   InitBaseSingle(double tparams[NTERMS][N_PHASES], const Score data, int& index);
    void   InitBaseArray(double tparams[NTERMS][N_PHASES], const Score *data, int imax, int& index);

    void   PrintSingle(const std::string &name, double params[NTERMS][N_PHASES], int &index);
    void   PrintArray(const std::string &name, double params[NTERMS][N_PHASES], int &index, int imax, const std::string &dim, int length);
    void   PrintArray2D(const std::string &name, double params[NTERMS][N_PHASES], int &index, int imax, int jmax, const std::string &dim, int length);

    // L'utilisation des tableaux 2D et plus est compliqué en C++.
    template <size_t rows, size_t cols>
    void InitCoeffArray2D(double coeffs[], Score (&score)[rows][cols][2], int& index)
    {
        for (size_t i = 0; i < rows; ++i)
            for (size_t j = 0; j < cols; ++j)
                coeffs[index++] = score[i][j][WHITE] - score[i][j][BLACK];
    }

    template <size_t rows, size_t cols>
    void InitBaseArray2D(double tparams[NTERMS][N_PHASES], const Score (&score)[rows][cols], int& index)
    {
        for (size_t i=0; i<rows; i++)
        {
            for (size_t j=0; j<cols; j++)
            {
                tparams[index][MG] = static_cast<double>(MgScore(score[i][j]));
                tparams[index][EG] = static_cast<double>(EgScore(score[i][j]));
                index++;
            }
        }
    }

};

extern Tuner ownTuner;

#endif

#endif // TUNER_H
