#include <cstring>
#include "MovePicker.h"
#include "types.h"
#include "Move.h"

//=====================================================
//! \brief  Constructeur
//-----------------------------------------------------
MovePicker::MovePicker(Board* _board, const History& _history, const SearchInfo *_info,
                       MOVE _ttMove, MOVE _killer1, MOVE _killer2, MOVE _counter, int _threshold) :
    board(_board),
    history(_history),
    info(_info),
    stage(STAGE_TABLE),
    gen_quiet(false),
    gen_legal(false),
    threshold(_threshold),
    tt_move(_ttMove),
    killer1(_killer1),
    killer2(_killer2),
    counter(_counter)
{
#if 0
    int nbr_noisy = 0;
    int nbr_quiet = 0;
    int nbr_capt = 0;
    int nbr_promo = 0;
    int nbr_capt_promo = 0;
    int nbr_pep = 0;
    int nbr_cas = 0;

    if (board->turn() == WHITE)
    {
        board->legal_moves<WHITE, MoveGenType::ALL>(mll);
        board->legal_noisy<WHITE>(mln);
        board->legal_quiet<WHITE>(mlq);
    }
    else
    {
        board->legal_moves<BLACK, MoveGenType::ALL>(mll);
        board->legal_noisy<BLACK>(mln);
        board->legal_quiet<BLACK>(mlq);
    }

    for (int i=0; i<mll.count; i++)
    {
        if (Move::is_capturing(mll.mlmoves[i].move))
        {
            if (Move::is_promoting(mll.mlmoves[i].move))
            {
                nbr_capt_promo++;
                // mln.mlmoves[mln.count++].move = mll.mlmoves[i].move;
                nbr_noisy++;
            }
            else if (!Move::is_promoting(mll.mlmoves[i].move))
            {
                nbr_capt++;
                // mln.mlmoves[mln.count++].move = mll.mlmoves[i].move;
                nbr_noisy++;
            }
        }
        else if (Move::is_promoting(mll.mlmoves[i].move))
        {
            nbr_promo++;
            // mln.mlmoves[mln.count++].move = mll.mlmoves[i].move;
            nbr_noisy++;
        }
        else if(Move::is_enpassant(mll.mlmoves[i].move))
        {
            nbr_pep++;
            // mln.mlmoves[mln.count++].move = mll.mlmoves[i].move;
            nbr_noisy++;
        }
        else if(Move::is_castling(mll.mlmoves[i].move))
        {
            nbr_cas++;
            // mln.mlmoves[mln.count++].move = mll.mlmoves[i].move;
        }

        else
        {
            // mlq.mlmoves[mlq.count++].move = mll.mlmoves[i].move;
            nbr_quiet++;
        }
    }
    nbr_noisy = mln.size();
    nbr_quiet = mlq.size();

    if (mll.count != (nbr_noisy + nbr_quiet))
    {
        std::cout << ">>>>>>>>>>> erreur 1 " << std::endl;
        std::cout << "nbr_noisy =  " << nbr_noisy << std::endl;
        std::cout << "nbr_capt =  " << nbr_capt << std::endl;
        std::cout << "nbr_promo =  " << nbr_promo << std::endl;
        std::cout << "nbr_pep =  " << nbr_pep << std::endl;
        std::cout << "nbr_noisy =  " << mln.count << std::endl;
    }

    if (nbr_noisy != (nbr_capt + nbr_capt_promo + nbr_promo + nbr_pep))
    {
        std::cout << ">>>>>>>>>>> erreur 2 " << std::endl;
        std::cout << "nbr_noisy =  " << nbr_noisy << std::endl;
        std::cout << "nbr_capt =  " << nbr_capt << std::endl;
        std::cout << "nbr_capt_promo =  " << nbr_capt_promo << std::endl;
        std::cout << "nbr_promo =  " << nbr_promo << std::endl;
        std::cout << "nbr_pep =  " << nbr_pep << std::endl;
        std::cout << "nbr_castling =  " << nbr_cas << std::endl;
        std::cout << "nbr_noisy =  " << mln.count << std::endl;
    }


    if (nbr_noisy != mln.count)
        std::cout << ">>>>>>>>>>> erreur 3 " << std::endl;

    if (nbr_quiet != mlq.count)
        std::cout << ">>>>>>>>>>> erreur 4 " << std::endl;

    // std::cout << "vérification terminée" << std::endl;
    // score_noisy(board);
    // score_quiet(board);
#endif
}


