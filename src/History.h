#ifndef HISTORY_H
#define HISTORY_H

class History;

#include "types.h"
#include "defines.h"


// Quiet History, aussi appelée Main History : side_to_move, from, to
using MainHistoryTable        = I16[N_COLORS][N_SQUARES][N_SQUARES];

//============================================================================
//  Capture History
//      https://www.chessprogramming.org/History_Heuristic#Capture_History
//
//      Capture History, introduced by Stefan Geschwentner in 2016, is a variation on history heuristic applied on capture moves.
//      It is a history table indexed by moved piece, target square, and captured piece type.
//      The history table receives a bonus for captures that failed high, and maluses for all capture moves that did not fail high.
//      The history values is used as a replacement for LVA in MVV-LVA.
//============================================================================
using CaptureHistory           = I16[N_PIECE][N_SQUARES][N_PIECE_TYPE];     // [moved piece][target square][captured piece type]


//============================================================================
// Move Ordering : Countermove Heuristic
// This heuristic assumes that many moves have a "natural" response
//  indexed by [from][to] or by [piece][to] of the previous move
//============================================================================
using CounterMoveTable         = MOVE[N_PIECE][N_SQUARES];


//============================================================================
//  Continuation History
/*
 * Continuation History is a generalization of Counter Moves History and Follow Up History.
 * An n-ply Continuation History is the history score indexed by the move played n-ply ago and the current move.
 * 1-ply and 2-ply continuation histories are most popular and correspond to Counter Moves History
 * and Follow Up History respectively.
 * Many programs, notably Stockfish, also makes use of 3, 4, 5, and 6-ply continuation histories.
 */
//============================================================================
using ContinuationHistoryTable = I16[N_PIECE][N_SQUARES][N_PIECE][N_SQUARES];


class History
{
public:
    History();
    void reset();

    ContinuationHistoryTable continuation_history {{{{0}}}};

//--------------------------------------------

    MOVE get_counter_move(const SearchInfo *info) const;

    I16  get_capture_history(MOVE move) const;
    void update_capture_history(MOVE best_move, I16 depth,
                                         int capture_count, std::array<MOVE, MAX_MOVES>& capture_moves);

    int  get_quiet_history(Color color, const SearchInfo *info, const MOVE move, int &cm_hist, int &fm_hist) const;
    void update_quiet_history(Color color, SearchInfo *info, MOVE move, I16 depth,
                              int quiet_count, std::array<MOVE, MAX_MOVES>& quiet_moves);


private:

    static constexpr int MAX_HISTORY    = 16384;

//----------------------------------------------------
    void gravity(I16 *entry, int bonus);
    int  stat_bonus(int depth);
    int  stat_malus(int depth);

    //*********************************************************************
    //  Données initialisées une seule fois au début d'une nouvelle partie


    // tableau donnant le bonus/malus d'un coup quiet ayant provoqué un cutoff
    // bonus main_history [Color][From][Dest]
    MainHistoryTable  main_history = {{{0}}};

    // tableau des coups qui ont causé un cutoff au ply précédent
    // counter_move[opposite_color][piece][dest]
    CounterMoveTable counter_move = {{0}};

    // capture history  : [moved piece][target square][captured piece type]
    CaptureHistory capture_history = {{{0}}};


};

#endif // HISTORY_H
