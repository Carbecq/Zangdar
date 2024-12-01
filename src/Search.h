#ifndef SEARCH_H
#define SEARCH_H


class Search;

#include <cstring>
#include <thread>
#include "Timer.h"
#include "Board.h"
#include "types.h"
#include "defines.h"

constexpr int STACK_OFFSET = 4;
constexpr int STACK_SIZE   = MAX_PLY + 2*STACK_OFFSET;  // taille un peu trop grande, mais multiple de 8

//! \brief  Données initialisées à chaque début de recherche
struct SearchInfo {
    MOVE        excluded;   // coup à éviter
    int         eval;       // évaluation statique
    MOVE        move;       // coup cherché
    int         ply;        // profondeur de recherche
    PVariation  pv;         // Principale Variation
}__attribute__((aligned(64)));

//! \brief  Données d'une thread
struct ThreadData
{
public:

    SearchInfo  _info[STACK_SIZE];
    std::thread thread;
    Search*     search;     // pointeur sur la classe de recherche
    SearchInfo* info;       // pointeur décalé de STACK_OFFSET sur la tableau total _info[STACK_SIZE]
    PVariation  pvs[MAX_PLY+1];

    int         index;
    int         depth;
    int         seldepth;
    U64         nodes;
    bool        stopped;
    U64         tbhits;

    int         best_depth;

    int  get_best_depth()   const { return best_depth;              }
    MOVE get_best_move()    const { return pvs[best_depth].line[0]; }
    int  get_best_score()   const { return pvs[best_depth].score;   }
    int  get_pv_length()    const { return pvs[best_depth].length;  }
    MOVE get_pv_move(int i) const { return pvs[best_depth].line[i]; }

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

    //*********************************************************************
    void reset()
    {
        std::memset(_info,                 0, sizeof(SearchInfo)*STACK_SIZE);
        std::memset(pvs,                   0, sizeof(PVariation)*MAX_PLY);

        std::memset(killer1,               0, sizeof(KillerTable));
        std::memset(killer2,               0, sizeof(KillerTable));
        std::memset(history,               0, sizeof(Historytable));
        std::memset(counter_move,          0, sizeof(CounterMoveTable));
        std::memset(counter_move_history,  0, sizeof(CounterMoveHistoryTable));
        std::memset(followup_move_history, 0, sizeof(FollowupMoveHistoryTable));
    }
    //*********************************************************************
    int get_history(Color color, const MOVE move) const
    {
        return(history[color][Move::from(move)][Move::dest(move)]);
    }
    //==================================================================
    //! \brief  Récupère le counter_move history
    //! \param[in] ply    ply cherché
    //------------------------------------------------------------------
    int get_counter_move_history(int ply, MOVE move) const
    {
        MOVE previous_move = info[ply-1].move;

        return( (previous_move==Move::MOVE_NONE || previous_move==Move::MOVE_NULL)
                    ? 0
                    : counter_move_history[Move::piece(previous_move)][Move::dest(previous_move)][Move::piece(move)][Move::dest(move)] );
    }
    int get_followup_move_history(int ply, MOVE move) const
    {
        MOVE folowup_move = info[ply-2].move;

        return( (folowup_move==Move::MOVE_NONE || folowup_move==Move::MOVE_NULL)
                    ? 0
                    : followup_move_history[Move::piece(folowup_move)][Move::dest(folowup_move)][Move::piece(move)][Move::dest(move)] );
    }

}__attribute__((aligned(64)));


// classe permettant de redéfinir mon 'locale'
// en effet, je n'en ai pas trouvé (windows ? mingw ?)
// de qui permet d'écrire un entier avec un séparateur : 1.000.000

class MyNumPunct : public std::numpunct<char>
{
protected:
    virtual char do_thousands_sep() const { return '.'; }
    virtual std::string do_grouping() const { return "\03"; }
};


class Search
{
public:
    Search();
    ~Search();

    // Point de départ de la recherche
    template<Color C>
    void think(Board board, Timer timer, int _index);

    template <Color C> void aspiration_window(Board& board, Timer& timer, ThreadData* td, SearchInfo* si);
    void show_uci_result(const ThreadData *td, U64 elapsed) const;

private:
    template <Color C> void iterative_deepening(Board& board, Timer& timer, ThreadData* td, SearchInfo* si);
    template <Color C> int  alpha_beta(Board& board, Timer& timer, int alpha, int beta, int depth, ThreadData* td, SearchInfo* si);
    template <Color C> int  quiescence(Board& board, Timer& timer, int alpha, int beta, ThreadData* td, SearchInfo* si);

   void show_uci_best(const ThreadData *td) const;
    void show_uci_current(MOVE move, int currmove, int depth) const;
    bool check_limits(const Timer &timer, const ThreadData *td) const;

    void update_pv(SearchInfo* si, const MOVE move) const;
    void update_killers(ThreadData *td, int ply, MOVE move);
    void update_history(ThreadData *td, Color color, MOVE move, int bonus);
    void update_counter_move(ThreadData *td, Color oppcolor, int ply, MOVE move);
    MOVE get_counter_move(ThreadData *td, Color oppcolor, int ply) const;
    void update_counter_move_history(ThreadData *td, int ply, MOVE move, int bonus);
    void update_followup_move_history(ThreadData* td, int ply, MOVE move, int bonus);


    static constexpr int CONTEMPT    = 0;

    int Reductions[2][32][32];

    static constexpr int LateMovePruningDepth = 7;
    static constexpr int LateMovePruningCount[2][8] = {
        {0, 2, 3, 4, 6, 8, 13, 18},
        {0, 3, 4, 6, 8, 12, 20, 30}
    };

    static constexpr int FutilityMargin = 95;
    static constexpr int FutilityPruningDepth = 8;
    static constexpr int FutilityPruningHistoryLimit[] = { 12000, 6000 };

    static constexpr int CounterMovePruningDepth[] = { 3, 2 };
    static constexpr int CounterMoveHistoryLimit[] = { 0, -1000 };

    static constexpr int FollowUpMovePruningDepth[] = { 3, 2 };
    static constexpr int FollowUpMoveHistoryLimit[] = { -2000, -4000 };

    static constexpr int SEEPruningDepth =  9;
    static constexpr int SEEQuietMargin = -64;
    static constexpr int SEENoisyMargin = -19;

    static constexpr int ProbCutDepth  =   5;
    static constexpr int ProbCutMargin = 100;

};

#endif // SEARCH_H
