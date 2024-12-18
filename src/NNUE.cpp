#include "NNUE.h"
#include <cassert>
#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include "types.h"
#include "bitmask.h"

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

    return (output + network->output_bias) * SCALE / QAB;
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
    for (Usize i = 0; i < input.size(); ++i)
        input[i] += weights[offset + i];
}

//========================================================================
//! \brief  Suppression d'une feature
//------------------------------------------------------------------------
inline void NNUE::remove_feature(std::array<I16, HIDDEN_LAYER_SIZE> &input,
                                 const std::array<I16, INPUT_LAYER_SIZE * HIDDEN_LAYER_SIZE> &weights,
                                 Usize offset)
{
    for (Usize i = 0; i < input.size(); ++i)
        input[i] -= weights[offset + i];
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
I32 NNUE::activation(const std::array<I16, HIDDEN_LAYER_SIZE>& us,
                     const std::array<I16, HIDDEN_LAYER_SIZE>& them,
                     const std::array<I16, HIDDEN_LAYER_SIZE * 2>& weights)
{
    I32 sum = 0;

    for (Usize i = 0; i < HIDDEN_LAYER_SIZE; ++i)
    {
        sum += screlu(us[i])   * weights[i];
        sum += screlu(them[i]) * weights[HIDDEN_LAYER_SIZE + i];
    }

    return sum / QA;
}

template void NNUE::update_feature<true>(Color color, PieceType piece, int square);
template void NNUE::update_feature<false>(Color color, PieceType piece, int square);

template int NNUE::evaluate<WHITE>();
template int NNUE::evaluate<BLACK>();


