#include "NNUE.h"
#include <cassert>
#include "types.h"
#include "bitmask.h"
#include "simd.h"
#include "Move.h"

namespace {

#include "incbin/incbin.h"
INCBIN(networkData, NETWORK);
const Network *network = reinterpret_cast<const Network *>(gnetworkDataData);

}

//======================================================
//! \brief  Retourne l'évaluation du réseau
//------------------------------------------------------
template<Color color>
int NNUE::evaluate(Usize count)
{
    auto output = 0;
    const auto& current = accumulator.back();
    const int bucket = get_bucket(count);

    if constexpr (color == Color::WHITE)
            output = activation(current.white, current.black, network->output_weights, bucket);
    else
    output = activation(current.black, current.white, network->output_weights, bucket);

    return output;
}

//====================================================
//! \brief  Remise à zéro de tous les accumulateurs
//----------------------------------------------------
void NNUE::reset()
{
    accumulator.clear();
    accumulator.push_back({});  // Adds a new element at the end of the vector, after its current last element.
    accumulator.back().init_biases(network->feature_biases);
}

//====================================================
//! \brief  Remise à zéro l'accumulateur courant
//----------------------------------------------------
template <Color US>
void NNUE::reset_current()
{
    accumulator.back().reset<US>(network->feature_biases);
}

//====================================================
//! \brief  Ajoute 1 nouvel accumulateur
//----------------------------------------------------
void NNUE::push()
{
    auto& current = accumulator.back();  // référence sur le dernier élément
    assert(accumulator.size() < accumulator.capacity());
    accumulator.push_back(current);      // ajoute 1 élément à la fin
}

//====================================================
//! \brief  Supprime 1 accumulateur
//----------------------------------------------------
void NNUE::pop()
{
    accumulator.pop_back(); // supprime le dernier élément
    assert(!accumulator.empty());
}


//========================================================================
//! \brief  Ajout d'une feature
//------------------------------------------------------------------------
void NNUE::add(Piece piece, int from, int wking, int bking)
{
    const auto [white_idx, black_idx] = get_indices(piece, from, wking, bking);

    auto& accu = accumulator.back();

    // SIMD fait perdre "un peu" de perf

    // #if defined USE_SIMD
    //     constexpr int simd_width = sizeof(simd::Vepi16) / sizeof(I16);

    //     for (Usize i = 0; i < HIDDEN_LAYER_SIZE; i += simd_width)
    //     {
    //         auto current = simd::LoadEpi16(&accu.white[i]);
    //         auto value   = simd::LoadEpi16(&network->feature_weights[i+offset_w]);
    //         current      = simd::AddEpi16(current, value);
    //         simd::StoreEpi16(&accu.white[i], current);

    //         current = simd::LoadEpi16(&accu.black[i]);
    //         value   = simd::LoadEpi16(&network->feature_weights[i+offset_b]);
    //         current = simd::AddEpi16(current, value);
    //         simd::StoreEpi16(&accu.black[i], current);
    //     }
    // #else
    for (Usize i = 0; i < HIDDEN_LAYER_SIZE; ++i)
    {
        accu.white[i] += network->feature_weights[white_idx * HIDDEN_LAYER_SIZE + i];
        accu.black[i] += network->feature_weights[black_idx * HIDDEN_LAYER_SIZE + i];
    }
    // #endif
}

