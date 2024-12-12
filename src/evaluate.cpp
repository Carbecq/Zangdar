#include "Board.h"
#include "defines.h"
#include "evaluate.h"
#include "TranspositionTable.h"
#include "NNUE.h"

#if defined USE_TUNER
#include "Tuner.h"
#endif

/*
https://github.com/nmrugg/stockfish.js/blob/master/src/evaluate.cpp

https://hxim.github.io/Stockfish-Evaluation-Guide/
https://www.chessprogramming.org/Evaluation

*/

/*  Bien que beaucoup de codes implémentent des idées identiques,
 *  Ce code a principalement comme base Weiss et Ethereal.
 *  Ce sont des codes faciles à lire et instructifs.
 *
 */

//==========================================
//! \brief  Evaluation de la position
//! \return Evaluation statique de la position
//! du point de vue du camp au trait.
//------------------------------------------
[[nodiscard]] int Board::evaluate()
{
#if defined USE_NNUE
    if (side_to_move == WHITE)
        return nnue.evaluate<WHITE>();
    else
        return nnue.evaluate<BLACK>();

    //note : certains codes modifient la valeur retournée par nnue
    //       soit avec une "phase", soit avec une valeur "random".

#endif

    Score eval = 0;
    EvalInfo ei;

    // Initialisations
    init_eval_info(ei);

    //--------------------------------
    //  Evaluation des pions
    //--------------------------------
    eval += probe_pawn_cache(ei);

    //--------------------------------
    // Evaluation des pieces
    //--------------------------------
    eval += evaluate_pieces(ei);

    //--------------------------------
    // Evaluation des pions passés
    //  + à faire après "evaluate_pieces" à cause du tableau "allAttacks"
    //--------------------------------
    eval += evaluate_passed<WHITE>(ei)  - evaluate_passed<BLACK>(ei);

    //--------------------------------
    // Evaluation des menaces
    //--------------------------------
    eval += evaluate_threats<WHITE>(ei) - evaluate_threats<BLACK>(ei);

    //--------------------------------
    // Evaluation des attaques sur le roi ennemi
    //--------------------------------
    eval += evaluate_king_attacks<WHITE>(ei) - evaluate_king_attacks<BLACK>(ei);

#if defined USE_TUNER
    ownTuner.Trace.eval = eval;
#endif

    // Adjust eval by scale factor
    int scale = scale_factor(eval);

#if defined USE_TUNER
    ownTuner.Trace.scale = scale;
#endif

    // Adjust score by phase
    int phase24  = ei.phase24;
    int phase256 = get_phase256(phase24);

    // Compute and store an interpolated evaluation from white's POV
    eval = ( MgScore(eval) * phase256
            +   EgScore(eval) * (256 - phase256) * scale / 128 ) / 256;

    // Return the evaluation, negated if we are black
    return (side_to_move == WHITE ? eval : -eval) + Tempo;
}

//=======================================================
//! \brief  Calcul ou récupération de l'évaluation des pions
//-------------------------------------------------------
Score Board::probe_pawn_cache(EvalInfo& ei)
{
    // On ne peut pas utiliser la table lors d'un tuning
#if defined USE_TUNER
    return (evaluate_pawns<WHITE>(ei) - evaluate_pawns<BLACK>(ei));
#else

    Score eval = 0;
    Bitboard passed_pawns;

    // Recherche de la position des pions dans le cache
    if (transpositionTable.probe_pawn_table(get_pawn_hash(), eval, passed_pawns) == true)
    {
        // La table de pions contient la position,
        // On récupère l'évaluation de la table
        ei.passedPawns = passed_pawns;
    }
    else
    {
        // La table de pions ne contient pas la position,
        // on calcule l'évaluation, et on la stocke
        eval = evaluate_pawns<WHITE>(ei) - evaluate_pawns<BLACK>(ei);
        transpositionTable.store_pawn_table(get_pawn_hash(), eval, ei.passedPawns);
    }

    return eval;

#endif

}

