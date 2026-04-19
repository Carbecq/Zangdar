#include "NNUE.h"
#include <cassert>
#include "types.h"
#include "bitmask.h"
#include "simd.h"
#include "Move.h"
#include "Board.h"

namespace {

#include "incbin/incbin.h"
INCBIN(networkData, EVALFILE);
const Network *network = reinterpret_cast<const Network *>(gnetworkDataData);

}


//======================================================
//! \brief  Retourne l'évaluation du réseau
//------------------------------------------------------
template<Color color>
int NNUE::evaluate(const Accumulator& current, size_t count)
{
    auto output = 0;
    const int bucket = get_bucket(count);

    if constexpr (color == Color::WHITE)
        output = activation(current.white, current.black, network->output_weights, bucket);
    else
        output = activation(current.black, current.white, network->output_weights, bucket);

    return output;
}

//====================================================
//! \brief  Initialise NNUE pour une nouvelle recherche
//----------------------------------------------------
void NNUE::start_search(const Board& board)
{
    head_idx = 0;

    for (int i = 0; i < 2; i++)
        for (int j = 0; j < KING_BUCKETS_COUNT; j++)
            finny[i][j].init();

    Accumulator& head = stack[0];

    init_accumulator(head);

    const SQUARE wking = board.king_square[WHITE];
    const SQUARE bking = board.king_square[BLACK];
    SQUARE square;

    Bitboard occupied = board.occupancy_all();
    while (occupied)
    {
        square = BB::pop_lsb(occupied);
        add(head, board.piece_square[square], square, wking, bking);
    }

    head.updated[WHITE] = true;
    head.updated[BLACK] = true;

    head.king_square = board.king_square;
}

//====================================================
//! \brief  Rebase le stack NNUE : copie l'accumulateur courant
//!         en position 0 et remet head_idx à 0.
//!         Réinitialise les finny tables (elles référencent
//!         des positions de stack qui ne sont plus valides).
//!         Utilisé dans DataGen pour éviter l'overflow du stack
//!         quand start_search() n'est appelé qu'une fois par partie.
//!
//!  C'est utile. Voici pourquoi.
//!
//! Le problème sans rebase
//!
//! Dans DataGen, start_search() n'est appelé qu'une seule fois par partie
//!  (pour éviter de recalculer l'accumulateur de zéro à chaque coup).
//! La boucle de jeu enchaîne les make_move, et chaque coup fait un push() → head_idx monte de 1 à chaque demi-coup.
//! Sur une longue partie (50-100 coups), head_idx atteindrait la taille du stack (MAX_PLY+1) → overflow
//! et comportement indéfini.
//!
//! Ce que fait rebase
//!
//!     1. Applique les lazy updates en attente sur l'accumulateur courant (lazy_updates)
//!     2. Copie cet accumulateur en stack[0] et remet head_idx = 0
//!     3. Réinitialise les Finny tables — elles contiennent des références aux positions du stack
//!         (buckets de roi, bitboards), qui deviennent obsolètes après le décalage
//!
//! Le résultat : le stack repart de zéro avec l'état correct, sans recalculer l'accumulateur
//!   depuis la position initiale.
//!
//! Donc oui, c'est nécessaire dans le contexte DataGen où start_search() n'est pas rappelé à chaque coup.
//!   Sans ça, overflow garanti sur les parties longues.
//!
//----------------------------------------------------
void NNUE::rebase(const Board& board)
{
    if (head_idx > 0)
    {
        // Assurer que l'accumulateur courant est à jour
        Accumulator& current = stack[head_idx];
        lazy_updates(board, current);

        stack[0] = current;
        head_idx = 0;
    }

    // Réinitialiser les finny tables car elles peuvent contenir
    // des bitboards obsolètes après le rebase
    for (int i = 0; i < 2; i++)
        for (int j = 0; j < KING_BUCKETS_COUNT; j++)
            finny[i][j].init();
}