//=====================================================
//! \brief  Sélection du prochain coup
//-----------------------------------------------------
MLMove MovePicker::next_move(bool skipQuiets)
{
    switch (stage)
    {

    case STAGE_TABLE:

        // Play the table move if it is from this
        // position, also advance to the next stage

        stage = STAGE_GENERATE_NOISY;
        if (is_legal(tt_move))
            return MLMove{tt_move, 0};

        [[fallthrough]];

    case STAGE_GENERATE_NOISY:

        // Generate all noisy moves and evaluate them. Set up the
        // split in the array to store quiet and noisy moves. Also,
        // this stage is only a helper. Advance to the next one.

        if (board->turn() == WHITE)
            board->legal_moves<WHITE, MoveGenType::NOISY>(mln);
        else
            board->legal_moves<BLACK, MoveGenType::NOISY>(mln);
        score_noisy();
        stage = STAGE_GOOD_NOISY ;

        [[fallthrough]];

    case STAGE_GOOD_NOISY:

        // Check to see if there are still more noisy moves
        if (mln.count != 0)
        {
            int  best     = get_best(mln);

            // Don't play the table move twice
            if (mln.mlmoves[best].move == tt_move)
            {
                shift_move(mln, best);
                return next_move(skipQuiets);
            }

            if (!board->fast_see(mln.mlmoves[best].move, threshold))
            {
                shift_bad(best);
                return next_move(skipQuiets);
            }

            return pop_move(mln, best);
        }

        if (skipQuiets)
        {
            stage = STAGE_BAD_NOISY;
            return next_move(skipQuiets);
        }

        stage = STAGE_KILLER_1;

        [[fallthrough]];

    case STAGE_KILLER_1:

        // Play the killer move if it is from this position.
        // position, and also advance to the next stage
        stage = STAGE_KILLER_2;

        if (   !skipQuiets
               && killer1 != tt_move)
        {
            if (is_legal(killer1))
                return MLMove{killer1, 0};
        }

        [[fallthrough]];

    case STAGE_KILLER_2:

        // Play the killer move if it is from this position.
        // position, and also advance to the next stage
        stage = STAGE_COUNTER_MOVE;

        if (   !skipQuiets
               && killer2 != tt_move)
        {
            if (is_legal(killer2))
                return MLMove{killer2, 0};
        }

        [[fallthrough]];

    case STAGE_COUNTER_MOVE:

        // Play the counter move if it is from this position.
        // position, and also advance to the next stage
        stage = STAGE_GENERATE_QUIET;

        if (   !skipQuiets
               && counter != tt_move
               && counter != killer1
               && counter != killer2)
        {
            if (is_legal(counter))
                return MLMove{counter, 0};
        }

        [[fallthrough]];

    case STAGE_GENERATE_QUIET:

        // Generate all quiet moves and evaluate them
        // and also advance to the final fruitful stage
        if (!skipQuiets)
        {
            if (gen_quiet == false)
            {
                if (board->turn() == WHITE)
                    board->legal_moves<WHITE, MoveGenType::QUIET>(mlq);
                else
                    board->legal_moves<BLACK, MoveGenType::QUIET>(mlq);
                gen_quiet = true;
                score_quiet();
            }
        }
        stage = STAGE_QUIET;

        [[fallthrough]];

    case STAGE_QUIET:

        // Check to see if there are still more quiet moves
        if (mlq.count != 0 && !skipQuiets)
        {
            int  best     = get_best(mlq);
            MLMove bestMove = pop_move(mlq, best);

            if (   bestMove.move == tt_move
                   || bestMove.move == killer1
                   || bestMove.move == killer2
                   || bestMove.move == counter )
                return next_move(skipQuiets);
            else
                return bestMove;
        }

        // If no quiet moves left, advance stages
        stage = STAGE_BAD_NOISY;

        [[fallthrough]];

    case STAGE_BAD_NOISY:

        if (mlb.count > 0)
        {
            int  best     = get_best(mlb);
            MLMove bestMove = pop_move(mlb, best);

            // Don't play the table move twice
            if (   bestMove.move == tt_move
                   || bestMove.move == killer1
                   || bestMove.move == killer2
                   || bestMove.move == counter )
                return next_move(skipQuiets);
            return bestMove;
        }

        stage = STAGE_DONE;

        [[fallthrough]];

    case STAGE_DONE:
        return MLMove{Move::MOVE_NONE, 0};

    default:
        assert(0);
        return MLMove{Move::MOVE_NONE, 0};
    }
}

