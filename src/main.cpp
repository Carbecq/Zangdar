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
    std::cout << "main 1 " << std::endl;

    Attacks::init_masks();
    std::cout << "main 2 " << std::endl;

    //  Benchmark
    if (argCount > 1 && strcmp(argValue[1], "bench") == 0)
    {
        std::cout << "main 3 " << std::endl;
        Uci* uci = new Uci();
        uci->bench(argCount, argValue);
    }
    else
    {
#if defined USE_TUNER
        std::cout << "main 4 " << std::endl;

        ownTuner.runTexelTuning();
#else
        std::cout << "main 5 " << std::endl;

        Uci uci;
        std::cout << "main 6 " << std::endl;

        uci.run();
#endif
    }
    std::cout << "main fin " << std::endl;

    return 0;
}