//=================================================================
//! \brief  Evaluation des pions d'une couleur
//!
//-----------------------------------------------------------------
template<Color US>
Score Board::evaluate_pawns(EvalInfo& ei)
{
    constexpr Color THEM = ~US;
    constexpr Direction DOWN = (US == WHITE) ? SOUTH : NORTH;

    int sq, sqpos;
    int count;
    int rank;
    Score eval = 0;

    const Bitboard upawns = ei.pawns[US];
    const Bitboard tpawns = ei.pawns[THEM];

    // https://chessfox.com/pawn-structures-why-pawns-are-the-soul-of-chess/

    // isolés (voir ethereal)
    // double isolated  : 2 pions doublés bloqués par un pion ennemi
    // backward
    // doubled
    // connected
    // Weak unopposed pawn.
    // blocked


    // Pions doublés
    // 1- des pions doublés désignent deux pions de la même couleur sur une même colonne.
    // 2- n'est pris en compte que si l'un empèche l'autre de bouger
    // 3- on ne compte qu'une seule fois les pions doublés
    count = BB::count_bit(upawns & BB::north(upawns));
    eval += PawnDoubled * count;

#if defined DEBUG_EVAL
    if (count)
        printf("Le camp %s possède %d pions doublés \n", camp[1][US].c_str(), count);
#endif
#if defined USE_TUNER
    ownTuner.Trace.PawnDoubled[US] += count;
#endif


    // Pions doublés mais séparés d'une case
    count = BB::count_bit(upawns & BB::shift<NORTH_NORTH>(upawns));
    eval += PawnDoubled2 * count;

#if defined DEBUG_EVAL
    if (count)
        printf("Le camp %s possède %d pions doublés séparés \n", camp[1][US].c_str(), count);
#endif
#if defined USE_TUNER
    ownTuner.Trace.PawnDoubled2[US] += count;
#endif


    // Pions liés
    // Des pions sont liés lorsqu'ils sont placés sur une diagonale et que l'un défend l'autre.
    count = BB::count_bit(upawns & BB::all_pawn_attacks<THEM>(upawns));
    eval += PawnSupport * count;

#if defined DEBUG_EVAL
    if (count)
        printf("Le camp %s possède %d pions liés \n", camp[1][US].c_str(), count);
#endif
#if defined USE_TUNER
    ownTuner.Trace.PawnSupport[US] += count;
#endif


    // Open pawns
    // undefended, unopposed pawns (#436)
    // Undefended, unopposed pawns are targets for constant pressure by rooks and queens.
    Bitboard open = ~BB::fill<DOWN>(tpawns);
    count = BB::count_bit(upawns & open & ~BB::all_pawn_attacks<US>(upawns));
    eval += PawnOpen * count;

#if defined DEBUG_EVAL
    if (count)
        printf("Le camp %s possède %d pions non défendus sans opposition \n", camp[1][US].c_str(), count);
#endif
#if defined USE_TUNER
    ownTuner.Trace.PawnOpen[US] += count;
#endif


    // Pions en phalange (Pawn Phalanx)
    Bitboard phalanx = upawns & BB::west(upawns);
    while (phalanx)
    {
        rank  = SQ::relative_rank8<US>(SQ::rank(BB::pop_lsb(phalanx)));
        eval += PawnPhalanx[rank];

#if defined DEBUG_EVAL
        printf("Le camp %s possède des pions en phalange sur la rangée %d \n", camp[1][US].c_str(), rank);
#endif
#if defined USE_TUNER
        ownTuner.Trace.PawnPhalanx[rank][US]++;
#endif
    }


    // Evaluation des pions un par un
    Bitboard bb = upawns;
    while (bb)
    {
        sq    = BB::pop_lsb(bb);                   // case où est la pièce
        sqpos = SQ::relative_square<US>(sq);               // case inversée pour les tables
        eval += PawnValue;                         // score matériel
        eval += PawnPSQT[sqpos];                   // score positionnel

#if defined DEBUG_EVAL
            //            printf("le pion %s (%d) sur la case %s a une valeur MG de %d \n", side_name[US].c_str(), Us, square_name[sq].c_str(), mg_pawn_table[sqpos]);
#endif
#if defined USE_TUNER
        ownTuner.Trace.PawnValue[US]++;
        ownTuner.Trace.PawnPSQT[sqpos][US]++;
#endif


        // Pion isolé
        //  Un pion isolé est un pion qui n'a plus de pion de son camp sur les colonnes adjacentes.
        //  Un pion isolé peut être redoutable en milieu de partie1.
        //  C'est souvent une faiblesse en finale, car il est difficile à défendre.
        if ((upawns & AdjacentFilesMask64[sq]) == 0)
        {
            eval += PawnIsolated;

#if defined DEBUG_EVAL
            printf("le pion %s en %s est isolé \n", camp[1][US].c_str(), square_name[sq].c_str());
#endif
#if defined USE_TUNER
            ownTuner.Trace.PawnIsolated[US]++;
#endif
        }


        // Pion passé
        //  un pion passé est un pion qui n'est pas gêné dans son avance vers la 8e rangée par un pion adverse,
        //  c'est-à-dire qu'il n'y a pas de pion adverse devant lui, ni sur la même colonne, ni sur une colonne adjacente
        if ((PassedPawnMask[US][sq] & tpawns) == 0)
        {
            rank  = SQ::rank(sqpos);
            eval += PawnPassed[rank];

#if defined DEBUG_EVAL
            printf("le pion %s en %s est passé \n", camp[1][US].c_str(), square_name[sq].c_str());
#endif
#if defined USE_TUNER
            ownTuner.Trace.PawnPassed[rank][US]++;
#endif

            // Pion passé protégé
            if (SQ::square_BB(sq) & BB::all_pawn_attacks<US>(upawns))
            {
                eval += PassedDefended[rank];

#if defined DEBUG_EVAL
                printf("le pion %s en %s est passé et protégé \n", camp[1][US].c_str(), square_name[sq].c_str());
#endif
#if defined USE_TUNER
                ownTuner.Trace.PassedDefended[rank][US]++;
#endif
            }

            ei.passedPawns |= SQ::square_BB(sq);
        }

    } // pions

    return eval;
}

//=================================================================
//! \brief  Evaluation des pions passés d'une couleur
//-----------------------------------------------------------------
template<Color US>
Score Board::evaluate_passed(EvalInfo& ei)
{
    constexpr Color THEM     = ~US;
    constexpr Direction UP   = (US == WHITE) ? NORTH : SOUTH;
    constexpr Direction DOWN = (US == WHITE) ? SOUTH : NORTH;

    Score eval = 0;
    int count;
    int sq;
    int forward;

    Bitboard passers = colorPiecesBB[US] & ei.passedPawns;

    while (passers)
    {
        sq     = BB::pop_lsb(passers);                   // case où est la pièce

        forward  = sq + UP;
        int rank = SQ::relative_rank8<US>(SQ::rank(sq));

        if (rank < RANK_4)
            continue;

        int file  = SQ::file(sq);
        int promo = SQ::relative_square<US>(SQ::square(file, RANK_8));

        // printf("sq= %d %s ; rank=%d file=%d promo=%d %s \n", sq, square_name[sq].c_str(), rank, file, promo, square_name[promo].c_str());
        // printf("distance promo      = %d \n", chebyshev_distance(sq, promo));
        // printf("distance roi ennemi = %d \n", chebyshev_distance(king_square<THEM>(), promo));
        // printf("THEm=%d side=%d \n", THEM, side_to_move);

        // Règle du carré
        if (   getNonPawnMaterialCount<THEM>() == 0
            && chebyshev_distance(sq, promo) < chebyshev_distance(king_square<THEM>(), promo) - (THEM == side_to_move))
        {
            eval += PassedSquare;

#if defined DEBUG_EVAL
            printf("le pion passé %s en %s respecte la règle du carré \n", camp[1][US].c_str(), square_name[sq].c_str());
#endif
#if defined USE_TUNER
            ownTuner.Trace.PassedSquare[US]++;
#endif
        }


        // Distance au roi ami
        count = chebyshev_distance(forward, king_square<US>());
        eval += count * PassedDistUs[rank];

#if defined DEBUG_EVAL
        if (count)
            printf("le pion passé %s en %s est à une distance de %d du roi ami \n", camp[1][US].c_str(), square_name[sq].c_str(), count);
#endif
#if defined USE_TUNER
        ownTuner.Trace.PassedDistUs[rank][US] += count;
#endif


        // Distance au roi ennemi
        count = (rank - RANK_3) * chebyshev_distance(forward, king_square<THEM>());
        eval += count * PassedDistThem;

#if defined DEBUG_EVAL
        if (count)
            printf("le pion passé %s en %s est à une distance de %d du roi ennemi \n", camp[1][US].c_str(), square_name[sq].c_str(), count);
#endif
#if defined USE_TUNER
        ownTuner.Trace.PassedDistThem[US] += count;
#endif


        // Pion passé bloqué
        if (pieceOn[forward])
        {
            eval += PassedBlocked[rank];

#if defined DEBUG_EVAL
            printf("le pion passé %s en %s est bloqué \n", camp[1][US].c_str(), square_name[sq].c_str());
#endif
#if defined USE_TUNER
            ownTuner.Trace.PassedBlocked[rank][US]++;
#endif
        }


        // Pion passé libre d'avancer
        else if (!(SQ::square_BB(forward) & ei.attacked[THEM]))
        {
            eval += PassedFreeAdv[rank];

#if defined DEBUG_EVAL
            printf("le pion passé %s en %s est libre d'avancer \n", camp[1][US].c_str(), square_name[sq].c_str());
#endif
#if defined USE_TUNER
            ownTuner.Trace.PassedFreeAdv[rank][US]++;
#endif
        }

        // Tour soutenant le pion; il n'y a rien entre le pion et la tour
        if (  occupancy_cp<US, ROOK>()                          // les tours amies
            & BB::fill<DOWN>(SQ::square_BB(sq))         // situées derrière le pion
            & Attacks::rook_moves(sq, ei.occupied)) // cases attaquées par une tour en "sq"
        {
            eval += PassedRookBack;

#if defined DEBUG_EVAL
            printf("le pion passé %s en %s est protégé par une tour \n", camp[1][US].c_str(), square_name[sq].c_str());
#endif
#if defined USE_TUNER
            ownTuner.Trace.PassedRookBack[US]++;
#endif
        }
    }

    return eval;
}

