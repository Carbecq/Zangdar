#include <chrono>
#include "Tuner.h"
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include "defines.h"
#include "Board.h"
#include "evaluate.h"

/*
  Gradient Decent Tuning for Chess Engines as described by Andrew Grant
  in his paper: Evaluation & Tuning in Chess Engines:

*   Code provenant de Weiss, puis re-écriture en C++
*   voir aussi : Drofa, Ethereal
*
*/

#if defined USE_TUNER

//===================================================
//! \brief  Constructeur
//---------------------------------------------------
Tuner::Tuner() noexcept
{
    std::memset(&Trace,      0, sizeof(EvalTrace));
    std::memset(&EmptyTrace, 0, sizeof(EvalTrace));
}

void Tuner::runTexelTuning()
{
    double baseParams[NTERMS][N_PHASES] = {{0}};
    double params[NTERMS][N_PHASES] = {{0}};
    double momentum[NTERMS][N_PHASES] = {{0}};
    double velocity[NTERMS][N_PHASES] = {{0}};
    double K;
    double error = 0;
    double rate = LRRATE;

    std::cout <<"Tuning << " << NTERMS << " terms using " << NPOSITIONS << " positions from " << DATASET << std::endl;

    // Initialisation des valeurs de base

    std::cout << "InitBaseParams ...";
    InitBaseParams(baseParams);
    std::cout << " done" << std::endl;

    std::cout << "PrintParameters ...";
    PrintParameters(params, baseParams);
    std::cout << " done" << std::endl;

    // Initialisation du tuner
    std::cout << "InitTunerEntries ... \n";
    InitTunerEntries();
    std::cout << "InitTunerEntries done" << std::endl;

    std::cout << "Optimal K... \n" << std::endl;
    K = ComputeOptimalK();
    std::cout << "Optimal K: " << K << std::endl;

    auto start_search      = std::chrono::high_resolution_clock::now();
    auto start_report      = start_search;

    for (int epoch = 1; epoch <= MAXEPOCHS; epoch++)
    {
        // std::cout << "epoch "  << epoch << std::endl;

        double gradient[NTERMS][N_PHASES] = { {0} };
        ComputeGradient(gradient, params, K);

        for (int i = 0; i < NTERMS; i++) 
        {
            double mg_grad = (-K / 200.0) * gradient[i][MG] / NPOSITIONS;
            double eg_grad = (-K / 200.0) * gradient[i][EG] / NPOSITIONS;

            momentum[i][MG] = BETA_1 * momentum[i][MG] + (1.0 - BETA_1) * mg_grad;
            momentum[i][EG] = BETA_1 * momentum[i][EG] + (1.0 - BETA_1) * eg_grad;

            velocity[i][MG] = BETA_2 * velocity[i][MG] + (1.0 - BETA_2) * pow(mg_grad, 2);
            velocity[i][EG] = BETA_2 * velocity[i][EG] + (1.0 - BETA_2) * pow(eg_grad, 2);

            params[i][MG] -= rate * momentum[i][MG] / (1e-8 + sqrt(velocity[i][MG]));
            params[i][EG] -= rate * momentum[i][EG] / (1e-8 + sqrt(velocity[i][EG]));
        }

        error = TunedEvaluationErrors(params, K);
        // printf("\rEpoch [%d] Error = [%.8f], Rate = [%g]", epoch, error, rate);

        // Pre-scheduled Learning Rate drops
        if (epoch % LRSTEPRATE == 0)
            rate = rate / LRDROPRATE;
        if (epoch % REPORTING == 0)
        {
            printf("\rEpoch [%d] Error = [%.8f], Rate = [%g]", epoch, error, rate);
            PrintParameters(params, baseParams);
            auto end            = std::chrono::high_resolution_clock::now();
            auto delta_report   = std::chrono::duration_cast<std::chrono::milliseconds>(end - start_report);
            auto delta_search   = std::chrono::duration_cast<std::chrono::milliseconds>(end - start_search);
            std::cout << "epoch = " << epoch << " ; sec = " << delta_report.count()/1000.0 << "  " << delta_search.count()/1000.0 << std::endl;

            start_report = end;
        }
    }
}