//====================================================
//! \brief  Ajoute 1 nouvel accumulateur
//----------------------------------------------------
void NNUE::push()
{
    assert(head_idx < stack.size()-1);  // < 128
    head_idx++;
}

//====================================================
//! \brief  Supprime 1 accumulateur
//----------------------------------------------------
void NNUE::pop()
{
    assert(head_idx > 0);
    head_idx--;
}

//====================================================
//! \brief  Re-initalise toutes les perspectives
//----------------------------------------------------
void NNUE::init_accumulator(Accumulator& acc)
{
    acc.init_biases(network->feature_biases);
}

//====================================================
//! \brief  Re-initalise les tables Finny
//----------------------------------------------------
void FinnyEntry::init()
{
    typePiecesBB.fill({});
    colorPiecesBB.fill({});
    accumulator.init_biases(network->feature_biases);
}

//========================================================================
//! \brief  Ajout d'une feature
//------------------------------------------------------------------------
void NNUE::add(Accumulator& accu, Piece piece, SQUARE from, SQUARE wking, SQUARE bking)
{
    const auto [white_idx, black_idx] = get_indices(piece, from, wking, bking);

    // Non-SIMD volontairement : appelé une seule fois par pièce au début
    // de la recherche (start_search), pas dans la boucle critique.
    for (size_t i = 0; i < HIDDEN_LAYER_SIZE; ++i)
        accu.white[i] += network->feature_weights[white_idx * HIDDEN_LAYER_SIZE + i];

    for (size_t i = 0; i < HIDDEN_LAYER_SIZE; ++i)
        accu.black[i] += network->feature_weights[black_idx * HIDDEN_LAYER_SIZE + i];
}


//=========================================
//! \brief  Évaluation SCReLU + produit matrice-vecteur (activations × output_weights) → score en centipions
//!
//! Algorithme "Lizard" (SCReLU dual perspective) :
//!   1. SCReLU SIMD : clipped = clamp(acc, 0, QA)
//!      product = clipped × weight  (I16, safe : 255×127 = 32385 < 32767)
//!      result  = madd(product, clipped) → I32  (= clipped² × weight)
//!      → sum accumule clipped² × weight sur toutes les paires
//!
//!   2. Mise à l'échelle :
//!      sum est à l'échelle QA² × QB
//!      eval = sum / QA + bias   (bias à QAB = QA×QB → cohérent après /QA)
//!      eval = eval × SCALE / QAB
//!
//! Ref : https://cosmo.tardis.ac/files/2024-06-01-nnue.html
//-----------------------------------------
#if defined USE_SIMD

constexpr int kChunkSize = sizeof(simd::Vepi16) / sizeof(I16);

I32 NNUE::activation(const std::array<I16, HIDDEN_LAYER_SIZE>& us,
                     const std::array<I16, HIDDEN_LAYER_SIZE>& them,
                     const std::array<I16, HIDDEN_LAYER_SIZE * N_COLORS * OUTPUT_BUCKETS>& weights,
                     const int bucket)
{
    const int adresse = bucket * N_COLORS * HIDDEN_LAYER_SIZE;

    auto sum = simd::ZeroEpi32();

    // Perspective du joueur actif (us)
    for (size_t i = 0; i < HIDDEN_LAYER_SIZE; i += kChunkSize)
    {
        const auto input   = simd::LoadEpi16(&us[i]);
        const auto weight  = simd::LoadEpi16(&weights[adresse + i]);

        // Écrêtage : clamp(acc, 0, QA) → valeur entre 0 et 255
        const auto clipped = simd::Clip(input, QA);

        // 1ère multiplication : clipped × weight → I16
        // Pas d'overflow : 255 × 127 = 32385 < 32767 (max I16)
        const auto product = simd::MultiplyEpi16(clipped, weight);

        // 2ème multiplication avec élargissement vers I32 (madd) :
        // result[i] = product[2i] × clipped[2i] + product[2i+1] × clipped[2i+1]
        //           = clipped² × weight  (SCReLU = activation au carré)
        const auto result  = simd::MultiplyAddEpi16(product, clipped);

        // Accumulation en I32
        sum = simd::AddEpi32(sum, result);
    }

    // Perspective de l'adversaire (them)
    for (size_t i = 0; i < HIDDEN_LAYER_SIZE; i += kChunkSize)
    {
        const auto input   = simd::LoadEpi16(&them[i]);
        const auto weight  = simd::LoadEpi16(&weights[adresse + HIDDEN_LAYER_SIZE + i]);
        const auto clipped = simd::Clip(input, QA);
        const auto product = simd::MultiplyEpi16(clipped, weight);
        const auto result  = simd::MultiplyAddEpi16(product, clipped);
        sum = simd::AddEpi32(sum, result);
    }

    // Réduction horizontale : somme tous les I32 du vecteur en un seul scalaire
    I32 eval = simd::ReduceAddEpi32(sum);

    // Dé-quantification : sum est à l'échelle QA²×QB → on divise par QA → échelle QA×QB
    eval /= QA;

    // Ajout du biais de sortie (stocké à l'échelle QA×QB = QAB)
    eval += network->output_bias[bucket];

    // Mise à l'échelle en centipions, puis dé-quantification finale
    eval *= SCALE;
    eval /= QAB;

    return eval;
}