//===================================================================
//! \brief  Evaluation des pièces
//-------------------------------------------------------------------
Score Board::evaluate_pieces(EvalInfo& ei)
{
    Score eval = 0;

    eval += evaluate_knights<WHITE>(ei) - evaluate_knights<BLACK>(ei);
    eval += evaluate_bishops<WHITE>(ei) - evaluate_bishops<BLACK>(ei);
    eval += evaluate_rooks<WHITE>(ei)   - evaluate_rooks<BLACK>(ei);
    eval += evaluate_queens<WHITE>(ei)  - evaluate_queens<BLACK>(ei);
    eval += evaluate_king<WHITE>(ei)    - evaluate_king<BLACK>(ei);

    return eval;
}

//=================================================================
//! \brief  Evaluation des cavaliers d'une couleur
//-----------------------------------------------------------------
template<Color US>
Score Board::evaluate_knights(EvalInfo& ei)
{
    constexpr Color THEM = ~US;
    constexpr Direction DOWN = (US == WHITE) ? SOUTH : NORTH;

    int sq, sqpos;
    Score eval = 0;
    int defended;
    int outside;

    Bitboard bb = occupancy_cp<US, KNIGHT>();
    int count;
    ei.phase24 += BB::count_bit(bb);


    while (bb)
    {
        sq    = BB::pop_lsb(bb);                // case où est la pièce
        sqpos = SQ::relative_square<US>(sq);    // case inversée pour les tables
        eval += KnightValue;                    // score matériel
        eval += KnightPSQT[sqpos];              // score positionnel

#if defined USE_TUNER
        ownTuner.Trace.KnightValue[US]++;
        ownTuner.Trace.KnightPSQT[sqpos][US]++;
#endif


        // mobilité
        Bitboard attackBB   = Attacks::knight_moves(sq);
        Bitboard mobilityBB = attackBB & ei.mobilityArea[US];
        count = BB::count_bit(mobilityBB);
        eval += KnightMobility[count];

#if defined USE_TUNER
        ownTuner.Trace.KnightMobility[count][US]++;
#endif


        // Un avant-poste est une case sur laquelle votre pièce ne peut pas
        // être attaquée par un pion adverse.
        // L’avant-poste est particulièrement efficace quand il est défendu
        // par un de vos propres pions et qu’il permet de contrôler des cases clefs
        // de la position adverse.
        // Les cavaliers sont les pièces qui profitent le mieux des avant-postes.

        // Définition d'un avant-poste :
        //      + rangs = 4,5,6 (pour Blancs)
        //      + pas de pion ennemi sur les cases adjacentes et en avant
        //        soit le cavalier est déjà attaqué,
        //        soit un pion ennemi pourrait avancer et attaquer le cavalier

        if (BB::test_bit(ei.outposts[US], sq))
        {
            outside  = BB::test_bit(FILE_A_BB | FILE_H_BB, sq);
            defended = BB::test_bit(ei.attackedBy[US][PAWN], sq);
            eval += KnightOutpost[outside][defended];

#if defined DEBUG_EVAL
            if (defended)
            {
                if (outside)
                    printf("le cavalier %s en %s est sur un avant-poste défendu extérieur\n", camp[1][US].c_str(), square_name[sq].c_str());
                else
                    printf("le cavalier %s en %s est sur un avant-poste défendu intérieur\n", camp[1][US].c_str(), square_name[sq].c_str());
            }
            else
            {
                if (outside)
                    printf("le cavalier %s en %s est sur un avant-poste non défendu extérieur\n", camp[1][US].c_str(), square_name[sq].c_str());
                else
                    printf("le cavalier %s en %s est sur un avant-poste non défendu intérieur\n", camp[1][US].c_str(), square_name[sq].c_str());
            }
#endif
#if defined USE_TUNER
            ownTuner.Trace.KnightOutpost[outside][defended][US]++;
#endif
        }


        // Un pion (ami ou ennemi) fait un écran au cavalier.
        // Il ne peut pas être attaqué par une tour
        if (BB::test_bit(BB::shift<DOWN>(typePiecesBB[PAWN]), sq))
        {
            eval += KnightBehindPawn;

#if defined DEBUG_EVAL
            printf("le cavalier %s en %s est situé derrière un pion \n", camp[1][US].c_str(), square_name[sq].c_str());
#endif
#if defined USE_TUNER
            ownTuner.Trace.KnightBehindPawn[US]++;
#endif
        }

        //  Attaques sur le roi ennemi
        if (Bitboard kingRingAtks = ei.KingRing[THEM] & attackBB; kingRingAtks!=0)
        {
            // ei.kingAttackersCount[US]++;
            ei.KingAttackersWeight[US] += KingAttackersWeight[KNIGHT];
            ei.KingAttacksCount[US]    += BB::count_bit(kingRingAtks);

#if defined USE_TUNER
            ownTuner.Trace.KingAttackersWeight[KNIGHT][US]++;
#endif
#if defined DEBUG_EVAL
            printf("le cavalier %s en %s attaque le roi ennemi %d fois \n", camp[1][US].c_str(), square_name[sq].c_str(), BB::count_bit(kingRingAtks));
            BB::PrintBB(kingRingAtks, "kingRingAtks");
            BB::PrintBB(attackBB, "attackBB");
#endif
        }

        //  Update des attaques
        ei.attackedBy[US][KNIGHT] |= attackBB;
        ei.attackedBy2[US]        |= attackBB & ei.attacked[US];
        ei.attacked[US]           |= attackBB;
    }

    return eval;
}