//============================================================
//! \brief  Initalisation du tuner avec les valeurs
//! du DATASET
//------------------------------------------------------------
void Tuner::InitTunerEntries()
{
    Board board;

    std::string     str_file(HOME);
    str_file += DATASET;

    std::ifstream   file(str_file);

    // Ouverture du fichier
    if (!file.is_open())
    {
        std::cout << "[Tuner::InitTunerEntries] impossible d'ouvrir le fichier " << str_file << std::endl;
        exit(1);
    }
    else
    {
        std::cout << "[Tuner::InitTunerEntries] fichier " << str_file << " ouvert" << std::endl;
    }

    std::string     line;
    std::string     str_fen;
    std::string     str_res;
    std::string     str;
    std::string     aux;
    Position        position;

    std::vector<std::string> liste;
    int  index = 0;
    bool f = true;

    // Lecture du fichier ligne à ligne
    while (std::getline(file, line))
    {
        // ligne vide
        if (line.size() < 3)
            continue;

        // Commentaire ou espace au début de la ligne
        aux = line.substr(0,1);
        if (aux == "/" || aux == " ")
            continue;

        liste = split(line, '[');           // Lichess
        // liste = split(line, '"');       // Zurichess

        str_fen = liste[0];
        str_res = liste[1];

        // std::cout << str_fen << std::endl;
        // std::cout << str_res << std::endl;


        f = true;

        // Lichess
        if (line.find("[1.0]") != std::string::npos)
            position.result = 1.0;
        else if (line.find("[0.5]") != std::string::npos)
            position.result = 0.5;
        else if (line.find("[0.0]") != std::string::npos)
            position.result = 0.0;
        else
        {
            printf("Cannot Parse %s\n", line.c_str());
            f = false;
        }

        // Zurichess
        // if (line.find("1-0") != std::string::npos)
        //     position.result = 1.0;
        // else if (line.find("1/2-1/2") != std::string::npos)
        //     position.result = 0.5;
        // else if (line.find("0-1") != std::string::npos)
        //     position.result = 0.0;
        // else
        // {
        //     printf("Cannot Parse %s\n", line.c_str());
        //     f = false;
        // }


        if (f == true)
        {
            // Set the board with the current FEN and initialize
            board.set_fen(str_fen, false);
            InitTunerEntry(position, &board);
            positions[index] = position;
            index++;
        }

        line = "";
        liste.clear();
    }

    file.close();
    if (index != NPOSITIONS)
    {
        std::cout << "erreur index " << index << "  positions " << NPOSITIONS << std::endl;
        return;
    }

    std::cout << "lecture effectuée" << std::endl;

    //   printf("%d %d %d \n", nbr, min_count, max_count);

}

//=======================================================================
//! \brief  Initialisation d'une entrée du tuner
//! Chaque entrée correspond à une position du dataset
//-----------------------------------------------------------------------
void Tuner::InitTunerEntry(Position& position, Board *board)
{
    // phase calculée à partir du nombre de pièces
    int phase24 = board->get_phase24();

    // phase24 =  0  : EndGame
    //           24  : MiddleGame
    position.pfactors[MG] = 0 + phase24 / 24.0;   // 0-24 -> 0-1 ; donc 1 en MG
    position.pfactors[EG] = 1 - phase24 / 24.0;   // 0-24 -> 1-0 ; donc 1 en EG

    // phase
    position.phase256 = board->get_phase256(phase24);

    Trace = EmptyTrace;

    // les coefficients sont relatifs à une position
    std::array<Score, NTERMS> coeffs;

    // Save a WHITE POV static evaluation
    // Evaluation should be from White POV,
    // but we still call evaluate() from stm perspective
    // to get right tempo evaluation
    position.seval = (board->side_to_move == WHITE) ? board->evaluate()
                                                 : -board->evaluate();

    // Initialisation des coefficients de "Trace"
    // à partir de l'évaluation
    // std::cout << "InitCoefficients ...";
    InitCoefficients(coeffs);
    // std::cout << " done" << std::endl;

    // Allocation mémoire des tuples de l'entrée
    // et initialisation à partir des coefficients "coeffs"
    // std::cout << "InitTunerTuples ..." << position.seval;
    InitTunerTuples(position, coeffs);
    // std::cout << " done" << std::endl;


    // 5. Save Final evaluation for easier gradient recalculation
    // As we called evaluate() from stm perspective
    // we need to adjust it here to be from WHITE POW
    position.eval  = Trace.eval;
    position.turn  = board->side_to_move;

    // 6. Also save modifiers to know is it is
    // OCBEndgame
    position.scale = Trace.scale / 128.0;
}

