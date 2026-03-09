
// Fichier inspiré de Weiss et Ethereal

#include "pyrrhic/tbprobe.h"
#include "Board.h"
#include "TranspositionTable.h"
#include "ThreadPool.h"
#include "Move.h"

/*
https://syzygy-tables.info/
https://github.com/AndyGrant/Pyrrhic
*/


//=========================================================================
//! \brief Converts a tbresult into a score
//! \param[in] wdl  result
//! \param[in] dtz  ply
//-------------------------------------------------------------------------
void Board::TBScore(const unsigned wdl, const unsigned dtz, int& score, int& bound) const
{
    // Convert the WDL value to a score. We consider blessed losses
    // and cursed wins to be a draw, and thus set value to zero.

    // Identify the bound based on WDL scores. For wins and losses the
    // bound is not exact because we are dependent on the height, but
    // for draws (and blessed / cursed) we know the tbresult to be exact

    switch (wdl)
    {
    case TB_WIN:
        score = TBWIN - dtz;
        bound = BOUND_LOWER;
        break;
    case TB_LOSS:
        score = -TBWIN + dtz;
        bound = BOUND_UPPER;
        break;
    default:
        score = 0;
        bound = BOUND_EXACT;
        break;
    }
}

//=========================================================================
//! \brief Probe the Win-Draw-Loss (WDL) table.
//-------------------------------------------------------------------------
bool Board::probe_wdl(int& score, int& bound, int ply) const
{
    // Don't probe at root, when castling is possible, or when 50 move rule
    // was not reset by the last move. Finally, there is obviously no point
    // if there are more pieces than we have TBs for.

    int probeLimit = threadPool.get_syzygyProbeLimit();
    int pieceCount = BB::count_bit(occupancy_all());

    if (   ply == 0
        || get_status().castling
        || get_status().fiftymove_counter
        || pieceCount > TB_LARGEST
        || (probeLimit > 0 && pieceCount > probeLimit))
        return false;

 //   std::cout << display() << std::endl;

 //     printf("probe_wdl : ply=%d cas=%d half=%d BC=%d-%d : ", ply, castling, halfmove_counter, BB::count_bit(occupied()), TB_LARGEST);

    // Tap into Pyrrhic's API. Pyrrhic takes the board representation, followed
    // by the enpass square (0 if none set), and the turn. Pyrrhic defines WHITE
    // as 1, and BLACK as 0, which is the opposite of how Ethereal defines them

    unsigned result = tb_probe_wdl(
        occupancy_c<WHITE>(),  occupancy_c<BLACK>(),
        occupancy_p<PieceType::KING>(),   occupancy_p<PieceType::QUEEN>(),
        occupancy_p<PieceType::ROOK>(),   occupancy_p<PieceType::BISHOP>(),
        occupancy_p<PieceType::KNIGHT>(), occupancy_p<PieceType::PAWN>(),
        get_status().ep_square == SQUARE_NONE ? 0 : get_status().ep_square,
        turn() == WHITE ? 1 : 0);

    // Probe failed
    if (result == TB_RESULT_FAILED)
    {
 //      printf(" failed \n");
        return false;
    }

    TBScore(result, ply, score, bound);
//    printf("probe_wdl success : score=%d ply=%d tbwin=%d tbloss=%d \n", score, ply, TB_WIN, TB_LOSS);

    return true;
}

