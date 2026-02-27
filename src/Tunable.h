#ifndef TUNABLE_H
#define TUNABLE_H

//  Tuning can be done with OpenBench (SPSA), Weather Factory, or chess-tuning-tools
//  Compile with -DUSE_TUNING to expose parameters via UCI

//  https://github.com/jnlt3/weather-factory


#include <vector>
#include <string>

namespace Tunable
{
struct TunableParam
{
    explicit TunableParam(const std::string& _name, int _value, int _min, int _max);

    std::string name;
    int value, min, max, step;

    operator int() const { return value; }
};

inline std::vector<TunableParam *> params;


void setParam(const std::string &name, int value);
std::string paramsToSpsa();
std::string paramsToUci();
void paramsToJSON();

#if defined USE_TUNING
#define PARAM(name, value, min, max) inline TunableParam name(#name, value, min, max)
#else
#define PARAM(name, value, min, max) constexpr int name = value
#endif

// Quiescence
PARAM(DeltaPruningBias,    300, 100, 400);

// Aspiration Window
PARAM(AspirationWindowsDepth,    6, 3, 10);
PARAM(AspirationWindowsInitial, 12, 5, 30);
PARAM(AspirationWindowsDelta,   16, 8, 40);
PARAM(AspirationWindowsExpand, 6667, 4000, 9000);

// LMR Table (valeurs x100)
PARAM(LMR_CaptureBase,      0,   0, 150);
PARAM(LMR_CaptureDivisor, 325, 200, 500);
PARAM(LMR_QuietBase,      175, 100, 250);
PARAM(LMR_QuietDivisor,   225, 150, 400);

//  LATE MOVE REDUCTION
PARAM(LMR_HistReductionDivisor, 5000, 3000, 8000);
PARAM(LMR_DeeperMargin, 43, 20, 60);
PARAM(LMR_DeeperScale, 2, 0, 10);
PARAM(LMR_ShallowerMargin, 11, 0, 20);

//  RAZORING
PARAM(RazoringDepth,    3, 1, 5);
PARAM(RazoringMargin, 200, 100, 300);

//  STATIC NULL MOVE PRUNING ou aussi REVERSE FUTILITY PRUNING
PARAM(SNMPDepth,      6, 2, 10);
PARAM(SNMPMargin,    70, 50, 100);

//  NULL MOVE PRUNING
PARAM(NMPDepth,     3, 1, 5);
PARAM(NMPReduction, 3, 1, 6);
PARAM(NMPMargin,   32, 16, 48);
PARAM(NMPMax,     384, 350, 450);
PARAM(NMPDivisor, 128, 64, 192);

//  ProbCut
PARAM(ProbCutDepth,     5, 1, 8);
PARAM(ProbCutMargin,  100, 50, 200);
PARAM(ProbcutReduction, 4, 2, 6);

// Futility Pruning.
PARAM(FPMargin,                  95, 50, 150);
PARAM(FPDepth,                    8, 5, 12);
PARAM(FPHistoryLimit,         12000, 8000, 16000);
PARAM(FPHistoryLimitImproving, 6000, 2000, 10000);

PARAM(HistoryPruningDepth,     3, 1, 5);
PARAM(HistoryPruningLimit,  4500, 0, 8000);

// Static Exchange Evaluation Pruning
PARAM(SEEPruningDepth,  9, 5, 12);
PARAM(SEEQuietMargin, -64, -128, 0);
PARAM(SEENoisyMargin, -19, -50, 50);

//  SINGULAR EXTENSION
PARAM(SEDepth, 8, 5, 10);

// History
PARAM(HistoryBonusMargin, 364,  128,  512);
PARAM(HistoryBonusBias,   -66, -128,  128);
PARAM(HistoryBonusMax,   1882, 1024, 4096);


} // namespace Tunable

#endif // TUNABLE_H