//==========================================================================
//! \brief  Allocation mémoire des coefficients de chaque entrée
//--------------------------------------------------------------------------
void Tuner::InitTunerTuples(Position& position, const std::array<Score, NTERMS>& coeffs)
{
    int length = 0, tidx = 0;

    // Count the needed Coefficients
    // un coeff nul signifie que les scores Blancs et Noirs sont égaux
    for (int i = 0; i < NTERMS; i++)
        length += coeffs[i] != 0.0;

    // length est beaucoup plus petit que NTERMS
    // ce qui permet un gain de mémoire

    // Initialise les éléments de tuples
    // i = index dans les coefficients
    position.tuples.resize(length);
    for (int i = 0; i < NTERMS; i++)
        if (coeffs[i] != 0.0)
            position.tuples[tidx++] = {i, coeffs[i]} ;
}


void Tuner::InitCoefficients(std::array<Score, NTERMS>& coeffs)
{
    int index = 0;

    // Piece values
    InitCoeffSingle(coeffs, Trace.PawnValue,    index);
    InitCoeffSingle(coeffs, Trace.KnightValue,  index);
    InitCoeffSingle(coeffs, Trace.BishopValue,  index);
    InitCoeffSingle(coeffs, Trace.RookValue,    index);
    InitCoeffSingle(coeffs, Trace.QueenValue,   index);

    // PSQT
    InitCoeffArray1D(coeffs, Trace.PawnPSQT,       index);
    InitCoeffArray1D(coeffs, Trace.KnightPSQT,     index);
    InitCoeffArray1D(coeffs, Trace.BishopPSQT,     index);
    InitCoeffArray1D(coeffs, Trace.RookPSQT,       index);
    InitCoeffArray1D(coeffs, Trace.QueenPSQT,      index);
    InitCoeffArray1D(coeffs, Trace.KingPSQT,       index);

    // Pions
    InitCoeffSingle(coeffs, Trace.PawnDoubled,                  index);
    InitCoeffSingle(coeffs, Trace.PawnDoubled2,                 index);
    InitCoeffSingle(coeffs, Trace.PawnSupport,                  index);
    InitCoeffSingle(coeffs, Trace.PawnOpen,                     index);
    InitCoeffArray1D(coeffs, Trace.PawnPhalanx,          index);
    InitCoeffSingle(coeffs, Trace.PawnIsolated,                 index);
    InitCoeffArray1D(coeffs, Trace.PawnPassed,           index);
    InitCoeffArray1D(coeffs, Trace.PassedDefended,       index);

    // Pions passés
    InitCoeffSingle(coeffs, Trace.PassedSquare,                 index);
    InitCoeffArray1D(coeffs, Trace.PassedDistUs,         index);
    InitCoeffSingle(coeffs, Trace.PassedDistThem,               index);
    InitCoeffArray1D(coeffs, Trace.PassedBlocked,        index);
    InitCoeffArray1D(coeffs, Trace.PassedFreeAdv,        index);
    InitCoeffSingle(coeffs, Trace.PassedRookBack,               index);

    // Cavaliers
    InitCoeffSingle(coeffs, Trace.KnightBehindPawn,         index);
    InitCoeffArray2D(coeffs, Trace.KnightOutpost,     index);

    // Fous
    InitCoeffSingle(coeffs, Trace.BishopPair,               index);
    InitCoeffSingle(coeffs, Trace.BishopBadPawn,            index);
    InitCoeffSingle(coeffs, Trace.BishopBehindPawn,         index);
    InitCoeffSingle(coeffs, Trace.BishopLongDiagonal,       index);

    // Tours
    InitCoeffArray1D(coeffs, Trace.RookOnOpenFile,         index);
    InitCoeffSingle(coeffs, Trace.RookOnBlockedFile,        index);

    InitCoeffSingle(coeffs, Trace.QueenRelativePin,        index);

    // Roi
    InitCoeffSingle(coeffs, Trace.KingAttackPawn,           index);
    InitCoeffSingle(coeffs, Trace.PawnShelter,              index);

    // Menaces
    InitCoeffArray1D(coeffs, Trace.ThreatByKnight,          index);
    InitCoeffArray1D(coeffs, Trace.ThreatByBishop,          index);
    InitCoeffArray1D(coeffs, Trace.ThreatByRook,            index);
    InitCoeffSingle(coeffs, Trace.ThreatByKing,                     index);
    InitCoeffSingle(coeffs, Trace.HangingThreat,                    index);
    InitCoeffSingle(coeffs, Trace.PawnThreat,                       index);
    InitCoeffSingle(coeffs, Trace.PawnPushThreat,                   index);
    InitCoeffSingle(coeffs, Trace.PawnPushPinnedThreat,             index);
    InitCoeffSingle(coeffs, Trace.KnightCheckQueen,                 index);
    InitCoeffSingle(coeffs, Trace.BishopCheckQueen,                 index);
    InitCoeffSingle(coeffs, Trace.RookCheckQueen,                   index);

    // Attaques sur le roi ennemi
    InitCoeffArray1D(coeffs, Trace.SafeCheck,               index);
    InitCoeffArray1D(coeffs, Trace.UnsafeCheck,             index);
    InitCoeffArray1D(coeffs, Trace.KingAttackersWeight,     index);
    InitCoeffArray1D(coeffs, Trace.KingAttacksCount,              index);

    // Mobilité
    InitCoeffArray1D(coeffs, Trace.KnightMobility,      index);
    InitCoeffArray1D(coeffs, Trace.BishopMobility,     index);
    InitCoeffArray1D(coeffs, Trace.RookMobility,       index);
    InitCoeffArray1D(coeffs, Trace.QueenMobility,      index);



    if (index != NTERMS){
        printf("Error in InitCoefficients(): i = %d ; NTERMS = %d\n", index, NTERMS);
        exit(EXIT_FAILURE);
    }

}