//=================================================================
//! \brief  Evaluation des fous d'une couleur
//-----------------------------------------------------------------
template<Color US>
Score Board::evaluate_bishops(EvalInfo& ei)
{
    constexpr Color THEM = ~US;
    constexpr Direction DOWN = (US == WHITE) ? SOUTH : NORTH;

    int sq, sqpos;
    int count;

    Bitboard bishopSquares;
    Bitboard badPawns;          // pions de la couleur du fou
    Bitboard blockedPawns;

    Score    eval = 0;
    Bitboard bb   = occupancy_cp<US, BISHOP>();
    ei.phase24 += BB::count_bit(bb);

    // Paire de fous
    // On ne teste pas si les fous sont de couleur différente !
    if (BB::count_bit(bb) >= 2)
    {
        eval += BishopPair;

#if defined DEBUG_EVAL
        printf("Le camp %s possède la paire de fous \n", side_name[US].c_str());
#endif
#if defined USE_TUNER
        ownTuner.Trace.BishopPair[US]++;
#endif
    }


    while (bb)
    {
        sq    = BB::pop_lsb(bb);                   // case où est la pièce
        sqpos = SQ::relative_square<US>(sq);               // case inversée pour les tables
        eval += BishopValue;                       // score matériel
        eval += BishopPSQT[sqpos];                 // score positionnel

#if defined USE_TUNER
        ownTuner.Trace.BishopValue[US]++;
        ownTuner.Trace.BishopPSQT[sqpos][US]++;
#endif

        // mobilité
        Bitboard attackBB   = XRayBishopAttack<US>(sq);
        Bitboard mobilityBB = attackBB & ei.mobilityArea[US];
        count = BB::count_bit(mobilityBB);
        eval += BishopMobility[count];

#if defined USE_TUNER
        ownTuner.Trace.BishopMobility[count][US]++;
#endif


        // Mauvais fou
        // https://hxim.github.io/Stockfish-Evaluation-Guide/
        // Bishop pawns. Number of pawns on the same color square as the bishop multiplied by one plus the number of our blocked pawns in the center files C, D, E or F.
        bishopSquares = BB::test_bit(DarkSquares, sq) ? DarkSquares : LightSquares;    // Bitboard de la couleur du fou
        badPawns      = ei.pawns[US] & bishopSquares;                                   // Bitboard de nos pions de la couleur du fou
        blockedPawns  = ei.blockedPawns[US] & CenterFiles;                              // Bitboard de nos pions centraux bloqués
        //TODO prendre blocked ou rammed ?

        count = BB::count_bit(badPawns) * BB::count_bit(blockedPawns);
        eval += count * BishopBadPawn;

#if defined DEBUG_EVAL
        if (count)
            printf("le fou %s en %s a %d mauvais pions et %d pions bloqués centraux\n", camp[1][US].c_str(), square_name[sq].c_str(),
                   BB::count_bit(badPawns), BB::count_bit(blockedPawns));
#endif
#if defined USE_TUNER
        ownTuner.Trace.BishopBadPawn[US] += count;
#endif


        // Un pion (ami ou ennemi) fait un écran au fou.
        // Il ne peut pas être attaqué par une tour
        if (BB::test_bit(BB::shift<DOWN>(typePiecesBB[PAWN]), sq))
        {
            eval += BishopBehindPawn;

#if defined DEBUG_EVAL
            printf("le fou %s en %s est situé derrière un pion \n", camp[1][US].c_str(), square_name[sq].c_str());
#endif
#if defined USE_TUNER
            ownTuner.Trace.BishopBehindPawn[US]++;
#endif
        }


        // Bonus for bishop on a long diagonal which can "see" both center squares
        if (BB::multiple(Attacks::bishop_moves(sq, occupancy_p<PAWN>()) & CenterSquares))
        {
            eval += BishopLongDiagonal;

#if defined DEBUG_EVAL
            printf("le fou %s en %s contrôle le centre \n", camp[1][US].c_str(), square_name[sq].c_str());
#endif
#if defined USE_TUNER
            ownTuner.Trace.BishopLongDiagonal[US]++;
#endif
        }

        //  Attaques sur le roi ennemi
        if (Bitboard kingRingAtks = ei.KingRing[THEM] & attackBB; kingRingAtks!=0)
        {
            // ei.kingAttackersCount[US]++;
            ei.KingAttackersWeight[US] += KingAttackersWeight[BISHOP];
            ei.KingAttacksCount[US]    += BB::count_bit(kingRingAtks);

#if defined USE_TUNER
            ownTuner.Trace.KingAttackersWeight[BISHOP][US]++;
#endif
#if defined DEBUG_EVAL
            printf("le fou %s en %s attaque le roi ennemi %d fois \n", camp[1][US].c_str(), square_name[sq].c_str(), BB::count_bit(kingRingAtks));
            BB::PrintBB(kingRingAtks, "kingRingAtks");
            BB::PrintBB(attackBB, "attackBB");
#endif
        }

        //  Update des attaques
        ei.attackedBy[US][BISHOP] |= attackBB;
        ei.attackedBy2[US]        |= attackBB & ei.attacked[US];
        ei.attacked[US]           |= attackBB;
    }

    return eval;
}

