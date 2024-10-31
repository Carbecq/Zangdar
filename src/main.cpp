#include <cstring>
#include "Uci.h"
#include "TranspositionTable.h"
#include "ThreadPool.h"
#include "PolyBook.h"
#include "Attacks.h"

// Globals
TranspositionTable  transpositionTable(HASH_SIZE);
PolyBook            ownBook;
ThreadPool          threadPool(1, false, true);

#if defined USE_TUNER
#include "Tuner.h"
Tuner               ownTuner;
#endif

extern void init_bitmasks();

int main(int argCount, char* argValue[])
{
    Attacks::init_masks();

    //  Benchmark
    if (argCount > 1 && strcmp(argValue[1], "bench") == 0)
    {
        Uci* uci = new Uci();
        uci->bench(argCount, argValue);
    }
    else
    {
#if defined USE_TUNER
        ownTuner.runTexelTuning();
#else
        Uci* uci = new Uci();
        uci->run();
#endif
    }

    return 0;
}
