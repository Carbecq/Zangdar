#include "NNUE.h"
#include <cassert>
#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include "types.h"
#include "bitmask.h"
#include "simd.h"

namespace {

#include "incbin/incbin.h"
INCBIN(networkData, NETWORK);
const Network *network = reinterpret_cast<const Network *>(gnetworkDataData);

}

//======================================================
//! \brief  Retourne l'évaluation du réseau
//------------------------------------------------------
template<Color color>
int NNUE::evaluate()
{
    auto output = 0;
    const auto& current = accumulator.back();

    if constexpr (color == Color::WHITE)
        output = activation(current.white, current.black, network->output_weights);
    else
        output = activation(current.black, current.white, network->output_weights);

    return output;
}

//====================================================
//! \brief  Remise à zéro
//----------------------------------------------------
void NNUE::reset()
{
    accumulator.clear();
    accumulator.push_back({});
    accumulator.back().init_biases(network->feature_biases);
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
//! \brief  Modification d'une feature
//------------------------------------------------------------------------
template <bool add_to_square>
void NNUE::update_feature(Color color, PieceType piece, int square)
{
    const auto [white_idx, black_idx] = get_indices(color, piece, square);

    auto& current = accumulator.back();
    if constexpr (add_to_square)
    {
        add_feature(current.white, network->feature_weights, white_idx * HIDDEN_LAYER_SIZE);
        add_feature(current.black, network->feature_weights, black_idx * HIDDEN_LAYER_SIZE);
    }
    else
    {
        remove_feature(current.white, network->feature_weights, white_idx * HIDDEN_LAYER_SIZE);
        remove_feature(current.black, network->feature_weights, black_idx * HIDDEN_LAYER_SIZE);
    }
}

//========================================================================
//! \brief  Ajout d'une feature
//------------------------------------------------------------------------
inline void NNUE::add_feature(std::array<I16, HIDDEN_LAYER_SIZE> &input,
                              const std::array<I16, INPUT_LAYER_SIZE * HIDDEN_LAYER_SIZE> &weights,
                              Usize offset)
{
#if defined USE_SIMD
    constexpr int simd_width = sizeof(simd::Vepi16) / sizeof(I16);

    for (Usize i = 0; i < HIDDEN_LAYER_SIZE; i += simd_width)
    {
        auto current = simd::LoadEpi16(&input[i]);
        auto value   = simd::LoadEpi16(&weights[i+offset]);
        current      = simd::AddEpi16(current, value);
        simd::StoreEpi16(&input[i], current);
    }
#else
    for (Usize i = 0; i < HIDDEN_LAYER_SIZE; ++i)
        input[i] += weights[offset + i];
#endif
}

//========================================================================
//! \brief  Suppression d'une feature
//------------------------------------------------------------------------
inline void NNUE::remove_feature(std::array<I16, HIDDEN_LAYER_SIZE> &input,
                                 const std::array<I16, INPUT_LAYER_SIZE * HIDDEN_LAYER_SIZE> &weights,
                                 Usize offset)
{
#if defined USE_SIMD
    constexpr int simd_width = sizeof(simd::Vepi16) / sizeof(I16);

    for (Usize i = 0; i < HIDDEN_LAYER_SIZE; i += simd_width)
    {
        auto current = simd::LoadEpi16(&input[i]);
        auto value   = simd::LoadEpi16(&weights[i+offset]);
        current      = simd::SubEpi16(current, value);
        simd::StoreEpi16(&input[i], current);
    }
#else
    for (Usize i = 0; i < HIDDEN_LAYER_SIZE; ++i)
        input[i] -= weights[offset + i];
#endif
}

//========================================================================
//! \brief  Calcule l'indece du triplet (couleur, piece, case)
//! dans l'Input Layer
//------------------------------------------------------------------------
std::pair<Usize, Usize> NNUE::get_indices(Color color, PieceType piece, int square)
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

    assert(piece  != PieceType::NO_PIECE);
    assert(square != SquareType::NO_SQUARE);

    constexpr Usize color_stride = N_SQUARES * 6;
    constexpr Usize piece_stride = N_SQUARES;

    //TODO : refaire le codage des pièces ?
    //  PieceType : PAWN = 1
    //  pour NNUE : PAWN = 0
    const Usize piece_nnue = static_cast<Usize>(piece - 1);

    const auto white_indice =  color * color_stride + piece_nnue * piece_stride + static_cast<Usize>(square);
    const auto black_indice = !color * color_stride + piece_nnue * piece_stride + static_cast<Usize>(SQ::mirrorVertically(square));

    return {white_indice, black_indice};
}

//=========================================
//! \brief  Calcul de la valeur résultante des 2 accumulateurs
//! et de leur poids
//-----------------------------------------
#if defined USE_SIMD

constexpr int kChunkSize = sizeof(simd::Vepi16) / sizeof(I16);


I32 NNUE::activation(const std::array<I16, HIDDEN_LAYER_SIZE>& us,
                     const std::array<I16, HIDDEN_LAYER_SIZE>& them,
                     const std::array<I16, HIDDEN_LAYER_SIZE * 2>& weights)
{
    // Routine provenant de Integral
    // Merci pour les commentaires )

    // Doc : https://cosmo.tardis.ac/files/2024-06-01-nnue.html
    // On va utiliser l'algorithme "Lizard"

    auto sum = simd::ZeroEpi32();

    // Compute evaluation from our perspective
    for (Usize i = 0; i < HIDDEN_LAYER_SIZE; i += kChunkSize)
    {
        const auto input  = simd::LoadEpi16(&us[i]);
        const auto weight = simd::LoadEpi16(&weights[i]);

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
        const auto weight = simd::LoadEpi16(&weights[i + HIDDEN_LAYER_SIZE]);

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
    eval += network->output_bias;

    // Scale the evaluation
    eval *= SCALE;

    // De-quantize again
    eval /= QAB;

    return eval;

}

#else

I32 NNUE::activation(const std::array<I16, HIDDEN_LAYER_SIZE>& us,
                     const std::array<I16, HIDDEN_LAYER_SIZE>& them,
                     const std::array<I16, HIDDEN_LAYER_SIZE * 2>& weights)
{
    I32 eval = 0;

    for (Usize i = 0; i < HIDDEN_LAYER_SIZE; ++i)
    {
        eval += screlu(us[i])   * weights[i];
        eval += screlu(them[i]) * weights[HIDDEN_LAYER_SIZE + i];
    }

    eval /= QA;
    eval += network->output_bias;
    eval *= SCALE;
    eval /= QAB;

    return eval;
}

#endif


template void NNUE::update_feature<true>(Color color, PieceType piece, int square);
template void NNUE::update_feature<false>(Color color, PieceType piece, int square);

template int NNUE::evaluate<WHITE>();
template int NNUE::evaluate<BLACK>();