//=================================================================
//! \brief  Evaluation des tours d'une couleur
//-----------------------------------------------------------------
template<Color US>
Score Board::evaluate_rooks(EvalInfo& ei)
{
    constexpr Color THEM = ~US;
    constexpr Direction DOWN = (US == WHITE) ? SOUTH : NORTH;

    int sq, sqpos;
    int count;
    Score eval = 0;

    Bitboard bb = occupancy_cp<US, ROOK>();
    ei.phase24 += 2*BB::count_bit(bb);

    while (bb)
    {
        sq     = BB::pop_lsb(bb);                   // case où est la pièce
        sqpos  = SQ::relative_square<US>(sq);               // case inversée pour les tables
        eval += RookValue;                         // score matériel
        eval += RookPSQT[sqpos];                   // score positionnel

#if defined USE_TUNER
        ownTuner.Trace.RookValue[US]++;
        ownTuner.Trace.RookPSQT[sqpos][US]++;
#endif


        // mobilité
        Bitboard attackBB   = XRayRookAttack<US>(sq);
        Bitboard mobilityBB = attackBB & ei.mobilityArea[US];
        count = BB::count_bit(mobilityBB);
        eval +=  RookMobility[count];

#if defined USE_TUNER
        ownTuner.Trace.RookMobility[count][US]++;
#endif


        // Colonnes ouvertes et semi-ouvertes (Stockfish)
        // Note : cette définition ne tient pas compte de la position
        //        relative de la tour et du pion
        if (is_on_semiopen_file<US>(sq))    // pas de pion ami
        {
            eval += RookOnOpenFile[is_on_semiopen_file<THEM>(sq)];

#if defined DEBUG_EVAL
            if (is_on_semiopen_file<THEM>(sq))
                printf("la tour %s en %s est sur une colonne ouverte \n", camp[0][US].c_str(), square_name[sq].c_str());
            else
                printf("la tour %s en %s est sur une colonne semi-ouverte \n", camp[0][US].c_str(), square_name[sq].c_str());
#endif
#if defined USE_TUNER
            ownTuner.Trace.RookOnOpenFile[is_on_semiopen_file<THEM>(sq)][US]++;
#endif
        }
        else
        {
            // If our pawn on this file is blocked, increase penalty (Stockfish)
            if ( ei.pawns[US]
                & BB::shift<DOWN>(ei.occupied)
                & FileMask64[sq] )
            {
                eval += RookOnBlockedFile;

#if defined DEBUG_EVAL
                printf("tour %s en %s : le pion sur la colonne est bloqué \n", camp[0][US].c_str(), square_name[sq].c_str());
#endif
#if defined USE_TUNER
                ownTuner.Trace.RookOnBlockedFile[US]++;
#endif
            }
        }

        // à ajouter
        //  + tour dirigée vers le roi ennemi, la dame ennemie ?
        //  + tour coincée (stockfish)


        //  Attaques sur le roi ennemi
        if (Bitboard kingRingAtks = ei.KingRing[THEM] & attackBB; kingRingAtks!=0)
        {
            // ei.kingAttackersCount[US]++;
            ei.KingAttackersWeight[US] += KingAttackersWeight[ROOK];
            ei.KingAttacksCount[US]    += BB::count_bit(kingRingAtks);

#if defined USE_TUNER
            ownTuner.Trace.KingAttackersWeight[ROOK][US]++;
#endif
#if defined DEBUG_EVAL
            printf("la tour %s en %s attaque le roi ennemi %d fois \n", camp[1][US].c_str(), square_name[sq].c_str(), BB::count_bit(kingRingAtks));
            BB::PrintBB(kingRingAtks, "kingRingAtks");
#endif
        }

        //  Update des attaques
        ei.attackedBy[US][ROOK]   |= attackBB;
        ei.attackedBy2[US]        |= attackBB & ei.attacked[US];
        ei.attacked[US]           |= attackBB;
    }

    return eval;
}

//=================================================================
//! \brief  Evaluation des Dames d'une couleur
//-----------------------------------------------------------------
template<Color US>
Score Board::evaluate_queens(EvalInfo& ei)
{
    constexpr Color THEM = ~US;

    int sq, sqpos;
    int count;
    Score eval = 0;
    Bitboard bb = occupancy_cp<US, QUEEN>();
    ei.phase24 += 4*BB::count_bit(bb);

    while (bb)
    {
        sq     = BB::pop_lsb(bb);                   // case où est la pièce
        sqpos  = SQ::relative_square<US>(sq);               // case inversée pour les tables
        eval += QueenValue;                        // score matériel
        eval += QueenPSQT[sqpos];                  // score positionnel

#if defined USE_TUNER
        ownTuner.Trace.QueenValue[US]++;
        ownTuner.Trace.QueenPSQT[sqpos][US]++;
#endif

        // mobilité
        Bitboard attackBB   = XRayQueenAttack<US>(sq);
        Bitboard mobilityBB = attackBB & ei.mobilityArea[US];
        count = BB::count_bit(mobilityBB);
        eval += QueenMobility[count];

#if defined USE_TUNER
        ownTuner.Trace.QueenMobility[count][US]++;
#endif

        //  La Dame peut être victime d'une attaque à la découverte
        if (discoveredAttacks<US>(sq))
        {
            eval += QueenRelativePin;

#if defined DEBUG_EVAL
            printf("la dame %s en %s pourrait subir une attaque à la découverte \n", camp[0][US].c_str(), square_name[sq].c_str());
#endif
#if defined USE_TUNER
            ownTuner.Trace.QueenRelativePin[US]++;
#endif
        }


        //  Attaques sur le roi ennemi
        if (Bitboard kingRingAtks = ei.KingRing[THEM] & attackBB; kingRingAtks!=0)
        {
            // ei.kingAttackersCount[US]++;
            ei.KingAttackersWeight[US] += KingAttackersWeight[QUEEN];
            ei.KingAttacksCount[US]    += BB::count_bit(kingRingAtks);

#if defined USE_TUNER
            ownTuner.Trace.KingAttackersWeight[QUEEN][US]++;
#endif
#if defined DEBUG_EVAL
            printf("la dame %s en %s attaque le roi ennemi %d fois \n", camp[1][US].c_str(), square_name[sq].c_str(), BB::count_bit(kingRingAtks));
            BB::PrintBB(kingRingAtks, "kingRingAtks");
#endif
        }


        //  Update des attaques
        ei.attackedBy[US][QUEEN]  |= attackBB;
        ei.attackedBy2[US]        |= attackBB & ei.attacked[US];
        ei.attacked[US]           |= attackBB;
    }

    return eval;
}

