#ifndef THREADDATA_H
#define THREADDATA_H

class ThreadData;

#include <cstring>
#include <thread>
#include "types.h"
#include "defines.h"

constexpr int STACK_OFFSET = 4;
constexpr int STACK_SIZE   = MAX_PLY + 2*STACK_OFFSET;  // taille un peu trop grande, mais multiple de 8

//! \brief  Données initialisées à chaque début de recherche
struct SearchInfo {
    MOVE        excluded{};   // coup à éviter
    int         eval{};       // évaluation statique
    MOVE        move{};       // coup cherché
    int         ply{};        // profondeur de recherche
    PVariation  pv{};         // Principale Variation
    int         doubleExtensions{};
}__attribute__((aligned(64)));


// Déclarations pour le compilateur
class Search;
class Board;

//! \brief  Données d'une thread
class ThreadData
{
public:
    ThreadData();
    void reset();

    SearchInfo  _info[STACK_SIZE];
    std::thread thread;
    Search*     search;     // pointeur sur la classe de recherche
    SearchInfo* info;       // pointeur décalé de STACK_OFFSET sur la tableau total _info[STACK_SIZE]

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

    //*********************************************************************
    //  Données initialisées une seule fois au début d'une nouvelle partie

    KillerTable killer1;    // Killer Moves
    KillerTable killer2;

    // tableau donnant le bonus/malus d'un coup quiet ayant provoqué un cutoff
    // bonus history [Color][From][Dest]
    Historytable  history;

    // tableau des coups qui ont causé un cutoff au ply précédent
    // counter_move[opposite_color][piece][dest]
    CounterMoveTable counter_move;

    // counter_move history [piece][dest]
    CounterMoveHistoryTable  counter_move_history;

    // [piece][dest][piece][dest]
    FollowupMoveHistoryTable  followup_move_history;

    // capture history
 //   int capture_history[N_PIECES][N_SQUARES][N_PIECES] = {{{0}}};

    void update_pv(SearchInfo* si, const MOVE move) const;
    void update_killers(int ply, MOVE move);
    int  get_history(Color color, const MOVE move) const;
    void update_history(Color color, MOVE move, int bonus);
    MOVE get_counter_move(Color oppcolor, int ply) const;
    void update_counter_move(Color oppcolor, int ply, MOVE move);
    int  get_counter_move_history(int ply, MOVE move) const;
    void update_counter_move_history(int ply, MOVE move, int bonus);
    int  get_followup_move_history(int ply, MOVE move) const;
    void update_followup_move_history(int ply, MOVE move, int bonus);


}__attribute__((aligned(64)));


#endif // THREADDATA_H
