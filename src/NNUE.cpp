#include "NNUE.h"
#include <cassert>
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
    accumulator.push_back({});  // Adds a new element at the end of the vector, after its current last element.
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
//! \brief  Ajout d'une feature
//------------------------------------------------------------------------
void NNUE::add(Color color, PieceType piece, int from)
{
    const auto [white_idx, black_idx] = get_indices(color, piece, from);

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

//=================================================================================
//! \brief  Update combiné : Ajout + Suppression
//! \param[in]  sub_piece       pièce supprimée de from
//! \param[in]  add_piece       pièce ajoutée en dest
//---------------------------------------------------------------------------------
void NNUE::sub_add(Color color, PieceType sub_piece, int from, PieceType add_piece, int dest)
{
    // Optimisations de : https://cosmo.tardis.ac/files/2024-06-01-nnue.html

    const auto [white_sub_idx, black_sub_idx] = get_indices(color, sub_piece, from);
    const auto [white_add_idx, black_add_idx] = get_indices(color, add_piece, dest);

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
void NNUE::sub_sub_add(Color color, PieceType sub_piece_1, int from, PieceType sub_piece_2, PieceType add_piece, int dest)
{
    const auto [white_sub1_idx, black_sub1_idx] = get_indices(color, sub_piece_1, from);
    const auto [white_sub2_idx, black_sub2_idx] = get_indices(~color, sub_piece_2, dest);   // <<< ~color
    const auto [white_add_idx, black_add_idx]   = get_indices(color, add_piece, dest);

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
void NNUE::sub_sub_add(Color color, PieceType sub_piece_1, int from, PieceType sub_piece_2, int sub, PieceType add_piece, int dest)
{
    const auto [white_sub1_idx, black_sub1_idx] = get_indices(color, sub_piece_1, from);
    const auto [white_sub2_idx, black_sub2_idx] = get_indices(~color, sub_piece_2, sub);   // <<< ~color
    const auto [white_add_idx, black_add_idx]   = get_indices(color, add_piece, dest);

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

    assert(piece  != PieceType::PIECE_NONE);
    assert(square != SquareType::SQUARE_NONE);

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


template int NNUE::evaluate<WHITE>();
template int NNUE::evaluate<BLACK>();