//=================================================================
//! \brief  Evaluation du roi d'une couleur
//-----------------------------------------------------------------
template<Color US>
Score Board::evaluate_king(EvalInfo& ei)
{
    constexpr Color THEM = ~US;

    int sq, sqpos;
    Score eval = 0;
    sq     = x_king[US];
    sqpos  = SQ::relative_square<US>(sq);               // case inversée pour les tables
    eval += KingValue;                         // score matériel
    eval += KingPSQT[sqpos];                   // score positionnel

#if defined USE_TUNER
    ownTuner.Trace.KingPSQT[sqpos][US]++;
#endif

    // Le roi attaque un pion
    if (Attacks::king_moves(sq) & occupancy_cp<THEM, PAWN>())
    {
        eval += KingAttackPawn;

#if defined DEBUG_EVAL
        printf("le roi %s en %s attaque un pion \n", camp[0][US].c_str(), square_name[sq].c_str());
#endif
#if defined USE_TUNER
        ownTuner.Trace.KingAttackPawn[US]++;
#endif
    }

    // Bouclier du roi
    Bitboard pawnsInFront = typePiecesBB[PAWN] & PassedPawnMask[US][king_square<US>()];
    Bitboard ourPawns = pawnsInFront & colorPiecesBB[US] & ~BB::all_pawn_attacks<THEM>(ei.pawns[THEM]);

    int count = BB::count_bit(ourPawns);
    eval += count * PawnShelter;

#if defined DEBUG_EVAL
    printf("le roi %s en %s possède un bouclier de %d pions \n", camp[1][US].c_str(), square_name[sq].c_str(), count);
#endif
#if defined USE_TUNER
    ownTuner.Trace.PawnShelter[US] += count;
#endif

    return eval;
}

//=================================================================
//! \brief  Evaluation des menaces
//-----------------------------------------------------------------
template<Color US>
Score Board::evaluate_threats(const EvalInfo& ei)
{
    constexpr Color THEM        = ~US;
    constexpr Direction UP      = (US == WHITE) ? NORTH : SOUTH;
    constexpr Bitboard Rank3Rel = (US == WHITE) ? RANK_3_BB : RANK_6_BB;

    int sq;
    int attacked;
    Score eval = 0;

    const Bitboard covered        = ei.attackedBy[THEM][PAWN] | (ei.attackedBy2[THEM] & ~ei.attackedBy2[US]);
    const Bitboard nonPawnEnemies = occupancy_c<THEM>() & ~occupancy_cp<THEM, PAWN>();
    const Bitboard weak           = ~covered & ei.attacked[US];
    Bitboard threats;

    for (int piece = KNIGHT; piece <= ROOK; piece++)
    {
        threats = occupancy_c<THEM>() & ei.attackedBy[US][piece];
        if (piece == KNIGHT || piece == BISHOP)
            threats &= nonPawnEnemies | weak;
        else
            threats &= weak;

        while (threats)
        {
            sq = BB::pop_lsb(threats);  // case de la pièce attaquée
            attacked = pieceOn[sq];     // la pièce attaquée

            switch (piece)              // la pièce attaquante
            {
            case KNIGHT:
                eval += ThreatByKnight[attacked];
#if defined USE_TUNER
                ownTuner.Trace.ThreatByKnight[attacked][US]++;
#endif
                break;

            case BISHOP:
                eval += ThreatByBishop[attacked];
#if defined USE_TUNER
                ownTuner.Trace.ThreatByBishop[attacked][US]++;
#endif
                break;

            case ROOK:
                eval += ThreatByRook[attacked];
#if defined USE_TUNER
                ownTuner.Trace.ThreatByRook[attacked][US]++;
#endif
                break;

            default:
                break;
            }
        }
    }

    // Roi
    Bitboard kingThreats = weak & ei.attackedBy[US][KING] & occupancy_c<THEM>();
    if (kingThreats)
    {
        eval += ThreatByKing;
#if defined USE_TUNER
        ownTuner.Trace.ThreatByKing[US]++;
#endif
    }

    // Pièces attaquées et non défendues
    Bitboard hangingPieces = occupancy_c<THEM>() & ~ei.attacked[THEM] & ei.attacked[US];
    eval += BB::count_bit(hangingPieces) * HangingThreat;
#if defined USE_TUNER
    ownTuner.Trace.HangingThreat[US] += BB::count_bit(hangingPieces);
#endif

    // Pièces attaquées par des pions
    Bitboard pawnThreats = nonPawnEnemies & ei.attackedBy[US][PAWN];
    eval += BB::count_bit(pawnThreats) * PawnThreat;

    // Pièces pouvant être attaquées par l'avance de pion
    Bitboard pawnPushes = BB::shift<UP>(ei.pawns[US])          & ~ei.occupied;  // avance d'1 case des pions
    pawnPushes         |= BB::shift<UP>(pawnPushes & Rank3Rel) & ~ei.occupied;  // avance d'1 case des pions, SI il sont sur la 3ème rangée
    pawnPushes         &= (ei.attacked[US] | ~ei.attacked[THEM]);               // pions protégés et non attaqués
    Bitboard pawnPushAttacks = BB::all_pawn_attacks<US>(pawnPushes);
    pawnPushAttacks         &= nonPawnEnemies;

    eval += BB::count_bit(pawnPushAttacks)          * PawnPushThreat
            + BB::count_bit(pawnPushAttacks & get_pinned()) * PawnPushPinnedThreat;

#if defined USE_TUNER
    ownTuner.Trace.PawnThreat[US]            += BB::count_bit(pawnThreats);
    ownTuner.Trace.PawnPushThreat[US]        += BB::count_bit(pawnPushAttacks) ;
    ownTuner.Trace.PawnPushPinnedThreat[US]  += BB::count_bit(pawnPushAttacks & get_pinned());
#endif

    // Attaque de la dame ennemie
    Bitboard bb = occupancy_cp<THEM, QUEEN>();
    if (bb)
    {
        int oppQueenSquare = BB::pop_lsb(bb);

        Bitboard knightQueenHits = Attacks::knight_moves(oppQueenSquare)
                                   & ei.attackedBy[US][KNIGHT]
                                   & ~occupancy_cp<US, PAWN>()
                                   & ~covered;
        eval += BB::count_bit(knightQueenHits) * KnightCheckQueen;

        Bitboard bishopQueenHits = Attacks::bishop_moves(oppQueenSquare, ei.occupied)
                                   & ei.attackedBy[US][BISHOP]
                                   & ~occupancy_cp<US, PAWN>()
                                   & ~covered
                                   & ei.attackedBy2[US];
        eval += BB::count_bit(bishopQueenHits) * BishopCheckQueen;

        Bitboard rookQueenHits = Attacks::rook_moves(oppQueenSquare, ei.occupied)
                                 & ei.attackedBy[US][ROOK]
                                 & ~occupancy_cp<US, PAWN>()
                                 & ~covered
                                 & ei.attackedBy2[US];
        eval += BB::count_bit(rookQueenHits) * RookCheckQueen;

#if defined USE_TUNER
        ownTuner.Trace.KnightCheckQueen[US] += BB::count_bit(knightQueenHits);
        ownTuner.Trace.BishopCheckQueen[US] += BB::count_bit(bishopQueenHits);
        ownTuner.Trace.RookCheckQueen[US]   += BB::count_bit(rookQueenHits) ;
#endif
    }

    return eval;
}