#else

// Fallback scalaire (sans SIMD) — même algorithme SCReLU Lizard
I32 NNUE::activation(const std::array<I16, HIDDEN_LAYER_SIZE>& us,
                     const std::array<I16, HIDDEN_LAYER_SIZE>& them,
                     const std::array<I16, HIDDEN_LAYER_SIZE * N_COLORS * OUTPUT_BUCKETS>& weights,
                     const int bucket)
{
    I32 eval = 0;
    const int adresse = bucket * N_COLORS * HIDDEN_LAYER_SIZE;

    for (Usize i = 0; i < HIDDEN_LAYER_SIZE; ++i)
    {
        eval += screlu(us[i])   * weights[adresse + i];
        eval += screlu(them[i]) * weights[adresse + HIDDEN_LAYER_SIZE + i];
    }

    eval /= QA;
    eval += network->output_bias[bucket];
    eval *= SCALE;
    eval /= QAB;

    return eval;
}

#endif

//============================================================
//! \brief  Utilisation de l'optimisation Lazy Updates
//! https://www.chessprogramming.org/NNUE#Lazy_Updates
//------------------------------------------------------------
void NNUE::lazy_updates(const Board& board, Accumulator& acc)
{
    lazy_update<WHITE>(board, acc);
    lazy_update<BLACK>(board, acc);
}

template <Color side>
void NNUE::lazy_update(const Board &board, Accumulator& acc)
{
    if (acc.updated[side])
        return;

    assert(head_idx > 0);

    const SQUARE king = acc.king_square[side];

    for (int iter = int(head_idx) - 1; iter >= 0; --iter)
    {
        // Recherche d'un accumulateur à raffraichir
        if (NNUE::need_refresh<side>(stack[iter].king_square[side], king) == true)
        {
            // Comme le cout d'un raffraichissement est important
            // autant le faire sur le dernier accumulateur : acc
            refresh_accumulator<side>(board, acc);
            break;
        }

        // On a trouvé le dernier accumulateur updaté
        if (stack[iter].updated[side] == true)
        {
            size_t last_updated = iter;

            // on va remonter les accumulateurs;
            // et les updater un par un
            while (last_updated != head_idx)
            {
                update<side>(stack[last_updated], stack[last_updated+1], king);
                last_updated++;
            }
            break;
        }
    }
}

//======================================================
//! \brief  Faut-il raffraichir l'accumulateur ?
//------------------------------------------------------
template <Color side>
bool NNUE::need_refresh(SQUARE old_king, SQUARE new_king)
{
    assert(SQ::is_ok(old_king));
    assert(SQ::is_ok(new_king));

    // 1) Horizontal Mirroring
    //      est-ce qu'on a changé de côté ?
    if ((old_king & 0b100) != (new_king & 0b100))
        return true;

    // 2) King Bucket
    //      est-ce qu'on a change de bucket ?
    return king_buckets_map[get_relative_square<side>(old_king)] != king_buckets_map[get_relative_square<side>(new_king)];
}

