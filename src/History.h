#ifndef HISTORY_H
#define HISTORY_H


class History;

#include "types.h"
#include "defines.h"
#include "Move.h"
#include "Tunable.h"
#include "Board.h"

// Quiet History, aussi appelée Main History : side_to_move, from, to
using MainHistoryTable = I16[N_COLORS][N_SQUARES][N_SQUARES];

//============================================================================
//  Capture History
//      https://www.chessprogramming.org/History_Heuristic#Capture_History
//
//      Capture History, introduced by Stefan Geschwentner in 2016, is a variation on history heuristic applied on capture moves.
//      It is a history table indexed by moved piece, target square, and captured piece type.
//      The history table receives a bonus for captures that failed high, and maluses for all capture moves that did not fail high.
//      The history values is used as a replacement for LVA in MVV-LVA.
//============================================================================
using CaptureHistory = I16[N_PIECE][N_SQUARES][N_PIECE_TYPE];     // [moved piece][target square][captured piece type]


//============================================================================
// Move Ordering : Countermove Heuristic
// This heuristic assumes that many moves have a "natural" response
//  indexed by [from][to] or by [piece][to] of the previous move
//============================================================================
using CounterMoveTable = MOVE[N_PIECE][N_SQUARES];


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

//============================================================================
//  Correction History
//
// Static Evaluation Correction History, also known as Correction History or CorrHist for short.
// It records the difference between static evaluation and search score of a position to a table
// indexed by the corresponding board feature, then uses the difference to adjust future
// static evaluations in positions with the same feature
//============================================================================
using PawnCorrectionHistoryTable        = int[N_COLORS][PAWN_HASH_SIZE];
using MaterialCorrectionHistoryTable    = int[N_COLORS][PAWN_HASH_SIZE];



class History
{
public:
    History();
    void reset();
    inline I16 get_main_history(Color color, MOVE move) const {
        return main_history[color][Move::from(move)][Move::dest(move)];
    }

    ContinuationHistoryTable continuation_history {{{{0}}}};

//--------------------------------------------

    MOVE get_counter_move(const SearchInfo *info) const;

    inline I16 get_capture_history(MOVE move) const {
        return capture_history[static_cast<U32>(Move::piece(move))][Move::dest(move)][static_cast<U32>(Move::captured_type(move))];
    }
    void update_capture_history(MOVE best_move, I16 depth,
                                         int capture_count, std::array<MOVE, MAX_MOVES>& capture_moves);

    int  get_quiet_history(Color color, const SearchInfo *info, const MOVE move, int &cm_hist, int &fm_hist) const;
    void update_quiet_history(Color color, SearchInfo *info, MOVE move, I16 depth,
                              int quiet_count, std::array<MOVE, MAX_MOVES>& quiet_moves);


    void update_correction_history(const Board &board, int depth, int best_score, int static_eval);

    int correct_eval(const Board& board, int raw_eval);


private:

    static constexpr int MAX_HISTORY = 16384;
    static constexpr int CorrectionHistoryScale = 256;
    static constexpr int CorrectionHistoryMax   = 256 * 32;

//----------------------------------------------------
    //=====================================================
    //  Ajoute un bonus à l'historique
    //      history gravity
    //-----------------------------------------------------
    inline void gravity(I16* entry, int bonus)
    {
        // Formulation du WIKI :
        // int clamped_bonus = std::clamp(bonus, -MAX_HISTORY, MAX_HISTORY);
        // entry += clamped_bonus - (*entry) * abs(clamped_bonus) / MAX_HISTORY;

        *entry += bonus - *entry * abs(bonus) / MAX_HISTORY;
    }

    inline int stat_bonus(int depth)
    {
        return std::min<int>(Tunable::HistoryBonusMax, depth*Tunable::HistoryBonusMargin - Tunable::HistoryBonusBias);
    }

    inline int stat_malus(int depth)
    {
        return -stat_bonus(depth);
    }


    void update_main(Color color, MOVE move, int bonus);
    void update_cont(SearchInfo* info, MOVE move, int bonus);
    void update_capt(MOVE move, int malus);
    void update_correction(int& entry, int scaled_bonus, int weight);


    //*********************************************************************
    //  Données initialisées une seule fois au début d'une nouvelle partie


    // tableau donnant le bonus/malus d'un coup quiet ayant provoqué un cutoff
    MainHistoryTable  main_history = {{{0}}};

    // tableau des coups qui ont causé un cutoff au ply précédent
    CounterMoveTable counter_move = {{0}};

    // capture history  : [moved piece][target square][captured piece type]
    CaptureHistory capture_history = {{{0}}};

    // Correction History
    PawnCorrectionHistoryTable     pawn_correction_history = {{0}};
    MaterialCorrectionHistoryTable material_correction_history[N_COLORS] = {{{0}}};
};

#endif // HISTORY_H
