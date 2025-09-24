#ifndef THREADDATA_H
#define THREADDATA_H

class ThreadData;

#include <cstring>
#include <thread>
#include "defines.h"
#include "types.h"
#include "History.h"

constexpr int STACK_OFFSET = 4;
constexpr int STACK_SIZE   = MAX_PLY + 2*STACK_OFFSET;  // taille un peu trop grande, mais multiple de 8


// Déclarations pour le compilateur
class Search;

//! \brief  Données d'une thread
class ThreadData
{
public:
    ThreadData();

    std::thread thread;
    Search*     search;     // pointeur sur la classe de recherche
    U64         nodes;
    U64         tbhits;
    History     history;

    int         index;
    int         depth;
    int         seldepth;
    int         score;
    bool        stopped;

    MOVE        best_move;
    int         best_score;
    int         best_depth;

private:

}__attribute__((aligned(64)));


#endif // THREADDATA_H
