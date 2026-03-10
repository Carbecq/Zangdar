
// Fichier inspiré de Weiss et Ethereal

#include <algorithm>
#include <iomanip>
#include <vector>
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
        return false;

    TBScore(result, ply, score, bound);

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
        // Collecter TOUS les coups gagnants, avec leur DTZ pour le tri.
        // On trie par DTZ croissant : les coups qui zeroing le plus tôt en premier,
        // ce qui aide alpha-bêta à établir rapidement un alpha élevé (TBWIN-ply).
        for (const unsigned* r = results; *r != TB_RESULT_FAILED; r++)
        {
            if (TB_GET_WDL(*r) == TB_WIN)
            {
                root_moves.mlmoves[root_moves.count].move  = convertPyrrhicMove(*r);
                root_moves.mlmoves[root_moves.count].value = (int)TB_GET_DTZ(*r);
                root_moves.count++;
            }
        }
        std::sort(root_moves.mlmoves.begin(),
                  root_moves.mlmoves.begin() + root_moves.count,
                  [](const MLMove& a, const MLMove& b) { return a.value < b.value; });
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

//=========================================================================
//! \brief Fonction de test : sonde les tables Syzygy pour une position
//!        donnée et affiche le résultat. En cas de victoire (WIN), liste
//!        tous les coups gagnants avec leurs caractéristiques
//!        (nom, prise, prise en passant, promotion).
//!        Affiche également la position sur l'échiquier.
//-------------------------------------------------------------------------
void Board::probe_root_test() const
{
    if (threadPool.get_useSyzygy() == false)
    {
        std::cout << "Les tables Syzygy ne sont pas chargées." << std::endl;
        return;
    }

    std::cout << display() << std::endl;
    std::cout << "FEN : " << get_fen() << std::endl;

    int pieceCount = BB::count_bit(occupancy_all());
    std::cout << "Pièces : " << pieceCount
              << "  TB_LARGEST : " << (int)TB_LARGEST << std::endl;

    // Vérification des conditions préalables au sondage
    if (get_status().castling)
    {
        std::cout << "Position hors TB : droits de roque présents." << std::endl;
        return;
    }
    int probeLimit = threadPool.get_syzygyProbeLimit();
    if (   pieceCount > TB_LARGEST
        || (probeLimit > 0 && pieceCount > probeLimit))
    {
        std::cout << "Position hors TB : trop de pièces." << std::endl;
        return;
    }

    // Sondage Pyrrhic : récupère tous les coups avec leur WDL/DTZ
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

    if (best == TB_RESULT_FAILED)
    {
        std::cout << "Sondage TB échoué (position absente des tables)." << std::endl;
        return;
    }
    if (best == TB_RESULT_CHECKMATE)
    {
        std::cout << "Résultat TB : MAT." << std::endl;
        return;
    }
    if (best == TB_RESULT_STALEMATE)
    {
        std::cout << "Résultat TB : PAT." << std::endl;
        return;
    }

    unsigned wdl = TB_GET_WDL(best);
    unsigned dtz = TB_GET_DTZ(best);

    const char* wdl_str = (wdl == TB_WIN)         ? "VICTOIRE"
                        : (wdl == TB_LOSS)         ? "DÉFAITE"
                        : (wdl == TB_BLESSED_LOSS) ? "DÉFAITE_BÉNIE (nulle)"
                        : (wdl == TB_CURSED_WIN)   ? "VICTOIRE_MAUDITE (nulle)"
                        :                            "NULLE";

    std::cout << "Résultat TB : " << wdl_str
              << "  DTZ : " << dtz
              << "  Meilleur coup : " << Move::name(convertPyrrhicMove(best))
              << std::endl;

    // Collecte tous les coups (résultats Pyrrhic), triés par WDL décroissant
    // (meilleur en premier), puis par DTZ croissant (même logique que probe_root)
    struct TBMove { MOVE mv; unsigned wdl_val; unsigned dtz_val; };
    std::vector<TBMove> all_moves;
    for (const unsigned* r = results; *r != TB_RESULT_FAILED; r++)
        all_moves.push_back({ convertPyrrhicMove(*r), TB_GET_WDL(*r), TB_GET_DTZ(*r) });

    std::sort(all_moves.begin(), all_moves.end(),
              [](const TBMove& a, const TBMove& b) {
                  if (a.wdl_val != b.wdl_val) return a.wdl_val > b.wdl_val;
                  return a.dtz_val < b.dtz_val;
              });

    auto wdl_label = [](unsigned w) -> const char* {
        switch (w) {
            case TB_WIN:          return "VICTOIRE";
            case TB_CURSED_WIN:   return "MAUDITE";
            case TB_DRAW:         return "NULLE";
            case TB_BLESSED_LOSS: return "BÉNIE";
            case TB_LOSS:         return "DÉFAITE";
            default:              return "?";
        }
    };

    std::cout << "Tous les coups (triés par WDL puis DTZ) :" << std::endl;
    for (const auto& e : all_moves)
    {
        bool is_capt  = Move::is_capturing(e.mv);
        bool is_ep    = Move::is_enpassant(e.mv);
        bool is_promo = Move::is_promoting(e.mv);

        std::string type;
        if      (is_ep)    type = "prise en passant";
        else if (is_capt && is_promo) type = std::string("prise+promotion(") + pieceTypeToChar(Move::promoted_type(e.mv)) + ")";
        else if (is_capt)  type = std::string("prise(") + pieceTypeToChar(Move::captured_type(e.mv)) + ")";
        else if (is_promo) type = std::string("promotion(") + pieceTypeToChar(Move::promoted_type(e.mv)) + ")";
        else               type = "tranquille";

        std::cout << "  "
                  << std::left  << std::setw(7) << Move::name(e.mv)
                  << std::left  << std::setw(10) << wdl_label(e.wdl_val)
                  << "DTZ=" << std::right << std::setw(3) << e.dtz_val
                  << "  " << type
                  << std::endl;
    }
}

MOVE Board::convertPyrrhicMove(unsigned result) const
{
    // Extraction de la représentation de coup de Pyrrhic
    unsigned to    = TB_GET_TO(result);
    unsigned from  = TB_GET_FROM(result);
    unsigned ep    = TB_GET_EP(result);
    unsigned promo = TB_GET_PROMOTES(result);   // 1 = Dame ; 4 = Cavalier      syzygy
                                                // 5          2                 moi

    //TODO à vérifier

    Piece piece = piece_square[from];
    Color color = Move::color(piece);

    // Conversion de la notation de coup. Attention : les flags de promotion de Pyrrhic sont inversés
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

