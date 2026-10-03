#ifndef MOVEPICKER_H
#define MOVEPICKER_H

class MovePicker;

#include "MoveList.h"
#include "Board.h"
#include "History.h"

enum {
    STAGE_TABLE,
    STAGE_GENERATE_NOISY,
    STAGE_GOOD_NOISY,
    STAGE_KILLER_1,
    STAGE_KILLER_2,
    STAGE_COUNTER_MOVE,
    STAGE_GENERATE_QUIET,
    STAGE_QUIET,
    STAGE_BAD_NOISY,
    STAGE_DONE
};

// https://www.nextptr.com/question/a6212599/passing-cplusplus-arrays-to-function-by-reference

class MovePicker
{
public:

    MovePicker(Board& _board, const History& _history, const SearchInfo* _info,
               MOVE _ttMove, MOVE _killer1, MOVE _killer2, MOVE _counter,
               int _threshold) ;

    MLMove next_move(bool skipQuiets);
    void   score_noisy();
    void   score_quiet();
    bool   is_legal(MOVE move);
    bool   is_legal_quiet(MOVE move);

    MLMove pop_move(MoveList &ml, size_t idx);
    void   shift_move(MoveList& ml, size_t idx);

    void shift_bad(size_t idx);
    size_t get_best(const MoveList &ml);
    //! \brief Retourne l'étape courante du sélecteur de coups
    int  get_stage() const { return stage;}

    bool hasNext() const;
    MOVE getNext();


private:
          Board&    board;
    const History&  history;  // pour les différents history
    const SearchInfo* info;

    void    generate_noisy();
    void    generate_quiet();
    void    generate_all();

    int     stage;         // étape courante du sélecteur
    bool    gen_noisy;     // a-t-on déjà généré les coups tactiques ?
    bool    gen_quiet;     // a-t-on déjà généré les coups tranquilles ?
    int     threshold;

    MOVE tt_move = Move::MOVE_NONE;
    MOVE killer1 = Move::MOVE_NONE;
    MOVE killer2 = Move::MOVE_NONE;
    MOVE counter = Move::MOVE_NONE;

    MoveList mlq;
    MoveList mln;
    MoveList mlb;

};

#endif // MOVEPICKER_H