//===================================================================
//  Prise en compte des modifications apportées à la perspective
//--------------------------------------------------------------------
template <Color side>
void NNUE::update(const Accumulator& src, Accumulator& dst, SQUARE king)
{
    const DirtyPieces& dp = dst.dirtyPieces;

    if (dp.type == DirtyPieces::CASTLING)
    {
        sub_sub_add_add<side>(src, dst,
                              dp.sub_1.piece, dp.sub_1.square,
                              dp.sub_2.piece, dp.sub_2.square,
                              dp.add_1.piece, dp.add_1.square,
                              dp.add_2.piece, dp.add_2.square,
                              king);
    }
    else if (dp.type == DirtyPieces::CAPTURE)
    {
        sub_sub_add<side>(src, dst,
                          dp.sub_1.piece, dp.sub_1.square,
                          dp.sub_2.piece, dp.sub_2.square,
                          dp.add_1.piece, dp.add_1.square,
                          king);
    }
    else
    {
        sub_add<side>(src, dst,
                      dp.sub_1.piece, dp.sub_1.square,
                      dp.add_1.piece, dp.add_1.square,
                      king);
    }

    dst.updated[side] = true;
}

//========================================================================
//! \brief  Calcule l'indice du triplet (couleur, piece, case)
//! dans l'Input Layer
//------------------------------------------------------------------------
std::pair<size_t, size_t> NNUE::get_indices(Piece piece, SQUARE square, SQUARE wking, SQUARE bking)
{
    // L'input Layer est rangée ainsi :
    //  + Les blancs
    //    + les pièces blanches
    //      + Les cases de chaque pièce blanche
    //  + les noirs
    //    + les pièces noires
    //      + Les cases de chaque pièce noire

    //  Lorsque une entrée est modifiée par un coup, il faut retrouver
    //  son indice.

    assert(piece  != Piece::PIECE_NONE);
    assert(square != Square::SQUARE_NONE);

    constexpr U32 color_stride = N_SQUARES * 6;
    constexpr U32 piece_stride = N_SQUARES;

    const auto base  = Move::type(piece);
    const auto color = Move::color(piece);

    //TODO : refaire le codage des pièces ?
    //  moi  : PAWN = 1
    //  NNUE : PAWN = 0
    const auto base_nnue = base - 1;

    const auto white_indice = king_buckets_map[wking] * INPUT_LAYER_SIZE
            + color * color_stride
            + base_nnue * piece_stride
            + get_square(square, wking);


    const auto black_indice = king_buckets_map[SQ::mirrorVertically(bking)] * INPUT_LAYER_SIZE
            + !color * color_stride
            + base_nnue * piece_stride
            + SQ::mirrorVertically(get_square(square, bking));

    return {white_indice, black_indice};
}

//========================================================================
//! \brief  Calcule l'indice du triplet (couleur, piece, case)
//!         pour une perspective
//! \param[in] side     perspective
//------------------------------------------------------------------------
template <Color side>
U32 NNUE::get_indice(Piece piece, SQUARE square, SQUARE king)
{
    assert(piece  != Piece::PIECE_NONE);
    assert(square != Square::SQUARE_NONE);

    constexpr U32 color_stride = N_SQUARES * 6;
    constexpr U32 piece_stride = N_SQUARES;

    const auto base  = Move::type(piece);
    const auto color = Move::color(piece);

    const auto base_nnue = base - 1;

    if (side == WHITE)
    {
        return king_buckets_map[king] * INPUT_LAYER_SIZE
                + color * color_stride
                + base_nnue * piece_stride
                + get_square(square, king);
    }
    else
    {
        return king_buckets_map[SQ::mirrorVertically(king)] * INPUT_LAYER_SIZE
                + !color * color_stride
                + base_nnue * piece_stride
                + SQ::mirrorVertically(get_square(square, king));
    }
}

