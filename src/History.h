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
//      Capture History, introduite par Stefan Geschwentner en 2016, est une variante de l'history
//      heuristic appliquée aux coups de capture.
//      C'est une table d'historique indexée par la pièce jouée, la case d'arrivée, et le type de
//      pièce capturée.
//      La table reçoit un bonus pour les captures qui ont fail high, et des malus pour toutes les
//      captures qui n'ont pas fail high.
//      Les valeurs de l'historique remplacent LVA dans MVV-LVA.
//
//      Indexée par threat_from / threat_to pour distinguer
//      les captures fuyant ou entrant dans une case attaquée.
//============================================================================
using CaptureHistory = I16[N_PIECE][2][2][N_SQUARES][N_PIECE_TYPE];     // [pièce jouée][threat from][threat to][case d'arrivée][type de pièce capturée]


//============================================================================
// Move Ordering : Countermove Heuristic
// Cette heuristique suppose que beaucoup de coups ont une réponse "naturelle",
//  indexée par [from][to] ou par [piece][to] du coup précédent
//============================================================================
using CounterMoveTable = MOVE[N_PIECE][N_SQUARES];


//============================================================================
//  Continuation History
/*
 * Continuation History généralise Counter Moves History et Follow Up History.
 * Une Continuation History à n plies est le score d'historique indexé par le coup joué
 * n plies plus tôt et le coup courant.
 * Les continuation histories à 1 et 2 plies sont les plus utilisées et correspondent
 * respectivement à Counter Moves History et Follow Up History.
 * Zangdar cumule aussi la profondeur 4 plies (voir les 3 lignes ci-dessous) : elle capture
 * le motif « ce coup répond bien au coup joué par le même camp il y a deux coups complets ».
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

    //============================================================================
    //! \brief  Retourne le score de main history (butterfly) d'un coup
    //! \param[in]  color   camp qui joue
    //! \param[in]  info    recherche actuelle
    //! \param[in]  move    coup quiet évalué
    //!
    //! \return Score de main history, indexé par threat from/to et from/to du coup
    //----------------------------------------------------------------------------
    inline I16 get_main_history(Color color, const SearchInfo *info, MOVE move) const {
        SQUARE from = Move::from(move);
        SQUARE dest = Move::dest(move);
        return main_history[color][BB::test_bit(info->threats, from)][BB::test_bit(info->threats, dest)][from][dest];
    }

    //============================================================================
    //! \brief  Retourne le score de pawn history d'un coup
    //! \param[in]  board   échiquier courant
    //! \param[in]  move    coup quiet évalué
    //!
    //! \return Score de pawn history, indexé par clé de pions/pièce/case d'arrivée
    //----------------------------------------------------------------------------
    inline I16 get_pawn_history(const Board& board, MOVE move) const {
        return pawn_history[board.get_pawn_key() & PAWNHIST_MASK][Move::piece(move)][Move::dest(move)];
    }

    ContinuationHistoryTable continuation_history {{{{0}}}};

    //--------------------------------------------

    MOVE get_counter_move(const SearchInfo *info) const;

    //============================================================================
    //! \brief  Retourne le score de capture history d'un coup
    //! \param[in]  info    recherche actuelle
    //! \param[in]  move    coup de capture évalué
    //!
    //! \return Score de capture history, indexé par pièce/threat from/to/case/pièce capturée
    //----------------------------------------------------------------------------
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

    //=====================================================
    //! \brief  Ajoute un bonus à l'historique (history gravity)
    //! \param[in,out]  entry   entrée de la table à mettre à jour
    //! \param[in]      bonus   bonus (positif) ou malus (négatif)
    //-----------------------------------------------------
    inline void gravity(I16& entry, int bonus)
    {
        // Formulation du WIKI :
        // int clamped_bonus = std::clamp(bonus, -MAX_HISTORY, MAX_HISTORY);
        // entry += clamped_bonus - (*entry) * abs(clamped_bonus) / MAX_HISTORY;

        entry += bonus - static_cast<int>(entry) * abs(bonus) / MAX_HISTORY;
    }

    //=====================================================
    //! \brief  Calcule le bonus d'historique en fonction de la profondeur
    //! \param[in]  depth   profondeur de recherche du nœud
    //!
    //! \return Bonus, plafonné à Tunable::HistoryBonusMax
    //-----------------------------------------------------
    inline int stat_bonus(int depth)
    {
        return std::min<int>(Tunable::HistoryBonusMax, depth*Tunable::HistoryBonusScale - Tunable::HistoryBonusOffset);
    }

    //=====================================================
    //! \brief  Calcule le malus d'historique en fonction de la profondeur
    //! \param[in]  depth   profondeur de recherche du nœud
    //!
    //! \return Malus (négatif), plafonné en valeur absolue à Tunable::HistoryMalusMax
    //-----------------------------------------------------
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

    // capture history  : [pièce jouée][threat from][threat to][case d'arrivée][type de pièce capturée]
    CaptureHistory capture_history = {{{{{0}}}}};

    // Correction History
    // Utilisé pour la correction de l'évaluation
    PawnCorrectionHistoryTable    pawn_correction_history = {{0}};
    NonPawnCorrectionHistoryTable non_pawn_correction_history[N_COLORS] = {{{0}}};
};

#endif // HISTORY_H
