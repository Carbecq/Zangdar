#include <cstring>
#include <string>
#include "Uci.h"
#include "TranspositionTable.h"
#include "ThreadPool.h"
#include "PolyBook.h"
#include "Attacks.h"
#include "DataGen.h"

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
    //  appel : Zangdar bench [depth] [nbr_threads] [hash_size]
    //          les arguments sont optionnels
    if (argCount > 1 && strcmp(argValue[1], "bench") == 0)
    {
        Uci* uci = new Uci();
        uci->bench(argCount, argValue);
    }

    //  DataGen
    //  appel : Zangdar datagen <nbr_threads> <max_games> <output_dir>
#if defined GENERATE
    else if (argCount > 1 && strcmp(argValue[1], "datagen") == 0)
    {
        DataGen(std::stoi(std::string{argValue[2]}), std::stoi(std::string{argValue[3]}), std::string{argValue[4]});
    }
#endif

    //  UCI
    else
    {
#if defined USE_TUNER
        ownTuner.runTexelTuning();
#else
        Uci uci;
        uci.run();
#endif
    }

    return 0;
}