//================================================================
//! \brief  Ajoute une pièce
//! \param[in] piece    pièce à ajouter
//! \param[in] color    couleur de cette pièce
//! \param[in] from     position de cette pièce
//! \param[in] king     position du roi de couleur "US"
//----------------------------------------------------------------
template <Color side>
void NNUE::add(Accumulator& accu, Piece piece, SQUARE from, SQUARE king)
{
    const auto idx = get_indice<side>(piece, from, king);

#if defined USE_SIMD
    constexpr int simd_width = sizeof(simd::Vepi16) / sizeof(I16);

    for (size_t i = 0; i < HIDDEN_LAYER_SIZE; i += simd_width)
    {
        if constexpr (side == WHITE)
        {
            simd::StoreEpi16(&accu.white[i], simd::AddEpi16(simd::LoadEpi16(&accu.white[i]), simd::LoadEpi16(&network->feature_weights[idx * HIDDEN_LAYER_SIZE + i])));
        }
        else
        {
            simd::StoreEpi16(&accu.black[i], simd::AddEpi16(simd::LoadEpi16(&accu.black[i]), simd::LoadEpi16(&network->feature_weights[idx * HIDDEN_LAYER_SIZE + i])));
        }
    }
#else

    if constexpr (side == WHITE)
    {
        for (Usize i = 0; i < HIDDEN_LAYER_SIZE; ++i)
            accu.white[i] += network->feature_weights[idx * HIDDEN_LAYER_SIZE + i];
    }
    else
    {
        for (Usize i = 0; i < HIDDEN_LAYER_SIZE; ++i)
            accu.black[i] += network->feature_weights[idx * HIDDEN_LAYER_SIZE + i];
    }
#endif
}

template <Color side>
void NNUE::sub(Accumulator& accu, Piece piece, SQUARE from, SQUARE king)
{
    const auto idx = get_indice<side>(piece, from, king);

#if defined USE_SIMD
    constexpr int simd_width = sizeof(simd::Vepi16) / sizeof(I16);

    for (size_t i = 0; i < HIDDEN_LAYER_SIZE; i += simd_width)
    {
        if constexpr (side == WHITE)
        {
            simd::StoreEpi16(&accu.white[i], simd::SubEpi16(simd::LoadEpi16(&accu.white[i]), simd::LoadEpi16(&network->feature_weights[idx * HIDDEN_LAYER_SIZE + i])));
        }
        else
        {
            simd::StoreEpi16(&accu.black[i], simd::SubEpi16(simd::LoadEpi16(&accu.black[i]), simd::LoadEpi16(&network->feature_weights[idx * HIDDEN_LAYER_SIZE + i])));
        }
    }
#else
    if constexpr (side == WHITE)
    {
        for (Usize i = 0; i < HIDDEN_LAYER_SIZE; ++i)
            accu.white[i] -= network->feature_weights[idx * HIDDEN_LAYER_SIZE + i];
    }
    else
    {
        for (Usize i = 0; i < HIDDEN_LAYER_SIZE; ++i)
            accu.black[i] -= network->feature_weights[idx * HIDDEN_LAYER_SIZE + i];
    }
#endif
}