//================================================================
//! \brief  Ajoute une pièce
//! \param[in] piece    pièce à ajouter
//! \param[in] color    couleur de cette pièce
//! \param[in] from     position de cette pièce
//! \param[in] king     position du roi de couleur "US"
//----------------------------------------------------------------
template <Color US>
void NNUE::add(Piece piece, int from, int king)
{
    auto& accu = accumulator.back();

    if constexpr (US == WHITE)
    {
        const auto white_idx = get_indice<WHITE>(piece, from, king);

        for (Usize i = 0; i < HIDDEN_LAYER_SIZE; ++i)
            accu.white[i] += network->feature_weights[white_idx * HIDDEN_LAYER_SIZE + i];
    }
    else
    {
        const auto black_idx = get_indice<BLACK>(piece, from, king);

        for (Usize i = 0; i < HIDDEN_LAYER_SIZE; ++i)
            accu.black[i] += network->feature_weights[black_idx * HIDDEN_LAYER_SIZE + i];
    }
}
//=================================================================================
//! \brief  Update combiné : Ajout + Suppression
//! \param[in]  sub_piece       pièce supprimée de from
//! \param[in]  add_piece       pièce ajoutée en dest
//---------------------------------------------------------------------------------
void NNUE::sub_add(Piece sub_piece, int from, Piece add_piece, int dest, int wking, int bking)
{
    // Optimisations de : https://cosmo.tardis.ac/files/2024-06-01-nnue.html

    const auto [white_sub_idx, black_sub_idx] = get_indices(sub_piece, from, wking, bking);
    const auto [white_add_idx, black_add_idx] = get_indices(add_piece, dest, wking, bking);

    auto& accu = accumulator.back();

#if defined USE_SIMD
    constexpr int simd_width = sizeof(simd::Vepi16) / sizeof(I16);

    for (Usize i = 0; i < HIDDEN_LAYER_SIZE; i += simd_width)
    {
        auto cur_w   = simd::LoadEpi16(&accu.white[i]);
        cur_w        = simd::AddEpi16(cur_w, simd::LoadEpi16(&network->feature_weights[white_add_idx * HIDDEN_LAYER_SIZE + i]));
        cur_w        = simd::SubEpi16(cur_w, simd::LoadEpi16(&network->feature_weights[white_sub_idx * HIDDEN_LAYER_SIZE + i]));
        simd::StoreEpi16(&accu.white[i], cur_w);

        auto cur_b = simd::LoadEpi16(&accu.black[i]);
        cur_b      = simd::AddEpi16(cur_b, simd::LoadEpi16(&network->feature_weights[black_add_idx * HIDDEN_LAYER_SIZE + i]));
        cur_b      = simd::SubEpi16(cur_b, simd::LoadEpi16(&network->feature_weights[black_sub_idx * HIDDEN_LAYER_SIZE + i]));
        simd::StoreEpi16(&accu.black[i], cur_b);
    }
#else
    for (Usize i = 0; i < HIDDEN_LAYER_SIZE; ++i)
    {
        accu.white[i] += network->feature_weights[white_add_idx * HIDDEN_LAYER_SIZE + i]
                - network->feature_weights[white_sub_idx * HIDDEN_LAYER_SIZE + i];
        accu.black[i] += network->feature_weights[black_add_idx * HIDDEN_LAYER_SIZE + i]
                - network->feature_weights[black_sub_idx * HIDDEN_LAYER_SIZE + i];
    }
#endif
}

