#ifndef THREADDATA_H
#define THREADDATA_H

class ThreadData;

#include <thread>
#include "defines.h"
#include "types.h"

constexpr int STACK_OFFSET = 4;
constexpr int STACK_SIZE   = MAX_PLY + 2*STACK_OFFSET;  // taille un peu trop grande, mais multiple de 8

struct SearchInfo {
    MOVE        killer1;    // killer moves
    MOVE        killer2;
    MOVE        excluded;   // coup à éviter
    int         eval;       // évaluation statique
    MOVE        move;       // coup cherché
    int         ply;        // profondeur de recherche
    PVariation  pv;         // Principale Variation
}__attribute__((aligned(64)));

//! \brief  Données d'une thread
class ThreadData
{
public:
    explicit ThreadData();

    void create(int i);
    void reset();
    void init();

    void update_pv(SearchInfo* si, const MOVE move) const;
    void update_history(Color color, MOVE move, int bonus);
    int  get_history(const Color color, const MOVE move) const;
    MOVE get_counter_move(Color oppcolor, int ply) const;
    void update_counter_move(Color oppcolor, int ply, MOVE move);
    void update_killers(SearchInfo *si, MOVE move);
    void update_counter_history(int ply, MOVE move, int bonus);
    MOVE get_counter_history(int ply, MOVE move) const;


    SearchInfo* info;
    std::thread thread;
    U64         nodes;
    U64         tbhits;
    int         index;
    MOVE        best_move;
    int         best_score;
    int         best_depth;
    int         score;
    int         depth;
    int         seldepth;
    bool        stopped;

    SearchInfo  _info[STACK_SIZE];

    // tableau donnant le bonus/malus d'un coup quiet ayant provoqué un cutoff
    // bonus history [Color][From][Dest]
    I16    history[N_COLORS][N_SQUARES][N_SQUARES];

    // tableau des coups qui ont causé un cutoff au ply précédent
    // cm_table[opposite_color][piece][dest]
    MOVE   cm_table[N_COLORS][N_PIECES][N_SQUARES];

    // counter_move history
    I16 cm_history[N_PIECES][N_SQUARES][N_PIECES][N_SQUARES];

}__attribute__((aligned(64)));

#endif // THREADDATA_H
