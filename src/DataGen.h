#ifndef DATAGEN_H
#define DATAGEN_H

class DataGen;

#include <memory>
#include <thread>
#include <vector>
#include <random>
#include "Board.h"
#include "Search.h"

struct fenData {
    std::string fen;
    I32         score;  // score du point de vue des Blancs
};

class DataGen
{
public:
    DataGen(const int _nbr=4, const int max_games=100, const std::string &output="./fens");
    ~DataGen() {}

private:
    void genfens(int thread_id, const std::string& output,
                 const Usize max_games);
    // std::atomic<Usize> &total_fens);
    int  set_threads(const int nbr);
    template <Color color> void data_search(Board& board, Timer& timer,
                     Search* search,
                     // std::unique_ptr<Search>& search,
                     ThreadData* td, SearchInfo* si,
                     MOVE &move, I32 &score);

    constexpr static int MAX_RANDOM_PLIES =    8;   // nombre maximum de plies joués au hasard
    constexpr static int MAX_RANDOM_SCORE =  250;   // score maximum accepté pour la position résultant
    constexpr static int MAX_RANDOM_DEPTH =    2;   // profondeur maximum pour la recherche

    constexpr static int MAX_PLY_LOOP     =  400;   // nombre maximum de plyes dans la boucle de jeu

    constexpr static int HIGH_SCORE       = 2500;   // score maximum pour accepter une position;
                                                    // score minimum pour compter la position comme gagnante
    constexpr static int LOW_SCORE        =   10;   // score maximum pour compter la position comme annulante
    constexpr static int MIN_WIN_COUNT    =    4;   // nombre minimum de positions gagnantes pour adjuger la partie comme gagnée
    constexpr static int MIN_DRAW_COUNT   =   10;   // nombre minimum de positions nulles    pour adjuger la partie comme nulle

    // HARD_NODE_LIMIT est juste là pour empécher une explosion de la recherche
    constexpr static int HARD_NODE_LIMIT  = 5'000'000;
    constexpr static int SOFT_NODE_LIMIT  =     5'000;


};

/* 1 - 2xColor
 *
 * w = 0    1
 * b = 1   -1
 */

#endif // DATAGEN_H