//===============================================================
//! \brief  Initialisation à partir des valeurs de "evaluate.h"
//---------------------------------------------------------------
void Tuner::InitBaseParams(double tparams[NTERMS][N_PHASES])
{
    int index = 0;

#if defined PARAM_ZERO
    for (int i=0; i<NTERMS; i++)
        InitBaseSingle(tparams, 0,      index);
#else

    // Piece values
    InitBaseSingle(tparams, PawnValue,      index);
    InitBaseSingle(tparams, KnightValue,    index);
    InitBaseSingle(tparams, BishopValue,    index);
    InitBaseSingle(tparams, RookValue,      index);
    InitBaseSingle(tparams, QueenValue,     index);

    // PSQT
    InitBaseArray1D(tparams, PawnPSQT,        index);
    InitBaseArray1D(tparams, KnightPSQT,      index);
    InitBaseArray1D(tparams, BishopPSQT,      index);
    InitBaseArray1D(tparams, RookPSQT,        index);
    InitBaseArray1D(tparams, QueenPSQT,       index);
    InitBaseArray1D(tparams, KingPSQT,        index);

    // Pions
    InitBaseSingle(tparams, PawnDoubled,    index);
    InitBaseSingle(tparams, PawnDoubled2,   index);
    InitBaseSingle(tparams, PawnSupport,    index);
    InitBaseSingle(tparams, PawnOpen,       index);
    InitBaseArray1D( tparams, PawnPhalanx,    index);
    InitBaseSingle(tparams, PawnIsolated,   index);
    InitBaseArray1D( tparams, PawnPassed,     index);
    InitBaseArray1D( tparams, PassedDefended, index);

    // Pions passés
    InitBaseSingle(tparams, PassedSquare,            index);
    InitBaseArray1D( tparams, PassedDistUs,          index);
    InitBaseSingle(tparams, PassedDistThem,          index);
    InitBaseArray1D( tparams, PassedBlocked,         index);
    InitBaseArray1D( tparams, PassedFreeAdv,         index);
    InitBaseSingle(tparams, PassedRookBack,          index);

    // Divers
    InitBaseSingle(tparams, KnightBehindPawn,       index);
    InitBaseArray2D( tparams, KnightOutpost,  index);

    InitBaseSingle(tparams, BishopPair,             index);
    InitBaseSingle(tparams, BishopBadPawn,          index);
    InitBaseSingle(tparams, BishopBehindPawn,       index);
    InitBaseSingle(tparams, BishopLongDiagonal,     index);

    InitBaseArray1D( tparams, RookOnOpenFile,       index);
    InitBaseSingle(tparams, RookOnBlockedFile,      index);

    InitBaseSingle(tparams, QueenRelativePin,      index);

    // Roi
    InitBaseSingle(tparams, KingAttackPawn,         index);
    InitBaseSingle(tparams, PawnShelter,            index);

    // Menaces
    InitBaseArray1D( tparams, ThreatByKnight,            index);
    InitBaseArray1D( tparams, ThreatByBishop,            index);
    InitBaseArray1D( tparams, ThreatByRook,              index);
    InitBaseSingle(tparams, ThreatByKing,                       index);
    InitBaseSingle(tparams, HangingThreat,                      index);
    InitBaseSingle(tparams, PawnThreat,                         index);
    InitBaseSingle(tparams, PawnPushThreat,                     index);
    InitBaseSingle(tparams, PawnPushPinnedThreat,               index);
    InitBaseSingle(tparams, KnightCheckQueen,                   index);
    InitBaseSingle(tparams, BishopCheckQueen,                   index);
    InitBaseSingle(tparams, RookCheckQueen,                     index);

    // Attaques sur le roi ennemi
    InitBaseArray1D( tparams, SafeCheck,            index);
    InitBaseArray1D( tparams, UnsafeCheck,          index);
    InitBaseArray1D( tparams, KingAttackersWeight,  index);
    InitBaseArray1D( tparams, KingAttacksCount,     index);

    // Mobilité
    InitBaseArray1D( tparams, KnightMobility,       index);
    InitBaseArray1D( tparams, BishopMobility,       index);
    InitBaseArray1D( tparams, RookMobility,         index);
    InitBaseArray1D( tparams, QueenMobility,        index);


    if (index != NTERMS) {
        printf("Error 1 in InitBaseParams(): i = %d ; NTERMS = %d\n", index, NTERMS);
        exit(EXIT_FAILURE);
    }
#endif
}


