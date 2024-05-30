#ifndef SEARCH_H
#define SEARCH_H


class Search;

#include <thread>
#include "defines.h"
#include "Timer.h"
#include "Board.h"
#include "types.h"

constexpr int STACK_OFFSET = 4;
constexpr int STACK_SIZE   = MAX_PLY + 2*STACK_OFFSET;  // taille un peu trop grande, mais multiple de 8

struct SearchInfo {
    MOVE        killer1;    // killer moves
    MOVE        killer2;
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

    std::thread thread;
    Search*     search;     // pointeur sur la classe de recherche
    SearchInfo* info;       // pointeur décalé de STACK_OFFSET sur la tableau total _info[STACK_SIZE]

    int         index;
    int         depth;
    int         seldepth;
    int         score;
    U64         nodes;
    bool        stopped;
    U64         tbhits;

    MOVE        best_move;
    int         best_score;
    int         best_depth;

    SearchInfo  _info[STACK_SIZE];

    // tableau donnant le bonus/malus d'un coup quiet ayant provoqué un cutoff
    // bonus history [Color][From][Dest]
    I16  history[N_COLORS][N_SQUARES][N_SQUARES];

    // tableau des coups qui ont causé un cutoff au ply précédent
    // counter_move[opposite_color][piece][dest]
    MOVE counter_move[N_COLORS][N_PIECES][N_SQUARES];

    // counter_move history [piece][dest]
    I16  counter_move_history[N_PIECES][N_SQUARES][N_PIECES][N_SQUARES];

    //
    I16  followup_move_history[N_PIECES][N_SQUARES];

    // capture history
    int capture_history[N_PIECES][N_SQUARES][N_PIECES] = {{{0}}};

    //------------------------------------------------------------------
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
        // MOVE previous_move = info[ply-1].move;

        // return( (previous_move==Move::MOVE_NONE || previous_move==Move::MOVE_NULL)
        //             ? 0
        //             : followup_move_history[Move::piece(previous_move)][Move::dest(previous_move)][Move::piece(move)][Move::dest(move)] );
        return 0;
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
    Search(const Board &m_board, const Timer &m_timer);
    ~Search();

    // Point de départ de la recherche
    template<Color C>
    void think(int _index);

private:
    Board   board;
    Timer   timer;

    template <Color C> void iterative_deepening(ThreadData* td, SearchInfo* si);
    template <Color C> int  aspiration_window(ThreadData* td, SearchInfo* si);
    template <Color C> int  alpha_beta(int alpha, int beta, int depth, ThreadData* td, SearchInfo* si);
    template <Color C> int  quiescence(int alpha, int beta, ThreadData* td, SearchInfo* si);

    void show_uci_result(const ThreadData *td, U64 elapsed, PVariation &pv) const;
    void show_uci_best(const ThreadData *td) const;
    void show_uci_current(MOVE move, int currmove, int depth) const;
    bool check_limits(const ThreadData *td) const;

    void update_pv(SearchInfo* si, const MOVE move) const;
    void update_killers(SearchInfo *si, MOVE move);
    void update_history(ThreadData *td, Color color, MOVE move, int bonus);
    void update_counter_move(ThreadData *td, Color oppcolor, int ply, MOVE move);
    MOVE get_counter_move(ThreadData *td, Color oppcolor, int ply) const;
    void update_counter_move_history(ThreadData *td, int ply, MOVE move, int bonus);

    static constexpr int CONTEMPT    = 0;

    int Reductions[2][32][32];

    static constexpr int LateMovePruning[2][8] = {
        {0, 2, 3, 4, 6, 8, 13, 18},
        {0, 3, 4, 6, 8, 12, 20, 30}
    };

    static constexpr int SEEPruningDepth = 9;
    static constexpr int SEEQuietMargin = -64;
    static constexpr int SEENoisyMargin = -19;

};

#endif // SEARCH_H