//=================================================================================
//! \brief  Update combiné : Ajout + Suppression + Suppression
//! La pièce supprimée en dest appartient au camp ennemi
//! \param[in]  sub_piece_1     pièce supprimée de from
//! \param[in]  sub_piece_2     pièce ennemie supprimée de dest
//! \param[in]  add_piece       pièce ajoutée en dest
//---------------------------------------------------------------------------------
void NNUE::sub_sub_add(Piece sub_piece_1, int from, Piece sub_piece_2, Piece add_piece, int dest, int wking, int bking)
{
    const auto [white_sub1_idx, black_sub1_idx] = get_indices(sub_piece_1, from, wking, bking);
    const auto [white_sub2_idx, black_sub2_idx] = get_indices(sub_piece_2, dest, wking, bking);   // <<< ~color
    const auto [white_add_idx, black_add_idx]   = get_indices(add_piece, dest, wking, bking);

    auto& accu = accumulator.back();

#if defined USE_SIMD
    constexpr int simd_width = sizeof(simd::Vepi16) / sizeof(I16);

    for (Usize i = 0; i < HIDDEN_LAYER_SIZE; i += simd_width)
    {
        auto cur_w   = simd::LoadEpi16(&accu.white[i]);
        cur_w        = simd::AddEpi16(cur_w, simd::LoadEpi16(&network->feature_weights[white_add_idx * HIDDEN_LAYER_SIZE + i]));
        cur_w        = simd::SubEpi16(cur_w, simd::LoadEpi16(&network->feature_weights[white_sub1_idx * HIDDEN_LAYER_SIZE + i]));
        cur_w        = simd::SubEpi16(cur_w, simd::LoadEpi16(&network->feature_weights[white_sub2_idx * HIDDEN_LAYER_SIZE + i]));
        simd::StoreEpi16(&accu.white[i], cur_w);

        auto cur_b = simd::LoadEpi16(&accu.black[i]);
        cur_b      = simd::AddEpi16(cur_b, simd::LoadEpi16(&network->feature_weights[black_add_idx * HIDDEN_LAYER_SIZE + i]));
        cur_b      = simd::SubEpi16(cur_b, simd::LoadEpi16(&network->feature_weights[black_sub1_idx * HIDDEN_LAYER_SIZE + i]));
        cur_b      = simd::SubEpi16(cur_b, simd::LoadEpi16(&network->feature_weights[black_sub2_idx * HIDDEN_LAYER_SIZE + i]));
        simd::StoreEpi16(&accu.black[i], cur_b);
    }
#else
    for (Usize i = 0; i < HIDDEN_LAYER_SIZE; ++i)
    {
        accu.white[i] += network->feature_weights[white_add_idx * HIDDEN_LAYER_SIZE + i]
                - network->feature_weights[white_sub1_idx * HIDDEN_LAYER_SIZE + i]
                - network->feature_weights[white_sub2_idx * HIDDEN_LAYER_SIZE + i];
        accu.black[i] += network->feature_weights[black_add_idx * HIDDEN_LAYER_SIZE + i]
                - network->feature_weights[black_sub1_idx * HIDDEN_LAYER_SIZE + i]
                - network->feature_weights[black_sub2_idx * HIDDEN_LAYER_SIZE + i];
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
void NNUE::sub_sub_add(Piece sub_piece_1, int from, Piece sub_piece_2, int sub, Piece add_piece, int dest, int wking, int bking)
{
    const auto [white_sub1_idx, black_sub1_idx] = get_indices(sub_piece_1, from, wking, bking);
    const auto [white_sub2_idx, black_sub2_idx] = get_indices(sub_piece_2, sub, wking, bking);   // <<< ~color
    const auto [white_add_idx, black_add_idx]   = get_indices(add_piece, dest, wking, bking);

    auto& accu = accumulator.back();

#if defined USE_SIMD
    constexpr int simd_width = sizeof(simd::Vepi16) / sizeof(I16);

    for (Usize i = 0; i < HIDDEN_LAYER_SIZE; i += simd_width)
    {
        auto cur_w   = simd::LoadEpi16(&accu.white[i]);
        cur_w        = simd::AddEpi16(cur_w, simd::LoadEpi16(&network->feature_weights[white_add_idx * HIDDEN_LAYER_SIZE + i]));
        cur_w        = simd::SubEpi16(cur_w, simd::LoadEpi16(&network->feature_weights[white_sub1_idx * HIDDEN_LAYER_SIZE + i]));
        cur_w        = simd::SubEpi16(cur_w, simd::LoadEpi16(&network->feature_weights[white_sub2_idx * HIDDEN_LAYER_SIZE + i]));
        simd::StoreEpi16(&accu.white[i], cur_w);

        auto cur_b = simd::LoadEpi16(&accu.black[i]);
        cur_b      = simd::AddEpi16(cur_b, simd::LoadEpi16(&network->feature_weights[black_add_idx * HIDDEN_LAYER_SIZE + i]));
        cur_b      = simd::SubEpi16(cur_b, simd::LoadEpi16(&network->feature_weights[black_sub1_idx * HIDDEN_LAYER_SIZE + i]));
        cur_b      = simd::SubEpi16(cur_b, simd::LoadEpi16(&network->feature_weights[black_sub2_idx * HIDDEN_LAYER_SIZE + i]));
        simd::StoreEpi16(&accu.black[i], cur_b);
    }
#else
    for (Usize i = 0; i < HIDDEN_LAYER_SIZE; ++i)
    {
        accu.white[i] += network->feature_weights[white_add_idx * HIDDEN_LAYER_SIZE + i]
                - network->feature_weights[white_sub1_idx * HIDDEN_LAYER_SIZE + i]
                - network->feature_weights[white_sub2_idx * HIDDEN_LAYER_SIZE + i];
        accu.black[i] += network->feature_weights[black_add_idx * HIDDEN_LAYER_SIZE + i]
                - network->feature_weights[black_sub1_idx * HIDDEN_LAYER_SIZE + i]
                - network->feature_weights[black_sub2_idx * HIDDEN_LAYER_SIZE + i];
    }
#endif
}
//========================================================================
//! \brief  Calcule l'indece du triplet (couleur, piece, case)
//! dans l'Input Layer
//------------------------------------------------------------------------
std::pair<Usize, Usize> NNUE::get_indices(Piece piece, int square, int wking, int bking)
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

    assert(piece  != Piece::NONE);
    assert(square != SquareType::SQUARE_NONE);

    constexpr Usize color_stride = N_SQUARES * 6;
    constexpr Usize piece_stride = N_SQUARES;

    const auto base  = static_cast<U32>(Move::type(piece));
    const auto color = Move::color(piece);

    //TODO : refaire le codage des pièces ?
    //  moi  : PAWN = 1
    //  NNUE : PAWN = 0
    const auto base_nnue = base - 1;

    const auto white_indice =  color * color_stride + base_nnue * piece_stride + static_cast<Usize>(get_square(square, wking));
    const auto black_indice = !color * color_stride + base_nnue * piece_stride + static_cast<Usize>(SQ::mirrorVertically(get_square(square, bking)));

    return {white_indice, black_indice};
}

template <Color US>
Usize NNUE::get_indice(Piece piece, int square, int king)
{
    assert(piece  != Piece::NONE);
    assert(square != SquareType::SQUARE_NONE);

    constexpr Usize color_stride = N_SQUARES * 6;
    constexpr Usize piece_stride = N_SQUARES;

    const auto base  = static_cast<U32>(Move::type(piece));
    const auto color = Move::color(piece);

    const auto base_nnue = base - 1;

    if constexpr (US == WHITE)
        return  color * color_stride + base_nnue * piece_stride + static_cast<Usize>(get_square(square, king));
    else
        return !color * color_stride + base_nnue * piece_stride + static_cast<Usize>(SQ::mirrorVertically(get_square(square, king)));
}

//=========================================
//! \brief  Calcul de la valeur résultante des 2 accumulateurs
//! et de leur poids
//-----------------------------------------
#if defined USE_SIMD

constexpr int kChunkSize = sizeof(simd::Vepi16) / sizeof(I16);


I32 NNUE::activation(const std::array<I16, HIDDEN_LAYER_SIZE>& us,
                     const std::array<I16, HIDDEN_LAYER_SIZE>& them,
                     const std::array<I16, HIDDEN_LAYER_SIZE * N_COLORS * OUTPUT_BUCKETS>& weights,
                     const int bucket)
{
    // Routine provenant de Integral
    // Merci pour les commentaires )

    // Doc : https://cosmo.tardis.ac/files/2024-06-01-nnue.html
    // On va utiliser l'algorithme "Lizard"

    const int adresse = bucket * N_COLORS * HIDDEN_LAYER_SIZE;

    auto sum = simd::ZeroEpi32();

    // Compute evaluation from our perspective
    for (Usize i = 0; i < HIDDEN_LAYER_SIZE; i += kChunkSize)
    {
        const auto input  = simd::LoadEpi16(&us[i]);
        const auto weight = simd::LoadEpi16(&weights[adresse + i]);

        // Clip the accumulator values
        const auto clipped = simd::Clip(input, QA);

        // compute t = v * w in 16-bit
        // this step relies on v being less than 256 and abs(w) being less than 127,
        // so abs(v * w) is at most 255*126=32130, less than i16::MAX, which is 32767,
        // so the output still fits in i16.

        // Multiply weights by clipped values (still in i16, no overflow)
        const auto product = simd::MultiplyEpi16(clipped, weight);

        // Perform the second multiplication with widening to i32,
        // accumulating the result
        const auto result = simd::MultiplyAddEpi16(product, clipped);

        // Accumulate the results in 32-bit integers
        sum = simd::AddEpi32(sum, result);
    }

    // Compute evaluation from their perspective
    for (Usize i = 0; i < HIDDEN_LAYER_SIZE; i += kChunkSize)
    {
        const auto input  = simd::LoadEpi16(&them[i]);
        const auto weight = simd::LoadEpi16(&weights[adresse + HIDDEN_LAYER_SIZE + i]);

        // Clip the accumulator values
        const auto clipped = simd::Clip(input, QA);

        // Multiply weights by clipped values (still in i16, no overflow)
        const auto product = simd::MultiplyEpi16(clipped, weight);

        // Perform the second multiplication with widening to i32,
        // accumulating the result
        const auto result = simd::MultiplyAddEpi16(product, clipped);

        // Accumulate the results in 32-bit integers
        sum = simd::AddEpi32(sum, result);
    }

    // Perform a horizontal sum to get the final result
    I32 eval = simd::ReduceAddEpi32(sum);

    // De-quantize the evaluation because of our squared activation function
    eval /= QA;

    // Add final output bias
    eval += network->output_bias[bucket];

    // Scale the evaluation
    eval *= SCALE;

    // De-quantize again
    eval /= QAB;

    return eval;

}

#else

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



template int NNUE::evaluate<WHITE>(Usize count);
template int NNUE::evaluate<BLACK>(Usize count);

template void NNUE::reset_current<WHITE>();
template void NNUE::reset_current<BLACK>();

template void NNUE::add<WHITE>(Piece piece, int from, int king);
template void NNUE::add<BLACK>(Piece piece, int from, int king);
