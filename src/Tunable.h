#ifndef TUNABLE_H
#define TUNABLE_H

//  le tuning est fait en utilisant "Weather Factory"
//  https://github.com/jnlt3/weather-factory


#include <vector>
#include <string>

namespace Tunable
{
struct TunableParam
{
    explicit TunableParam(const std::string& _name, int _value, int _min, int _max, int _step);

    std::string name;
    int value, min, max, step;

    operator int() const { return value; }
};

inline std::vector<TunableParam *> params;


void setParam(const std::string &name, int value);
void paramsToSpsa();
std::string paramsToUci();
void paramsToJSON();

#if defined USE_TUNE
#define PARAM(name, value, min, max, step) inline TunableParam name(#name, value, min, max, step)
#else
#define PARAM(name, value, min, max, step) constexpr int name = value
#endif

//---------------------------------------------------------------------------
// Quiescence
//---------------------------------------------------------------------------
PARAM(DeltaPruningMargin, 1000, 900, 1100, 10);
PARAM(DeltaPruningBias,    300, 100, 400, 20);

//---------------------------------------------------------------------------
// Recherche
//---------------------------------------------------------------------------

/*
    static constexpr int FutilityMargin = 95;
    static constexpr int FutilityPruningDepth = 8;
    static constexpr int FutilityPruningHistoryLimit[] = { 12000, 6000 };

    static constexpr int ContinuationPruningDepth[] = { 3, 2 };
    static constexpr int ContinuationPruningHistoryLimit[] = { -1000, -2500 };

    static constexpr int SEEPruningDepth =  9;
    static constexpr int SEEQuietMargin = -64;
    static constexpr int SEENoisyMargin = -19;

    static constexpr int ProbCutDepth  =   5;
    static constexpr int ProbCutMargin = 100;
  */

//  RAZORING
PARAM(RazoringDepth,    3, 1, 5, 1);
PARAM(RazoringMargin, 200, 100, 200, 10);

//  STATIC NULL MOVE PRUNING ou aussi REVERSE FUTILITY PRUNING
PARAM(SNMPDepth,      6, 2, 10, 1);
PARAM(SNMPMargin,    70, 50, 100, 10);

//  NULL MOVE PRUNING
PARAM(NMPDepth,     3, 1, 5, 1);
PARAM(NMPReduction, 3, 2, 5, 1);
PARAM(NMPMargin,   32, 25, 35, 2);
PARAM(NMPMax,     384, 350, 450, 20);
PARAM(NMPDivisor, 128, 100, 150, 10);

//  ProbCut
PARAM(ProbCutDepth,     5, 1, 8, 1);
PARAM(ProbCutMargin,  100, 50, 200, 20);
PARAM(ProbcutReduction, 4, 3, 5, 1);

// Futility Pruning.
PARAM(FPMargin,                  95, 50, 150, 5);
PARAM(FPDepth,                    8, 5, 12, 1);
PARAM(FPHistoryLimit,         12000, 10000, 14000, 1000);
PARAM(FPHistoryLimitImproving, 6000, 4000, 8000, 200);

PARAM(HistoryPruningDepth,              3, 1, 5, 1);
PARAM(HistoryPruningLimit,          -1000, -2000, 0, 100);
PARAM(HistoryPruningLimitImproving,  1500, 0, 2000, 100);

// Static Exchange Evaluation Pruning
PARAM(SEEPruningDepth,  9, 5, 12, 1);
PARAM(SEEQuietMargin, -64, -100, 100, 32);
PARAM(SEENoisyMargin, -19, -50, 50, 10);

//  SINGULAR EXTENSION
PARAM(SEDepth, 8, 5, 10, 1);

//  LATE MOVE REDUCTION
PARAM(HistReductionDivisor, 5000, 3000, 8000, 200);

// History
PARAM(HistoryBonusMargin, 364,  128,  512,  32);
PARAM(HistoryBonusBias,   -66, -128,  128,  32);
PARAM(HistoryBonusMax,   1882, 1024, 4096, 256);


} // namespace Tunable

#endif // TUNABLE_H