double Tuner::Sigmoid(double K, double E)
{
    return 1.0 / (1.0 + exp(-K * E / 400.0));
}


double Tuner::ComputeOptimalK()
{
    const double rate = 100, delta = 1e-5, deviation_goal = 1e-6;
    double K = 2, deviation = 1;

    while (fabs(deviation) > deviation_goal)
    {
        double up   = StaticEvaluationErrors(K + delta);
        double down = StaticEvaluationErrors(K - delta);
        deviation = (up - down) / (2 * delta);
        K -= deviation * rate;

        std::cout << "up = " << up << " ; down = " << down << " ; deviation = " << deviation << " ; goal = " << deviation_goal << std::endl;
    }

    return K;

}

double Tuner::StaticEvaluationErrors(double K)
{
    // Compute the error of the dataset using the Static Evaluation.
    // We provide simple speedups that make use of the OpenMP Library.
    double total = 0.0;
#pragma omp parallel shared(total)
    {
#pragma omp for schedule(static, NPOSITIONS / NPARTITIONS) reduction(+:total)
        for (int i = 0; i < NPOSITIONS; i++)
            total += pow(positions[i].result - Sigmoid(K, positions[i].seval), 2);
    }

    return total / (double) NPOSITIONS;
}

double Tuner::LinearEvaluation(const Position& position, double params[NTERMS][N_PHASES])
{
    double midgame = MgScore(position.eval);
    double endgame = EgScore(position.eval);

    // Save any modifications for MG or EG for each evaluation type
    for (Usize i = 0; i < position.tuples.size(); i++)
    {
        int termIndex = position.tuples[i].index;
        midgame += (double) position.tuples[i].coeff * params[termIndex][MG];
        endgame += (double) position.tuples[i].coeff * params[termIndex][EG];
    }

    double eval = ( midgame * position.phase256
                   +   endgame * (256.0 - position.phase256) * position.scale ) / 256.0;

    return eval + (position.turn == WHITE ? Tempo : -Tempo);

}

