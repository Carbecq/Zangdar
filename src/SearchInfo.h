#ifndef SEARCHINFO_H
#define SEARCHINFO_H

struct SearchInfo {
    MOVE        excluded = Move::MOVE_NONE; // coup exclu (recherche singulière)
    int         static_eval{};              // évaluation statique
    MOVE        move = Move::MOVE_NONE;     // coup joué à ce ply
    int         ply{};                      // distance à la racine
    PVariation  pv{};                       // Principale Variation
    int         doubleExtensions{};         // doubles extensions sur la ligne
    PieceTo*    cont_hist{};                // pointeur sur History::continuation_history[piece(info-x)][dest(info-x)]
    MOVE        killer1 = Move::MOVE_NONE;
    MOVE        killer2 = Move::MOVE_NONE;
    Bitboard    threats = 0ULL;             // menaces du camp ennemi
    bool        tactical = false;           // si->move est-il tactical (capture / promo / ep)
    int         reduction{};                // réduction LMR appliquée au coup menant à l'enfant (hindsight)
}__attribute__((aligned(64)));

#endif // SEARCHINFO_H