//=================================================================================
//! \brief  Update combiné : Ajout + Suppression
//! \param[in]  sub_piece       pièce supprimée de from
//! \param[in]  add_piece       pièce ajoutée en dest
//---------------------------------------------------------------------------------
template <Color side>
void NNUE::sub_add(const Accumulator& src, Accumulator& dst,
                   Piece sub_piece, SQUARE sub,
                   Piece add_piece, SQUARE add,
                   SQUARE king)
{
    // Optimisations de : https://cosmo.tardis.ac/files/2024-06-01-nnue.html

    const auto sub_idx = get_indice<side>(sub_piece, sub, king);
    const auto add_idx = get_indice<side>(add_piece, add, king);

#if defined USE_SIMD
    constexpr int simd_width = sizeof(simd::Vepi16) / sizeof(I16);

    for (size_t i = 0; i < HIDDEN_LAYER_SIZE; i += simd_width)
    {
        if constexpr (side == WHITE)
        {
            auto cur_w  = simd::LoadEpi16(&src.white[i]);
            //TODO sub_add ou add_sub ??
            cur_w       = simd::SubEpi16(cur_w, simd::LoadEpi16(&network->feature_weights[sub_idx * HIDDEN_LAYER_SIZE + i]));
            cur_w       = simd::AddEpi16(cur_w, simd::LoadEpi16(&network->feature_weights[add_idx * HIDDEN_LAYER_SIZE + i]));
            simd::StoreEpi16(&dst.white[i], cur_w);
        }
        else
        {
            auto cur_w  = simd::LoadEpi16(&src.black[i]);
            //TODO sub_add ou add_sub ??
            cur_w       = simd::SubEpi16(cur_w, simd::LoadEpi16(&network->feature_weights[sub_idx * HIDDEN_LAYER_SIZE + i]));
            cur_w       = simd::AddEpi16(cur_w, simd::LoadEpi16(&network->feature_weights[add_idx * HIDDEN_LAYER_SIZE + i]));
            simd::StoreEpi16(&dst.black[i], cur_w);
        }
    }
#else
    if constexpr (side == WHITE)
    {
        for (Usize i = 0; i < HIDDEN_LAYER_SIZE; ++i)
        {
            dst.white[i] = src.white[i]
                    + network->feature_weights[add_idx * HIDDEN_LAYER_SIZE + i]
                    - network->feature_weights[sub_idx * HIDDEN_LAYER_SIZE + i];
        }
    }
    else
    {
        for (Usize i = 0; i < HIDDEN_LAYER_SIZE; ++i)
        {
            dst.black[i] = src.black[i]
                    + network->feature_weights[add_idx * HIDDEN_LAYER_SIZE + i]
                    - network->feature_weights[sub_idx * HIDDEN_LAYER_SIZE + i];
        }
    }
#endif
}


