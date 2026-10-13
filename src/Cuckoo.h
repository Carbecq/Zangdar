#ifndef CUCKOO_H
#define CUCKOO_H

#include "defines.h"

// Algorithme cuckoo de Marcel van Kervinck, pour une détection rapide des
// situations d'« upcoming repetition ».
// Description de l'algorithme dans le papier suivant :
// http://web.archive.org/web/20201107002606/https://marcelk.net/2013-04-06/paper/upcoming-rep-v2.pdf

// Sert à s'assurer la nulle quand on perd ; une répétition à venir ne rend pas
// pour autant la position nulle.

namespace Cuckoo {

// Première et seconde fonctions de hachage pour indexer les tables cuckoo

//! \brief  Première fonction de hachage pour indexer les tables cuckoo
inline U64 h1(U64 key) { return key & 0x1FFF; }

//! \brief  Seconde fonction de hachage pour indexer les tables cuckoo
inline U64 h2(U64 key) { return (key >> 16) & 0x1FFF; }

// Tables cuckoo : hashs Zobrist des coups réversibles valides, et les coups eux-mêmes
extern std::array<U64, 8192>  keys;
extern std::array<MOVE, 8192> moves;


void init();
}

#endif // CUCKOO_H