void Tuner::UpdateSingleGradient(const Position& position, double gradient[NTERMS][N_PHASES], double params[NTERMS][N_PHASES], double K)
{
    double E = LinearEvaluation(position, params);
    double S = Sigmoid(K, E);
    double X = (position.result - S) * S * (1 - S);

    double mgBase = X * position.pfactors[MG];
    double egBase = X * position.pfactors[EG];

    for (Usize i = 0; i < position.tuples.size(); i++)
    {
        int    index = position.tuples[i].index;
        double coeff = position.tuples[i].coeff;

        gradient[index][MG] += mgBase * coeff;
        gradient[index][EG] += egBase * coeff * position.scale;
    }

}

void Tuner::ComputeGradient(double gradient[NTERMS][N_PHASES], double params[NTERMS][N_PHASES], double K)
{

#pragma omp parallel shared(gradient)
    {
        double local[NTERMS][N_PHASES] = { {0} };

#pragma omp for schedule(static, NPOSITIONS / NPARTITIONS)
        for (int i = 0; i < NPOSITIONS; i++)
            UpdateSingleGradient(positions[i], local, params, K);

        for (int i = 0; i < NTERMS; i++) {
            gradient[i][MG] += local[i][MG];
            gradient[i][EG] += local[i][EG];
        }
    }

}

double Tuner::TunedEvaluationErrors(double params[NTERMS][N_PHASES], double K)
{
    double total = 0.0;

#pragma omp parallel shared(total)
    {
#pragma omp for schedule(static, NPOSITIONS / NPARTITIONS) reduction(+:total)
        for (int i = 0; i < NPOSITIONS; i++)
            total += pow(positions[i].result - Sigmoid(K, LinearEvaluation(positions[i], params)), 2);
    }

    return total / (double) NPOSITIONS;
}