//==================================================================
//! \brief  Evaluation des coups de capture et de promotion
//------------------------------------------------------------------
void MovePicker::score_noisy()
{
    MOVE move;
    int  value;

    for (size_t i = 0; i < mln.count; i++)
    {
        move     = mln.mlmoves[i].move;

        // Use the standard MVV-LVA
        // PieceType dest_type = board->piece_on(Move::dest(move));  // pièce prise ou promotion

        // std::cout << "i= " << i << "  " << Move::name(move) << std::endl;
        // std::cout << "captured = " << piece_name[Move::captured(move)] << " value = " << MvvLvaScores[Move::captured(move)][Move::piece(move)] << std::endl;
        // std::cout << "dest     = " << piece_name[dest_type] << " value = "            << MvvLvaScores[dest_type][Move::piece(move)] << std::endl;
        // std::cout << "promo    = " << piece_name[Move::promotion(move)] << " value = " << MvvLvaScores[Move::promotion(move)][Move::piece(move)] << std::endl;


        //    value = mg_value[dest_type] - Move::piece(move);
        value = MvvLvaScores[static_cast<U32>(Move::captured_type(move))][static_cast<U32>(Move::piece_type(move))];

        // A bonus is in order for queen promotions
        if (Move::is_promoting(move))
            value += EGPieceValue[static_cast<U32>(Move::promoted_type(move))];

        // Enpass is a special case of MVV-LVA
        else if (Move::is_enpassant(move))
            value = MvvLvaScores[static_cast<U32>(PieceType::PAWN)][static_cast<U32>(PieceType::PAWN)];
        // eg_value[PAWN] -PieceType::PAWN;

        mln.mlmoves[i].value = value ; // + history.get_capture_history(move) / 100; //TODO à modérer ??
    }
}

//==================================================================
//! \brief  Evaluation des coups tranquilles
//------------------------------------------------------------------
void MovePicker::score_quiet()
{
    MOVE move;

    // Use the History score for sorting
    for (size_t i = 0; i < mlq.count; i++)
    {
        move = mlq.mlmoves[i].move;
        mlq.mlmoves[i].value = history.get_main_history(board->turn(), move)
                + history.get_counter_move_history(info, move)
                + history.get_followup_move_history(info, move);

        // mlq.mlmoves[i].value = history.get_quiet_history(board->turn(), info, move);
    }
}

//====================================================
//! \brief  Retourne l'indice du meilleur élément
//-----------------------------------------------------
int MovePicker::get_best(const MoveList& ml)
{
    int best_index = 0;

    // Find highest scoring move
    for (size_t i = 1; i < ml.count; i++)
    {
        if (ml.mlmoves[i].value > ml.mlmoves[best_index].value)
            best_index = i;
    }

    return best_index;
}

//========================================================
//! \brief  Retourne le coup indiqué
//! puis déplace le dernier élément à la position
//! du coup indiqué
//--------------------------------------------------------
MLMove MovePicker::pop_move(MoveList& ml, int idx)
{
    /*
    ---------------+-----------------+
                   idx               count
                                    count
    */

    MLMove temp = ml.mlmoves[idx];

    ml.count--;
    std::memcpy(&ml.mlmoves[idx], &ml.mlmoves[ml.count], sizeof(MLMove));

    return temp;
}

//========================================================
//! \brief  Déplace le dernier élément à la position indiquée
//--------------------------------------------------------
void MovePicker::shift_move(MoveList& ml, int idx)
{
    ml.count--;
    std::memcpy(&ml.mlmoves[idx], &ml.mlmoves[ml.count], sizeof(MLMove));
}

//======================================================
//! \brief  Déplace le coup indiqué dans la liste mlb
//! Puis enlève le coup de la liste mln
//------------------------------------------------------
void MovePicker::shift_bad(int idx)
{
    // Put the bad capture in the "bad" list
    std::memcpy(&mlb.mlmoves[mlb.count], &mln.mlmoves[idx], sizeof(MLMove));
    mlb.count++;

    // put the last good capture here instead
    mln.count--;
    std::memcpy(&mln.mlmoves[idx], &mln.mlmoves[mln.count], sizeof(MLMove));
}


//==================================================================
//! \brief  Vérification de la légalité d'un coup
//------------------------------------------------------------------
bool MovePicker::is_legal(MOVE move)
{
    // assert(move != Move::MOVE_NONE);
    assert(move != Move::MOVE_NULL);

    if (move == Move::MOVE_NONE)
        return false;

    //ZZZZ  Peut-on faire plus rapide ???

    if (gen_legal == false)
    {
        if (board->turn() == WHITE)
            board->legal_moves<WHITE, MoveGenType::ALL>(mll);
        else
            board->legal_moves<BLACK, MoveGenType::ALL>(mll);
        gen_legal = true;
    }

    for (size_t n=0; n<mll.count; n++)
        if (mll.mlmoves[n].move == move)
            return true;
    return false;
}


std::string pchar[N_PIECE_TYPE] = {"NoPiece", "Pion", "Cavalier", "Fou", "Tour", "Dame", "Roi"};
void MovePicker::verify_MvvLva()
{
    for(U32 Victim = static_cast<U32>(PieceType::PAWN); Victim <= static_cast<U32>(PieceType::KING); ++Victim)
    {
        for(U32 Attacker = static_cast<U32>(PieceType::PAWN); Attacker <= static_cast<U32>(PieceType::KING); ++Attacker)
        {
            printf("%10s prend %10s = %d\n", pchar[Attacker].c_str(), pchar[Victim].c_str(), MvvLvaScores[Victim][Attacker]);
        }
    }

}