//=================================================================
//! \brief  Evaluation des attaques sur le roi ennemi.
//-----------------------------------------------------------------
template<Color US>
Score Board::evaluate_king_attacks(const EvalInfo& ei) const
{
    // Code provenant de Sirius 8;  voir aussi Stockfish

    constexpr Color THEM = ~US;
    const int theirKing = king_square<THEM>();

    Score eval = 0;

    Bitboard rookCheckSquares   = Attacks::rook_moves(theirKing, ei.occupied);
    Bitboard bishopCheckSquares = Attacks::bishop_moves(theirKing, ei.occupied);

    Bitboard knightChecks = ei.attackedBy[US][KNIGHT] & Attacks::knight_moves(theirKing);
    Bitboard bishopChecks = ei.attackedBy[US][BISHOP] & bishopCheckSquares;
    Bitboard rookChecks   = ei.attackedBy[US][ROOK] & rookCheckSquares;
    Bitboard queenChecks  = ei.attackedBy[US][QUEEN] & (bishopCheckSquares | rookCheckSquares);

    // Cases non attaquées par l'ennemi; on ne tient pas compte des attaques du roi ennemi
    Bitboard safe = ~ei.attacked[THEM] | (~ei.attackedBy2[THEM] & ei.attackedBy[THEM][KING]);

    eval += SafeCheck[KNIGHT] * BB::count_bit(knightChecks & safe);
    eval += SafeCheck[BISHOP] * BB::count_bit(bishopChecks & safe);
    eval += SafeCheck[ROOK]   * BB::count_bit(rookChecks   & safe);
    eval += SafeCheck[QUEEN]  * BB::count_bit(queenChecks  & safe);

#if defined USE_TUNER
    ownTuner.Trace.SafeCheck[KNIGHT][US]    += BB::count_bit(knightChecks & safe);
    ownTuner.Trace.SafeCheck[BISHOP][US]    += BB::count_bit(bishopChecks & safe);
    ownTuner.Trace.SafeCheck[ROOK][US]      += BB::count_bit(rookChecks   & safe);
    ownTuner.Trace.SafeCheck[QUEEN][US]     += BB::count_bit(queenChecks  & safe);
#endif

    eval += UnsafeCheck[KNIGHT] * BB::count_bit(knightChecks & ~safe);
    eval += UnsafeCheck[BISHOP] * BB::count_bit(bishopChecks & ~safe);
    eval += UnsafeCheck[ROOK]   * BB::count_bit(rookChecks   & ~safe);
    eval += UnsafeCheck[QUEEN]  * BB::count_bit(queenChecks  & ~safe);

#if defined USE_TUNER
    ownTuner.Trace.UnsafeCheck[KNIGHT][US]  += BB::count_bit(knightChecks & ~safe);
    ownTuner.Trace.UnsafeCheck[BISHOP][US]  += BB::count_bit(bishopChecks & ~safe);
    ownTuner.Trace.UnsafeCheck[ROOK][US]    += BB::count_bit(rookChecks   & ~safe);
    ownTuner.Trace.UnsafeCheck[QUEEN][US]   += BB::count_bit(queenChecks  & ~safe);
#endif

    // Score des attaques sur le Roi; c'est la somme des différentes pièces amies
    // Le tuner est appelé pour chaque pièce
    eval += ei.KingAttackersWeight[US];

    // Nombre d'attaques sur le Roi
    int attackCount = std::min(ei.KingAttacksCount[US], 13);
    eval += KingAttacksCount[attackCount];

#if defined DEBUG_EVAL
    printf("le roi ennemi %s en %s subit %d attaques\n", camp[1][THEM].c_str(), square_name[theirKing].c_str(), attackCount);
#endif
#if defined USE_TUNER
    ownTuner.Trace.KingAttacksCount[attackCount][US]++;
#endif

    return eval;
}