void Tuner::PrintParameters(double params[NTERMS][N_PHASES], double current[NTERMS][N_PHASES])
{
    int len = 8;    // nombre d'éléments par ligne
    double tparams[NTERMS][N_PHASES];

    for (int i = 0; i < NTERMS; ++i) {
        tparams[i][MG] = round(params[i][MG] + current[i][MG]);
        tparams[i][EG] = round(params[i][EG] + current[i][EG]);
    }

    int index = 0;
    puts("\n");

    PrintPieceValues(tparams, index);
    PrintPSQT(tparams, index);

    puts("\n//----------------------------------------------------------");
    puts("// Pions");
    PrintSingle("PawnDoubled",      tparams, index);
    PrintSingle("PawnDoubled2",     tparams, index);
    PrintSingle("PawnSupport",      tparams, index);
    PrintSingle("PawnOpen",         tparams, index);
    PrintArray( "PawnPhalanx",      tparams, index,     N_RANKS,    "[N_RANKS]", len);
    PrintSingle("PawnIsolated",     tparams, index);
    PrintArray( "PawnPassed",       tparams, index,     N_RANKS,    "[N_RANKS]", len);
    PrintArray( "PassedDefended",   tparams, index,     N_RANKS,    "[N_RANKS]", len);

    puts("\n//----------------------------------------------------------");
    puts("// Pions passés");
    PrintSingle("PassedSquare",     tparams, index);
    PrintArray( "PassedDistUs",     tparams, index,     N_RANKS,      "[N_RANKS]",  len);
    PrintSingle("PassedDistThem",   tparams, index);
    PrintArray( "PassedBlocked",    tparams, index,     N_RANKS,      "[N_RANKS]",  len);
    PrintArray( "PassedFreeAdv",    tparams, index,     N_RANKS,      "[N_RANKS]",  len);
    PrintSingle("PassedRookBack",   tparams, index);

    puts("\n//----------------------------------------------------------");
    puts("// Cavaliers");
    PrintSingle("KnightBehindPawn", tparams, index);
    PrintArray2D( "KnightOutpost",  tparams, index,     2,2,      "[2][2]",  len);

    puts("\n// Fous");
    PrintSingle("BishopPair",           tparams, index);
    PrintSingle("BishopBadPawn",        tparams, index);
    PrintSingle("BishopBehindPawn",     tparams, index);
    PrintSingle("BishopLongDiagonal",   tparams, index);

    puts("\n// Tours");
    PrintArray( "RookOnOpenFile",   tparams, index,     2,  "[2]",  len);
    PrintSingle("RookOnBlockedFile",tparams, index);

    puts("\n// Dames");
    PrintSingle("QueenRelativePin",tparams, index);

    puts("\n//----------------------------------------------------------");
    puts("// Roi");
    PrintSingle("KingAttackPawn",   tparams, index);
    PrintSingle("PawnShelter",      tparams, index);

    puts("\n//----------------------------------------------------------");
    puts("// Menaces");
    PrintArray( "ThreatByKnight",           tparams, index,     N_PIECES,     "[N_PIECES]", len);
    PrintArray( "ThreatByBishop",           tparams, index,     N_PIECES,     "[N_PIECES]", len);
    PrintArray( "ThreatByRook",             tparams, index,     N_PIECES,     "[N_PIECES]", len);
    PrintSingle("ThreatByKing",             tparams, index);
    PrintSingle("HangingThreat",            tparams, index);
    PrintSingle("PawnThreat",               tparams, index);
    PrintSingle("PawnPushThreat",           tparams, index);
    PrintSingle("PawnPushPinnedThreat",     tparams, index);
    PrintSingle("KnightCheckQueen",         tparams, index);
    PrintSingle("BishopCheckQueen",         tparams, index);
    PrintSingle("RookCheckQueen",           tparams, index);

    puts("\n//----------------------------------------------------------");
    puts("// Attaques sur le roi ennemi");
    PrintArray( "SafeCheck",            tparams, index,     N_PIECES,     "[N_PIECES]", len);
    PrintArray( "UnsafeCheck",          tparams, index,     N_PIECES,     "[N_PIECES]", len);
    PrintArray( "KingAttackersWeight",  tparams, index,     N_PIECES,     "[N_PIECES]", len);
    PrintArray( "KingAttacksCount",     tparams, index,     14,           "[14]",       len);

    PrintMobility(tparams, index);
    puts("");


    if (index != NTERMS) {
        printf("Error 2 in PrintParameters(): i = %d ; NTERMS = %d\n", index, NTERMS);
        exit(EXIT_FAILURE);
    }
}

void Tuner::PrintPieceValues(double params[NTERMS][N_PHASES], int& index)
{
    puts("\n//----------------------------------------------------------");
    puts("//  Valeur des pièces");
    puts("enum PieceValueEMG {");
    printf("    P_MG = %4d, P_EG = %4d,\n", (int) params[index][MG], (int) params[index][EG]); index++;
    printf("    N_MG = %4d, N_EG = %4d,\n", (int) params[index][MG], (int) params[index][EG]); index++;
    printf("    B_MG = %4d, B_EG = %4d,\n", (int) params[index][MG], (int) params[index][EG]); index++;
    printf("    R_MG = %4d, R_EG = %4d,\n", (int) params[index][MG], (int) params[index][EG]); index++;
    printf("    Q_MG = %4d, Q_EG = %4d,\n", (int) params[index][MG], (int) params[index][EG]); index++;
    printf("    K_MG = %4d, K_EG = %4d \n", 0, 0);
    puts("};");
}

void Tuner::PrintPSQT(double params[NTERMS][N_PHASES], int &index)
{
    int len = 8;

    //    puts("\n// Black's point of view - easier to read as it's not upside down");

    puts("\n//----------------------------------------------------------");
    puts("//  Bonus positionnel des pièces");
    puts("//  Du point de vue des Blancs - Mes tables sont comme ça");

    PrintArray("PawnPSQT",   params, index, N_SQUARES, "[N_SQUARES]", len);
    puts(" ");
    PrintArray("KnightPSQT", params, index, N_SQUARES, "[N_SQUARES]", len);
    puts(" ");
    PrintArray("BishopPSQT", params, index, N_SQUARES, "[N_SQUARES]", len);
    puts(" ");
    PrintArray("RookPSQT",   params, index, N_SQUARES, "[N_SQUARES]", len);
    puts(" ");
    PrintArray("QueenPSQT",  params, index, N_SQUARES, "[N_SQUARES]", len);
    puts(" ");
    PrintArray("KingPSQT",   params, index, N_SQUARES, "[N_SQUARES]", len);
}

