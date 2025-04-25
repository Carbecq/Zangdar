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
class Board;

//! \brief  Données d'une thread
class ThreadData
{
public:
    ThreadData();

    std::thread thread;
    Search*     search;     // pointeur sur la classe de recherche
    History     history;

    int         index;
    int         depth;
    int         seldepth;
    int         score;
    U64         nodes;
    bool        stopped;
    U64         tbhits;

    MOVE        best_move;
    int         best_score;
    int         best_depth;

    void update_pv(SearchInfo* si, const MOVE move) const;

private:

}__attribute__((aligned(64)));


#endif // THREADDATA_H
