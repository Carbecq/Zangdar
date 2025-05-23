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

constexpr int MvvLvaScores[N_PIECE_TYPE][N_PIECE_TYPE] = {
    {0,  0,  0,  0,  0,  0,  0}, // victim No_Piece
    {0, 16, 15, 14, 13, 12, 11}, // victim PAWN
    {0, 26, 25, 24, 23, 22, 21}, // victim Knight
    {0, 36, 35, 34, 33, 32, 31}, // victim Bishop
    {0, 46, 45, 44, 43, 42, 41}, // vitcim Rook
    {0, 56, 55, 54, 53, 52, 51}, // victim Queen
    {0,  0,  0,  0,  0,  0,  0}  // victim King
};

// https://www.nextptr.com/question/a6212599/passing-cplusplus-arrays-to-function-by-reference

class MovePicker
{
public:

    MovePicker(Board *_board, const History& _history, const SearchInfo* _info,
               MOVE _ttMove, MOVE _killer1, MOVE _killer2, MOVE _counter,
               int _threshold) ;

    MLMove next_move(bool skipQuiets);
    void   score_noisy();
    void   score_quiet();
    bool   is_legal(MOVE move);
    void   verify_MvvLva();

    MLMove pop_move(MoveList &ml, int idx);
    void   shift_move(MoveList& ml, int idx);

    void shift_bad(int idx);
    int  get_best(const MoveList &ml);
    int  get_stage() const { return stage;}

    bool hasNext() const;
    MOVE getNext();


private:
    Board*          board;
    const History&  history;  // pour les différents history
    const SearchInfo* info;

    int     stage;         // étape courante du sélecteur
    bool    gen_quiet;     // a-t-on déjà généré les coups tranquilles ?
    bool    gen_legal;
    int     threshold;

    MOVE tt_move;
    MOVE killer1;
    MOVE killer2;
    MOVE counter;

    MoveList mlq;
    MoveList mln;
    MoveList mlb;
    MoveList mll;   // MoveList de tous les coups légaux pour déterminer si un coup est légal

};

#endif // MOVEPICKER_H