void Tuner::PrintMobility(double params[NTERMS][N_PHASES], int& index)
{
    int len = 5;

    puts("\n//----------------------------------------------------------");
    printf("// Mobilité \n");

    PrintArray("KnightMobility", params, index,  9, "[9]",  len);
    puts(" ");
    PrintArray("BishopMobility", params, index, 14, "[14]", len);
    puts(" ");
    PrintArray("RookMobility",   params, index, 15, "[15]", len);
    puts(" ");
    PrintArray("QueenMobility",  params, index, 28, "[28]", len);
}

void Tuner::PrintSingle(const std::string& name, double params[NTERMS][N_PHASES], int& index)
{
    printf("constexpr Score %s = S(%4d, %4d);\n", name.c_str(), static_cast<int>(params[index][MG]), static_cast<int>(params[index][EG]));
    index++;
}

void Tuner::PrintArray(const std::string& name, double params[NTERMS][N_PHASES], int& index, int imax, const std::string& dim, int length)
{
    printf("constexpr Score %s%s = { ", name.c_str(), dim.c_str());

    for (int i = 0; i < imax; i++, index++)
    {
        if (i && i % length == 0)   // passage à la ligne, seulement à partir de la seconde ligne
            printf("\n    ");

        printf("S(%4d, %4d)", static_cast<int>(params[index][MG]), static_cast<int>(params[index][EG]));
        printf("%s", i == imax - 1 ? "" : ", ");    // fin de la ligne
    }

    printf("\n};\n");   // fin du tableau
}

void Tuner::PrintArray2D(const std::string& name, double params[NTERMS][N_PHASES], int& index, int imax, int jmax, const std::string& dim, int length)
{
    printf("constexpr Score %s%s = { ", name.c_str(), dim.c_str());

    for (int i=0; i<imax; i++)
    {
        if (i == 0)
            printf("  { ");
        else
            printf("\n  { ");

        for (int j=0; j<jmax; j++)
        {
            if (j && j % length == 0)
                printf("\n    ");

            printf("S(%4d, %4d)", static_cast<int>(params[index][MG]), static_cast<int>(params[index][EG]));
            printf("%s", j == jmax - 1 ? "" : ", ");

            index++;
        }
        if (i == imax-1)
            printf(" }\n};\n");
        else
            printf(" },");
    }
}

//! \brief  Initialisation du tableau tparams à partir des données
//! provenant de "evaluate.h"
void Tuner::InitBaseSingle(double tparams[NTERMS][N_PHASES], const Score data, int& index)
{
    tparams[index][MG] = static_cast<double>(MgScore(data));
    tparams[index][EG] = static_cast<double>(EgScore(data));
    index++;
}

//! \brief  Initialisation d'un tableau[imax]
// void Tuner::InitBaseArray(double tparams[NTERMS][N_PHASES], const Score* data, int imax, int& index)
// {
//     for (int i=0; i<imax; i++)
//     {
//         tparams[index][MG] = static_cast<double>(MgScore(data[i]));
//         tparams[index][EG] = static_cast<double>(EgScore(data[i]));
//         index++;
//     }
// }

//! \brief  Initialisation d'une valeur simple
//! à partir des données de Trace
void Tuner::InitCoeffSingle(std::array<Score, NTERMS>& coeffs, Score score[N_COLORS], int& index)
{
    coeffs[index++] = score[WHITE] - score[BLACK];
}

// note : en 2D, seul la première dimension peut être absente
// void Tuner::InitCoeffArray(std::array<Score, NTERMS>& coeffs, Score score[][N_COLORS], int imax, int& index)
// {
//     for (int i=0; i<imax; i++)
//         coeffs[index++] = score[i][WHITE] - score[i][BLACK];
// }

#endif

