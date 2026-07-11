#include <cmath>
#include <cstring>
#include "Search.h"
#include "ThreadPool.h"
#include "Move.h"
#include "TranspositionTable.h"


//=============================================
//! \brief  Constructeur
//---------------------------------------------
Search::Search() : stopFlagPtr(&stopped)
{
#if defined DEBUG_LOG
    char message[100];
    sprintf(message, "Search constructeur");
    printlog(message);
#endif

       init_reductions();
}

//=============================================
//! \brief  Initialise la table de réduction LMR
//---------------------------------------------
void Search::init_reductions()
{
    Reductions[0][0][0] = 0;
    Reductions[1][0][0] = 0;

    for (int d = 1; d < 32; ++d)
        for (int m = 1; m < 32; ++m)
        {
            Reductions[0][d][m] = Tunable::LMR_CaptureBase / 100.0 + log(d) * log(m) / (Tunable::LMR_CaptureDivisor / 100.0);
            Reductions[1][d][m] = Tunable::LMR_QuietBase   / 100.0 + log(d) * log(m) / (Tunable::LMR_QuietDivisor   / 100.0);
        }
}

//=============================================
//! \brief  Destructeur
//---------------------------------------------
Search::~Search()
{
}

//=========================================================
//! \brief  Affichage UCI du résultat de la recherche
//!
//! \param[in] elapsed      temps passé pour la recherche, en millisecondes
//! \param[in] pv           variation principale à afficher
//---------------------------------------------------------
void Search::show_uci_result(I64 elapsed, const PVariation& pv) const
{
    elapsed++; // évite une division par 0
    // commande envoyée à UCI
    // voir le document : uci_commands.txt

    std::stringstream stream;


    /*  score
     *      cp <x>
     *          le score du point de vue du moteur, en centipions.
     *      mate <y>
     *          mat en y coups, pas en plies.
     *          Si le moteur se fait mater, utiliser des valeurs négatives pour y.
     */

    // Rassemble les infos sur les nodes de toutes les threads
    U64 all_nodes  = threadPool.get_all_nodes();
    U64 all_tbhits = threadPool.get_all_tbhits();
    int hash_full  = table->hash_full();

    stream << "info "
           << " depth "    << best_depth
           << " seldepth " << seldepth

// time     : le temps de recherche, en ms
// nodes    : noeuds calculés
// nps      : nodes par seconde recherchés

           << " time "       << elapsed
           << " nodes "      << all_nodes
           << " nps "        << all_nodes * 1000 / elapsed
           << " tbhits "     << all_tbhits
           << " hashfull "   << hash_full;

    const int iter_best_score = pv_scores[best_depth];

    if (iter_best_score >= MATE_IN_X)
    {
        stream << " score mate " << (MATE - iter_best_score) / 2 + 1;             // score mate <y> : mat en y coups, pas en plies.
    }
    else if (iter_best_score <= -MATE_IN_X)
    {
        stream << " score mate " << (-MATE - iter_best_score) / 2;
    }
    else
    {
        stream << " score cp " << iter_best_score;
    }

    stream << " pv";
    for (int i=0; i<pv.length; i++)
        stream << " " << Move::name(pv.line[i]);

    std::cout << stream.str() << std::endl;
}

//=========================================================
//! \brief  Affichage UCI du meilleur coup trouvé
//!         N'est utilisé que pour le thread 0
//!
//! \param[in]  best_move   meilleur coup trouvé, en notation UCI
//---------------------------------------------------------
void Search::show_uci_best(MOVE best_move) const
{
    // ATTENTION AU FORMAT D'AFFICHAGE
    std::cout << "bestmove " << Move::name(best_move) << std::endl;
}

//=========================================================
//! \brief  Mise à jour de la Principal variation
//!
//! \param[in,out]  si     nœud courant de la pile de recherche (sa PV est reconstruite)
//! \param[in]      move   coup joué à ce nœud, placé en tête de la PV
//---------------------------------------------------------
void Search::update_pv(SearchInfo* si, const MOVE move) const
{
    si->pv.length = 1 + (si+1)->pv.length;
    si->pv.line[0] = move;
    memcpy(si->pv.line+1, (si+1)->pv.line, sizeof(MOVE) * (si+1)->pv.length);
}


