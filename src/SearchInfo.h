#ifndef SEARCHINFO_H
#define SEARCHINFO_H

struct SearchInfo {
    MOVE        excluded = Move::MOVE_NONE; // coup à éviter
    int         static_eval{};              // évaluation statique
    MOVE        move = Move::MOVE_NONE;     // coup cherché
    int         ply{};                      // profondeur de recherche
    PVariation  pv{};                       // Principale Variation
    int         doubleExtensions{};
    PieceTo*    cont_hist{};                // pointeur sur History::continuation_history[piece(info-x)][dest(info-x)]
    MOVE        killer1 = Move::MOVE_NONE;
    MOVE        killer2 = Move::MOVE_NONE;
    Bitboard    threats = 0ULL;             // menaces du camp ennemi
    bool        tactical = false;           // si->move est-il tactical (capture / promo / ep)
}__attribute__((aligned(64)));

#endif // SEARCHINFO_H