//=================================================================================
//! \brief  Update combiné : Ajout + Suppression + Suppression
//! La pièce supprimée en dest appartient au camp ennemi
//! Cette routine est utilisée pour la prise en passant
//! \param[in]  sub_piece_1     pièce supprimée de from
//! \param[in]  sub_piece_2     pièce ennemie supprimée de sub
//! \param[in]  add_piece       pièce ajoutée en dest
//!
//---------------------------------------------------------------------------------
template <Color side>
void NNUE::sub_sub_add(const Accumulator& src, Accumulator& dst,
                       Piece sub_piece_1, SQUARE sub_1,
                       Piece sub_piece_2, SQUARE sub_2,
                       Piece add_piece_1, SQUARE add_1,
                       SQUARE king)
{
    const auto sub1_idx = get_indice<side>(sub_piece_1, sub_1, king);
    const auto sub2_idx = get_indice<side>(sub_piece_2, sub_2, king);
    const auto add1_idx = get_indice<side>(add_piece_1, add_1, king);

#if defined USE_SIMD
    constexpr int simd_width = sizeof(simd::Vepi16) / sizeof(I16);

    if constexpr (side == WHITE)
    {
        for (size_t i = 0; i < HIDDEN_LAYER_SIZE; i += simd_width)
        {
            auto cur_w = simd::LoadEpi16(&src.white[i]);
            cur_w      = simd::AddEpi16(cur_w, simd::LoadEpi16(&network->feature_weights[add1_idx * HIDDEN_LAYER_SIZE + i]));
            cur_w      = simd::SubEpi16(cur_w, simd::LoadEpi16(&network->feature_weights[sub1_idx * HIDDEN_LAYER_SIZE + i]));
            cur_w      = simd::SubEpi16(cur_w, simd::LoadEpi16(&network->feature_weights[sub2_idx * HIDDEN_LAYER_SIZE + i]));
            simd::StoreEpi16(&dst.white[i], cur_w);
        }
    }
    else
    {
        for (size_t i = 0; i < HIDDEN_LAYER_SIZE; i += simd_width)
        {
            auto cur_w = simd::LoadEpi16(&src.black[i]);
            cur_w      = simd::AddEpi16(cur_w, simd::LoadEpi16(&network->feature_weights[add1_idx * HIDDEN_LAYER_SIZE + i]));
            cur_w      = simd::SubEpi16(cur_w, simd::LoadEpi16(&network->feature_weights[sub1_idx * HIDDEN_LAYER_SIZE + i]));
            cur_w      = simd::SubEpi16(cur_w, simd::LoadEpi16(&network->feature_weights[sub2_idx * HIDDEN_LAYER_SIZE + i]));
            simd::StoreEpi16(&dst.black[i], cur_w);
        }
    }
#else
    if constexpr (side == WHITE)
    {
        for (Usize i = 0; i < HIDDEN_LAYER_SIZE; ++i)
        {
            dst.white[i] = src.white[i]
                    + network->feature_weights[add1_idx * HIDDEN_LAYER_SIZE + i]
                    - network->feature_weights[sub1_idx * HIDDEN_LAYER_SIZE + i]
                    - network->feature_weights[sub2_idx * HIDDEN_LAYER_SIZE + i];
        }
    }
    else
    {
        for (Usize i = 0; i < HIDDEN_LAYER_SIZE; ++i)
        {
            dst.black[i] = src.black[i]
                    + network->feature_weights[add1_idx * HIDDEN_LAYER_SIZE + i]
                    - network->feature_weights[sub1_idx * HIDDEN_LAYER_SIZE + i]
                    - network->feature_weights[sub2_idx * HIDDEN_LAYER_SIZE + i];
        }
    }
#endif
}

template <Color side>
void NNUE::sub_sub_add_add(const Accumulator& src, Accumulator& dst,
                           Piece sub_piece_1, SQUARE sub_1,
                           Piece sub_piece_2, SQUARE sub_2,
                           Piece add_piece_1, SQUARE add_1,
                           Piece add_piece_2, SQUARE add_2,
                           SQUARE king)
{
    const auto sub1_idx = get_indice<side>(sub_piece_1, sub_1, king);
    const auto sub2_idx = get_indice<side>(sub_piece_2, sub_2, king);
    const auto add1_idx = get_indice<side>(add_piece_1, add_1, king);
    const auto add2_idx = get_indice<side>(add_piece_2, add_2, king);

#if defined USE_SIMD
    constexpr int simd_width = sizeof(simd::Vepi16) / sizeof(I16);

    if constexpr (side == WHITE)
    {
        for (size_t i = 0; i < HIDDEN_LAYER_SIZE; i += simd_width)
        {
            auto cur_w = simd::LoadEpi16(&src.white[i]);
            cur_w      = simd::AddEpi16(cur_w, simd::LoadEpi16(&network->feature_weights[add1_idx * HIDDEN_LAYER_SIZE + i]));
            cur_w      = simd::AddEpi16(cur_w, simd::LoadEpi16(&network->feature_weights[add2_idx * HIDDEN_LAYER_SIZE + i]));
            cur_w      = simd::SubEpi16(cur_w, simd::LoadEpi16(&network->feature_weights[sub1_idx * HIDDEN_LAYER_SIZE + i]));
            cur_w      = simd::SubEpi16(cur_w, simd::LoadEpi16(&network->feature_weights[sub2_idx * HIDDEN_LAYER_SIZE + i]));
            simd::StoreEpi16(&dst.white[i], cur_w);
        }
    }
    else
    {
        for (size_t i = 0; i < HIDDEN_LAYER_SIZE; i += simd_width)
        {
            auto cur_w = simd::LoadEpi16(&src.black[i]);
            cur_w      = simd::AddEpi16(cur_w, simd::LoadEpi16(&network->feature_weights[add1_idx * HIDDEN_LAYER_SIZE + i]));
            cur_w      = simd::AddEpi16(cur_w, simd::LoadEpi16(&network->feature_weights[add2_idx * HIDDEN_LAYER_SIZE + i]));
            cur_w      = simd::SubEpi16(cur_w, simd::LoadEpi16(&network->feature_weights[sub1_idx * HIDDEN_LAYER_SIZE + i]));
            cur_w      = simd::SubEpi16(cur_w, simd::LoadEpi16(&network->feature_weights[sub2_idx * HIDDEN_LAYER_SIZE + i]));
            simd::StoreEpi16(&dst.black[i], cur_w);
        }
    }
#else
    if constexpr (side == WHITE)
    {
        for (Usize i = 0; i < HIDDEN_LAYER_SIZE; ++i)
        {
            dst.white[i] = src.white[i]
                    + network->feature_weights[add1_idx * HIDDEN_LAYER_SIZE + i]
                    + network->feature_weights[add2_idx * HIDDEN_LAYER_SIZE + i]
                    - network->feature_weights[sub1_idx * HIDDEN_LAYER_SIZE + i]
                    - network->feature_weights[sub2_idx * HIDDEN_LAYER_SIZE + i];
        }
    }
    else
    {
        for (Usize i = 0; i < HIDDEN_LAYER_SIZE; ++i)
        {
            dst.black[i] = src.black[i]
                    + network->feature_weights[add1_idx * HIDDEN_LAYER_SIZE + i]
                    + network->feature_weights[add2_idx * HIDDEN_LAYER_SIZE + i]
                    - network->feature_weights[sub1_idx * HIDDEN_LAYER_SIZE + i]
                    - network->feature_weights[sub2_idx * HIDDEN_LAYER_SIZE + i];
        }

    }
#endif
}

