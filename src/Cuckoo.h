#ifndef CUCKOO_H
#define CUCKOO_H

#include "defines.h"

// Algorithme cuckoo de Marcel van Kervinck, pour une détection rapide des
// situations d'« upcoming repetition ».
// Description de l'algorithme dans le papier suivant :
// http://web.archive.org/web/20201107002606/https://marcelk.net/2013-04-06/paper/upcoming-rep-v2.pdf

/*
    Les tables cuckoo servent surtout à sécuriser un score de nulle quand on
     est en train de perdre.
    Si on a déjà mieux qu'une nulle (alpha > score_de_nulle), il faut quand même
     continuer à chercher même si une répétition est "imminente" :
     une répétition qui n'a pas encore eu lieu ne rend pas la position nulle pour autant.
    La vraie règle des échecs (triple répétition déjà survenue), elle, adjuge réellement la partie.
    Le gain apporté par les tables cuckoo est de l'ordre de 5-10 Elo dans la plupart des moteurs.

*/

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
