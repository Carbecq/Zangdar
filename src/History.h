#ifndef HISTORY_H
#define HISTORY_H


class History;

#include "types.h"
#include "defines.h"
#include "Move.h"
#include "Tunable.h"
#include "Board.h"
#include "SearchInfo.h"

// Quiet History, aussi appelée Main History, ou Butterfly History :
//  side_to_move, threat, threat, from, to
using MainHistoryTable = I16[N_COLORS][2][2][N_SQUARES][N_SQUARES];

//============================================================================
//  Pawn History
//      pawn_history[pawn_key & mask][piece][dest]
//============================================================================
static constexpr int PAWNHIST_MASK = PAWN_HASH_SIZE - 1;

using PawnHistoryTable = I16[PAWN_HASH_SIZE][N_PIECE][N_SQUARES];

//============================================================================
//  Capture History
//      https://www.chessprogramming.org/History_Heuristic#Capture_History
//
//      Capture History, introduced by Stefan Geschwentner in 2016, is a variation on history heuristic applied on capture moves.
//      It is a history table indexed by moved piece, target square, and captured piece type.
//      The history table receives a bonus for captures that failed high, and maluses for all capture moves that did not fail high.
//      The history values is used as a replacement for LVA in MVV-LVA.
//
//      Indexée par threat_from / threat_to pour distinguer
//      les captures fuyant ou entrant dans une case attaquée.
//============================================================================
using CaptureHistory = I16[N_PIECE][2][2][N_SQUARES][N_PIECE_TYPE];     // [moved piece][threat from][threat to][target square][captured piece type]


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
//  Enregistre l'écart moyen entre éval statique (NNUE) et score de recherche,
//  indexé par feature de position : pawn structure, et non-pawn structure
//  par couleur. corrected_eval = raw_eval + corr_pawn + corr_npw + corr_npb.
//
//  Affecte toutes les décisions basées sur l'éval statique :
//  SNMP/RFP, NMP, razoring, futility pruning, LMR, singular extension.
//
//  Mise à jour (gravity formula) :
//    change = clamp(eval_diff * depth * scale / 64, ±max_value/4)
//    entry += change - entry * |change| / max_value
//  → bonus proportionnel à la MAGNITUDE de l'écart (pas juste sa fréquence),
//    saturation auto à ±max_value.
//
//============================================================================
static constexpr int CORRHIST_MASK = CORR_HASH_SIZE - 1;    // CORR_HASH_SIZE est une puissance de 2

using PawnCorrectionHistoryTable       = I16[N_COLORS][CORR_HASH_SIZE];
using NonPawnCorrectionHistoryTable    = I16[N_COLORS][CORR_HASH_SIZE];


class History
{
public:
    History();
    void reset();
    inline I16 get_main_history(Color color, const SearchInfo *info, MOVE move) const {
        SQUARE from = Move::from(move);
        SQUARE dest = Move::dest(move);
        return main_history[color][BB::test_bit(info->threats, from)][BB::test_bit(info->threats, dest)][from][dest];
    }
    inline I16 get_pawn_history(const Board& board, MOVE move) const {
        return pawn_history[board.get_pawn_key() & PAWNHIST_MASK][Move::piece(move)][Move::dest(move)];
    }

    ContinuationHistoryTable continuation_history {{{{0}}}};

    //--------------------------------------------

    MOVE get_counter_move(const SearchInfo *info) const;

    inline I16 get_capture_history(const SearchInfo *info, MOVE move) const {
        const SQUARE from = Move::from(move);
        const SQUARE dest = Move::dest(move);
        return capture_history[Move::piece(move)]
                [BB::test_bit(info->threats, from)]
                [BB::test_bit(info->threats, dest)]
                [dest][Move::captured_type(move)];
    }
    void update_capture_history(const SearchInfo *info, MOVE best_move, I16 depth,
                                size_t capture_count, std::array<MOVE, MAX_MOVES>& capture_moves);

    int  get_quiet_history(Color color, const SearchInfo *info, const MOVE move, KEY pawnkey) const;
    void update_quiet_history(Color color, SearchInfo *info, MOVE move, KEY pawnkey, I16 depth,
                              size_t quiet_count, std::array<MOVE, MAX_MOVES>& quiet_moves);

    void update_pawn(KEY pawnkey, MOVE move, int bonus);

    void update_correction_history(const Board &board, int depth, int best_score, int static_eval);
    void update_continuation_history(SearchInfo* info, MOVE move, int score, int alpha, int beta, int depth);

    int corrected_eval(const Board& board, int raw_eval);


private:

    static constexpr int MAX_HISTORY = 16384;

    //----------------------------------------------------
    //=====================================================
    //  Ajoute un bonus à l'historique
    //      history gravity
    //-----------------------------------------------------
    inline void gravity(I16& entry, int bonus)
    {
        // Formulation du WIKI :
        // int clamped_bonus = std::clamp(bonus, -MAX_HISTORY, MAX_HISTORY);
        // entry += clamped_bonus - (*entry) * abs(clamped_bonus) / MAX_HISTORY;

        entry += bonus - static_cast<int>(entry) * abs(bonus) / MAX_HISTORY;
    }

    inline int stat_bonus(int depth)
    {
        return std::min<int>(Tunable::HistoryBonusMax, depth*Tunable::HistoryBonusScale - Tunable::HistoryBonusOffset);
    }

    inline int stat_malus(int depth)
    {
        return -std::min<int>(Tunable::HistoryMalusMax, depth*Tunable::HistoryMalusScale - Tunable::HistoryMalusOffset);
    }

    void update_main(Color color, SearchInfo *info, MOVE move, int bonus);
    void update_continuation(SearchInfo* info, MOVE move, int bonus);
    void update_capture(const SearchInfo *info, MOVE move, int delta);
    void update_correction(I16& entry, int eval_diff, int depth, int scale, int correction_max);

    //*********************************************************************
    //  Données initialisées une seule fois au début d'une nouvelle partie


    // tableau donnant le bonus/malus d'un coup quiet ayant provoqué un cutoff
    MainHistoryTable  main_history = {{{{{0}}}}};

    //
    PawnHistoryTable pawn_history = {{{0}}};


    // tableau des coups qui ont causé un cutoff au ply précédent
    CounterMoveTable counter_move = {{Move::MOVE_NONE}};

    // capture history  : [moved piece][threat from][threat to][target square][captured piece type]
    CaptureHistory capture_history = {{{{{0}}}}};

    // Correction History
    // Utilisé pour la correction de l'évaluation
    PawnCorrectionHistoryTable    pawn_correction_history = {{0}};
    NonPawnCorrectionHistoryTable non_pawn_correction_history[N_COLORS] = {{{0}}};
};

#endif // HISTORY_H
