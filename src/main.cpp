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
    init_bitmasks();
    Attacks::init_masks();

    U08 tt_date     = 0;        // 0 to 255
    U08 entry_date  = 192;
    I08 entry_depth = 0;
    int age;
    int k;

    age = ((tt_date - entry_date) & 255) * 256 + 255 - entry_depth;
    std::cout << "age = " << age << std::endl;

    tt_date     = 64;        // 0 to 255
    entry_date  = 0;

    age = ((tt_date - entry_date) & 255) * 256 + 255 - entry_depth;
    std::cout << "age = " << age << std::endl;

    int m = 4;

    for (int i=0; i<=256; i+=1)
    {
        k = i;
        if (i == -256)
            k = -255;
        if (i == 256)
            k = 255;
            printf("------------------- delta=%d  \n", k);
  //          age = ((tt_date - entry_date) & 255) * 256 + 255 - entry_depth;
            // age = ((k) & 255) * 256 + 255 - entry_depth;
            age = ((k) & 255) * m + 255 - entry_depth;

            std::cout << "&255 = " <<  ((k) & 255) << std::endl;
            std::cout << "*256 = " <<  ((k) & 255)*m << std::endl;
            std::cout << "+255 = " <<  ((k) & 255)*m+255 << std::endl;

            std::cout << "age = " << age << std::endl;
    }

#if defined USE_TUNER
    ownTuner.runTexelTuning();
#else
    Uci* uci = new Uci();
    uci->run();
#endif

    return 0;
}