//==========================================================================
//! \brief  Raffaichissement de l'accumulateur
//! De façon à minimiser le travail, on utilise les finny tables
//--------------------------------------------------------------------------
template <Color side>
void NNUE::refresh_accumulator(const Board &board, Accumulator& acc)
{
    const SQUARE king_square = board.get_king_square<side>();
    const int    king_bucket = king_buckets_map[get_relative_square<side>(king_square)];
    const int    mirrored    = SQ::file(king_square) > FILE_D;

    FinnyEntry& entry = finny[mirrored][king_bucket];

    SQUARE square;

    for (Color color : {Color::WHITE, Color::BLACK})
    {
        for (PieceType piece : {PieceType::PAWN, PieceType::KNIGHT, PieceType::BISHOP, PieceType::ROOK,
             PieceType::QUEEN, PieceType::KING })
        {
            const Bitboard old_pieces = entry.colorPiecesBB[side][color] & entry.typePiecesBB[side][piece];
            const Bitboard new_pieces = board.colorPiecesBB[color]       & board.typePiecesBB[piece];

            // rechreche des pièces à supprimer
            Bitboard to_remove = ~new_pieces & old_pieces;
            while (to_remove)
            {
                square = BB::pop_lsb(to_remove);
                sub<side>(entry.accumulator, Move::make_piece(color, piece), square, king_square);
            }

            // rechreche des pièces à ajouter
            Bitboard to_add = new_pieces & ~old_pieces;
            while (to_add)
            {
                square = BB::pop_lsb(to_add);
                add<side>(entry.accumulator, Move::make_piece(color, piece), square, king_square);
            }
        }
    }

    acc.updated[side] = true;

    entry.colorPiecesBB[side] = board.colorPiecesBB;
    entry.typePiecesBB[side]  = board.typePiecesBB;

    if constexpr (side == WHITE)
        acc.white = entry.accumulator.white;
    else
        acc.black = entry.accumulator.black;
}

template int NNUE::evaluate<WHITE>(const Accumulator& current, size_t count);
template int NNUE::evaluate<BLACK>(const Accumulator& current, size_t count);