//=====================================================================
//! \brief  Calcule un facteur d'échelle pour tenir compte
//! de divers facteurs dans l'évaluation
//---------------------------------------------------------------------
int Board::scale_factor(const Score eval)
{
    // Code provenant de Weiss

    Bitboard strongPawns = eval > 0 ? occupancy_cp<WHITE, PAWN>()
                                    : occupancy_cp<BLACK, PAWN>();

    // Scale down eval the fewer pawns the stronger side has
    int strongPawnCount = BB::count_bit(strongPawns);
    int x = 8 - strongPawnCount;
    int pawnScale = 128 - x * x;

    // Scale down when there aren't pawns on both sides of the board
    if (!(strongPawns & QueenSide) || !(strongPawns & KingSide))
        pawnScale -= 20;

    // Scale down eval for opposite-colored bishops endgames
    if (   getNonPawnMaterialCount<WHITE>() <= 2
        && getNonPawnMaterialCount<BLACK>() <= 2
        && getNonPawnMaterialCount<WHITE>() == getNonPawnMaterialCount<BLACK>()
        && BB::single(occupancy_cp<WHITE, BISHOP>())
        && BB::single(occupancy_cp<BLACK, BISHOP>())
        && BB::single(occupancy_p<BISHOP>() & DarkSquares))
    {
        return std::min((getNonPawnMaterialCount<WHITE>() == 1 ? 64 : 96), pawnScale);
    }

    return pawnScale;
}

//=============================================================
//! \brief  Initialisation des informations de l'évaluation
//-------------------------------------------------------------
void Board::init_eval_info(EvalInfo& ei)
{
    // Initialisation des Bitboards
    constexpr Bitboard  RelRanK2BB[2] = {                          // La rangée 2, relativement à la couleur
                                        RANK_BB[SQ::relative_rank8<WHITE>(RANK_2)],
                                        RANK_BB[SQ::relative_rank8<BLACK>(RANK_2)],
                                        };

    ei.phase24 = 0;

    ei.occupied     = occupancy_all();                                   // toutes les pièces (Blanches + Noires)

    ei.pawns[WHITE] = occupancy_cp<WHITE, PAWN>();
    ei.pawns[BLACK] = occupancy_cp<BLACK, PAWN>();

    // ei.rammedPawns[WHITE] = BB::all_pawn_advance<BLACK>(ei.pawns[BLACK], ~(ei.pawns[WHITE]));
    // ei.rammedPawns[BLACK] = BB::all_pawn_advance<WHITE>(ei.pawns[WHITE], ~(ei.pawns[BLACK]));

    ei.blockedPawns[WHITE] = BB::all_pawn_advance<BLACK>(ei.occupied, ~(ei.pawns[WHITE]));
    ei.blockedPawns[BLACK] = BB::all_pawn_advance<WHITE>(ei.occupied, ~(ei.pawns[BLACK]));

    // La zone de mobilité est définie ainsi (idée de Weiss) : toute case
    //  > ni attaquée par un pion adverse
    //  > ni occupée par un pion ami sur sa case de départ
    //  > ni occupée par un pion ami bloqué

    const Bitboard b[2] = {
        (RelRanK2BB[WHITE] | BB::shift<SOUTH>(ei.occupied)) & ei.pawns[WHITE],
        (RelRanK2BB[BLACK] | BB::shift<NORTH>(ei.occupied)) & ei.pawns[BLACK]
    };

    ei.mobilityArea[WHITE] = ~(b[WHITE] | BB::all_pawn_attacks<BLACK>(ei.pawns[BLACK]));
    ei.mobilityArea[BLACK] = ~(b[BLACK] | BB::all_pawn_attacks<WHITE>(ei.pawns[WHITE]));

    ei.KingAttackersWeight[WHITE] = 0;
    ei.KingAttackersWeight[BLACK] = 0;
    ei.KingAttacksCount[WHITE] = 0;
    ei.KingAttacksCount[BLACK] = 0;

    // Clear passed pawns, filled in during pawn eval
    ei.passedPawns = 0;

    for (int c=0; c<N_COLORS; c++)
    {
        ei.attacked[c]    = 0;
        ei.attackedBy2[c] = 0;

        for (int p=0; p<N_PIECES; p++)
        {
            ei.attackedBy[c][p] = 0;
        }
    }

    ei.attacked[WHITE] = ei.attackedBy[WHITE][PAWN] = BB::all_pawn_attacks<WHITE>(ei.pawns[WHITE]);

    // Le KingRing est constitué des cases autour du roi,
    // ainsi que des 3 cases au Nord pour les Blancs,
    //                       au Sud pour les Noirs

    Bitboard whiteKingAtks = Attacks::king_moves(king_square<WHITE>());
    ei.attackedBy[WHITE][KING] = whiteKingAtks;
    ei.attackedBy2[WHITE] = ei.attacked[WHITE] & whiteKingAtks;
    ei.attacked[WHITE] |= whiteKingAtks;
    ei.KingRing[WHITE] = (whiteKingAtks | BB::north(whiteKingAtks)) & ~SQ::square_BB(king_square<WHITE>());

    ei.attacked[BLACK] = ei.attackedBy[BLACK][PAWN] = BB::all_pawn_attacks<BLACK>(ei.pawns[BLACK]);

    Bitboard blackKingAtks = Attacks::king_moves(king_square<BLACK>());
    ei.attackedBy[BLACK][KING] = blackKingAtks;
    ei.attackedBy2[BLACK] = ei.attacked[BLACK] & blackKingAtks;
    ei.attacked[BLACK] |= blackKingAtks;
    ei.KingRing[BLACK] = (blackKingAtks | BB::south(blackKingAtks)) & ~SQ::square_BB(king_square<BLACK>());

    // Berserk / Stockfish modifiée
    ei.outposts[WHITE] =   (RANK_4_BB | RANK_5_BB | RANK_6_BB)              // rangées 4,5,6
                         & ~BB::fill<SOUTH>(ei.attackedBy[BLACK][PAWN]);    // cases non attaquables par un pion noir,
    // depuis sa case de départ, ou après un déplacement
    // & (  ei.attackedBy[WHITE][PAWN]                    // protégé par un pion blanc
    //    | BB::shift<SOUTH>(typePiecesBB[PAWN]))         // derrière un pion (blanc ou noir)

    ei.outposts[BLACK] =   (RANK_3_BB | RANK_4_BB | RANK_5_BB)
                         & ~BB::fill<NORTH>(ei.attackedBy[WHITE][PAWN]);
    // & (  ei.attackedBy[BLACK][PAWN]                    //
    //    | BB::shift<NORTH>(ei.pawns[WHITE] | ei.pawns[BLACK]))

}