//=========================================================================
//! \brief Probe Syzygy Tables at the root.
//!
//! Retourne true  → la position est NULLE ou PERDUE : 'move' est le coup DTZ-optimal
//!                   à jouer immédiatement ; win_count vaut 0.
//! Retourne false → la position est GAGNANTE ou hors des TB :
//!                   si win_count > 0, win_moves contient tous les coups gagnants TB
//!                   et la recherche doit se limiter à ces coups.
//!                   si win_count == 0, la position n'est pas dans les TB, recherche normale.
//!
//! This function should not be used during search.
//-------------------------------------------------------------------------
bool Board::probe_root(MOVE& move, MoveList& root_moves) const
{
    move      = Move::MOVE_NONE;
    root_moves.count = 0;

    // We cannot probe when there are castling rights, or when
    // we have more pieces than our largest Tablebase has pieces
    int probeLimit = threadPool.get_syzygyProbeLimit();
    int pieceCount = BB::count_bit(occupancy_all());

    if (   get_status().castling
         || pieceCount > TB_LARGEST
         || (probeLimit > 0 && pieceCount > probeLimit))
         return false;

    // Appel Pyrrhic avec un tableau de résultats pour obtenir TOUS les coups et leur WDL/DTZ
    unsigned results[TB_MAX_MOVES + 1];
    unsigned best = tb_probe_root(
        occupancy_c<WHITE>(),  occupancy_c<BLACK>(),
        occupancy_p<PieceType::KING>(),   occupancy_p<PieceType::QUEEN>(),
        occupancy_p<PieceType::ROOK>(),   occupancy_p<PieceType::BISHOP>(),
        occupancy_p<PieceType::KNIGHT>(), occupancy_p<PieceType::PAWN>(),
        get_status().fiftymove_counter,
        get_status().ep_square == SQUARE_NONE ? 0 : get_status().ep_square,
        turn() == WHITE ? 1 : 0,
        results);

    // Probe failed, or we are already in a finished position.
    if (   best == TB_RESULT_FAILED
        || best == TB_RESULT_CHECKMATE
        || best == TB_RESULT_STALEMATE)
        return false;

    unsigned wdl = TB_GET_WDL(best);

    if (wdl == TB_WIN)
    {
        // Collecter TOUS les coups gagnants pour que la recherche trouve le gain le plus rapide.
        // L'alpha-bêta les score en TBWIN-ply, trouvant naturellement le coup DTM-optimal.
        for (const unsigned* r = results; *r != TB_RESULT_FAILED; r++)
        {
            if (TB_GET_WDL(*r) == TB_WIN)
                root_moves.add(convertPyrrhicMove(*r));
        }
        // Retourner false : l'appelant doit lancer la recherche normale limitée à win_moves.
        return false;
    }

    // NUL ou PERTE : jouer le coup DTZ-optimal immédiatement
    move = convertPyrrhicMove(best);
    unsigned dtz = TB_GET_DTZ(best);
    int score = (wdl == TB_LOSS) ? -TBWIN + (int)dtz : 0;

    std::cout << "info depth " << dtz
              << " score cp "  << score
              << " time 0 nodes 0 nps 0 tbhits 1 pv " << Move::name(move)
              << std::endl;
    return true;
}

MOVE Board::convertPyrrhicMove(unsigned result) const
{
    // Extract Pyrhic's move representation
    unsigned to    = TB_GET_TO(result);
    unsigned from  = TB_GET_FROM(result);
    unsigned ep    = TB_GET_EP(result);
    unsigned promo = TB_GET_PROMOTES(result);   // 1 = Dame ; 4 = Cavalier      syzygy
                                                // 5          2                 moi

    //TODO à vérifier

    Piece piece = piece_square[from];
    Color color = Move::color(piece);

    // Convert the move notation. Care that Pyrrhic's promotion flags are inverted
    if (ep == 0u && promo == 0u)
        return Move::CODE(from, to, piece_square[from], piece_square[to], Piece::PIECE_NONE, Move::FLAG_NONE); // MoveMake(from, to, NORMAL_MOVE);
    else if (ep != 0u)
        return Move::CODE(from, get_status().ep_square, piece, Move::make_piece(~color, PieceType::PAWN), Piece::PIECE_NONE, Move::FLAG_ENPASSANT_MASK);
    else /* if (promo != 0u) */
    {
        PieceType p = static_cast<PieceType>(6-promo);
        return Move::CODE(from, to, piece, piece_square[to], Move::make_piece(color, p), Move::FLAG_NONE);
    }
}

