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
bool verifParam(const std::string &name, int value, int min, int max, int step);

#if defined USE_TUNING
#define PARAM(name, value, min, max) inline TunableParam name(#name, value, min, max)
#else
#define PARAM(name, value, min, max) constexpr int name = value
#endif

//----------------------------------------------------- LMR



// LMR Table (valeurs x100)
PARAM(LMR_CaptureBase,      0, -100, 200);   // base -1.0 à +2.0
PARAM(LMR_CaptureDivisor, 325,  200, 500);
PARAM(LMR_QuietBase,      175,   75, 300);
PARAM(LMR_QuietDivisor,   225,  150, 400);

//  LATE MOVE REDUCTION
PARAM(LMR_HistReductionDivisor, 5000, 3000, 8000);
PARAM(LMR_DeeperMargin, 43, 20, 60);
PARAM(LMR_DeeperScale, 2, 0, 5);
PARAM(LMR_ShallowerMargin, 11, 0, 20);

//----------------------------------------------------- Pruning
// Futility Pruning.
PARAM(FPMargin,                  95, 50, 150);
PARAM(FPDepth,                    8, 5, 12);
PARAM(FPHistoryLimit,         12000, 8000, 16000);
PARAM(FPHistoryLimitImproving, 6000, 2000, 10000);

// Static Exchange Evaluation Pruning
PARAM(SEEPruningDepth,  9, 5, 12);
PARAM(SEEQuietMargin, -64, -128, 0);
PARAM(SEENoisyMargin, -19, -50, 50);

// History Pruning
PARAM(HistoryPruningDepth,     3,   1,    6);
PARAM(HistoryPruningScale,  4500, 2000, 6000);

//------------------------------------------------------ NMP
//  NULL MOVE PRUNING
PARAM(NMPDepth,     3, 1, 6);
PARAM(NMPReduction, 3, 1, 6);
PARAM(NMPMargin,   32, 16, 48);
PARAM(NMPMax,     384, 350, 450);
PARAM(NMPDivisor, 128, 64, 192);

//  ProbCut
PARAM(ProbCutDepth,     5, 1, 8);
PARAM(ProbCutMargin,  100, 50, 200);
PARAM(ProbcutReduction, 4, 2, 7);

//  RAZORING
PARAM(RazoringDepth,    3, 1, 6);
PARAM(RazoringMargin, 200, 100, 300);

//  STATIC NULL MOVE PRUNING ou aussi REVERSE FUTILITY PRUNING
PARAM(SNMPDepth,                6,   2,  10);
PARAM(SNMPMargin,              70,  50, 100);
PARAM(SNMPCorrplexityScale,    60,   0, 128);

//-------------------------------------------- Bonus de l'history
// History
PARAM(HistoryBonusScale,    364,  128,  512);
PARAM(HistoryBonusOffset,   -66, -128,  128);
PARAM(HistoryBonusMax,     1882, 1024, 4096);

PARAM(HistoryMalusScale,    364,  128,  512);
PARAM(HistoryMalusOffset,   -66, -128,  128);
PARAM(HistoryMalusMax,     1882, 1024, 4096);

//-------------------------------------------- Timer

//   Peut-on tuner les valeurs du Timer ??
//   But be sure to tune especially time management parameters only at longer time controls
//   TC type 40/4 ou 10+0.1 minimum
// Timer 1, valeurs les plus critiques : définissent l'allocation de base
PARAM(softTimeScale  ,  57,  20,  80);      // valeur * 100
PARAM(hardTimeScale  ,  62,  20,  80);      // valeur * 100
PARAM(baseTimeScale  ,  20,  10,  40);      // valeur simple
// Timer 2, pondération de l'incrément et du node TM
PARAM(incrementScale ,  83,  40, 100);      // valeur * 100
PARAM(nodeTMBase     , 145, 100, 200);      // valeur * 100
PARAM(nodeTMScale    , 167, 100, 200);      // valeur * 100

//-------------------------------------------- Autres

// Aspiration Window
PARAM(AspirationWindowsDepth,    6, 3, 10);     // 6
PARAM(AspirationWindowsInitial, 12, 5, 30); // 12
PARAM(AspirationWindowsDelta,   16, 8, 40);     // 16
PARAM(AspirationWindowsExpand, 6667, 4000, 9000);

// Quiescence
PARAM(DeltaPruningBias,    300, 100, 400);

//  SINGULAR EXTENSION
PARAM(SEDepth, 8, 5, 10);

















} // namespace Tunable

#endif // TUNABLE_H