//==========================================
//! \brief  Evaluation de la position
//!
//! \param[in]  board   position à évaluer
//!
//! \return Evaluation statique de la position
//! du point de vue du camp au trait.
//------------------------------------------
[[nodiscard]] int Search::evaluate(const Board& board)
{
    // Du fait de lazy Updates, on a découpé la routine en 2

    Accumulator& acc = get_accumulator();

   // Lazy Updates
   nnue.lazy_updates(board, acc);

   return do_evaluate(board, acc);
}

//==========================================
//! \brief  Evaluation de la position à partir d'un accumulateur déjà à jour
//!
//! \param[in]  board   position à évaluer
//! \param[in]  acc     accumulateur NNUE correspondant à la position
//!
//! \return Evaluation statique de la position
//! du point de vue du camp au trait.
//------------------------------------------
[[nodiscard]] int Search::do_evaluate(const Board& board, Accumulator& acc)
{
    int value;
    if (board.side_to_move == WHITE)
        value = nnue.evaluate<WHITE>(acc, BB::count_bit(board.occupancy_all()));
    else
        value = nnue.evaluate<BLACK>(acc, BB::count_bit(board.occupancy_all()));

    constexpr int PawnScore    =   2;
    constexpr int KnightScore  =   3;
    constexpr int BishopScore  =   3;
    constexpr int RookScore    =   5;
    constexpr int QueenScore   =  12;
    constexpr int ScoreBias    = 230;
    constexpr int ScoreDivisor = 330;

    int phase = PawnScore   * BB::count_bit(board.occupancy_p<PieceType::PAWN>())
              + KnightScore * BB::count_bit(board.occupancy_p<PieceType::KNIGHT>())
              + BishopScore * BB::count_bit(board.occupancy_p<PieceType::BISHOP>())
              + RookScore   * BB::count_bit(board.occupancy_p<PieceType::ROOK>())
              + QueenScore  * BB::count_bit(board.occupancy_p<PieceType::QUEEN>());

    value = value * (ScoreBias + phase) / ScoreDivisor;

    // S'assure que l'évaluation ne se mélange pas avec les scores garantis de victoire/défaite
    value = std::clamp(value, -TBWIN_IN_X + 1, TBWIN_IN_X - 1);

    return value;
}

//==================================================
//! \brief  Réalise un coup
//! On met à jour à la fois la position, et le réseau
//!
//! \param[in,out]  board   position sur laquelle jouer le coup
//! \param[in]      move    coup à jouer
//--------------------------------------------------
template <Color US, bool Update_NNUE>
void Search::make_move(Board& board, const MOVE move) noexcept
{
    // Lazy Updates
    // On ne stocke que les modifications à faire,
    // mais on ne les applique pas.

    if constexpr (Update_NNUE == true)
    {
        nnue.push();    // nouvel accumulateur
    }
    Accumulator& accum = nnue.get_accumulator();

    // Update de la position
    board.make_move<US, Update_NNUE>(accum, move);

    // Empile le changement d'accumulateur
    accum.updated[WHITE] = false;
    accum.updated[BLACK] = false;
    accum.king_square    = board.king_square;
}

//=============================================================
//! \brief  Enlève un coup
//! On met à jour à la fois la position, et le réseau
//!
//! \param[in,out]  board   position sur laquelle annuler le dernier coup joué
//-------------------------------------------------------------
template <Color US, bool Update_NNUE>
void Search::undo_move(Board& board) noexcept
{
    if constexpr (Update_NNUE == true)
        nnue.pop(); // retourne a l'accumulateur précédent

    board.undo_move<US>();
}

// Instanciations explicites.
template void Search::make_move<WHITE, true>(Board& board, const U32 move) noexcept;
template void Search::make_move<BLACK, true>(Board& board, const U32 move) noexcept;
template void Search::make_move<WHITE, false>(Board& board, const U32 move) noexcept;
template void Search::make_move<BLACK, false>(Board& board, const U32 move) noexcept;

template void Search::undo_move<WHITE, true>(Board& board) noexcept ;
template void Search::undo_move<BLACK, true>(Board& board) noexcept ;
template void Search::undo_move<WHITE, false>(Board& board) noexcept ;
template void Search::undo_move<BLACK, false>(Board& board) noexcept ;
