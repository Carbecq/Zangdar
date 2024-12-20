#ifndef NNUE_H
#define NNUE_H

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <vector>
#include "types.h"

//  Description du réseau : (768->128)x2->1, ClippedReLU
//
//      entrées     : 2 (couleurs) * 6 (pièces) * 64 (cases)  = 768
//      neurones    : 128 ; 1 seule couche
//      perspective : sorties : 2 * HIDDEN_LAYER_SIZE
//      ClippedRelu : l'activation se fait en "clippant" la valeur entre 0 et QA (ici 255)
//                      int activated = std::clamp(static_cast<int>(us[i]), 0, 255);

//  Voir aussi les codes de
//      Clarity     (768->256)x2->1  crelu, perspective
//      Leorik      (768->256)x2->1  crelu, perspective
//      Pedantic    (768->384)x2->1  screlu, perspective


constexpr Usize INPUT_LAYER_SIZE  = N_COLORS * 6 * N_SQUARES;      // = 768 : entrées (note : il n'y a que 6 pièces
constexpr Usize HIDDEN_LAYER_SIZE = 128;     // Hidden Layer : nombre de neuron(es) ; va de 16 à ... 1024 (plus ?)

constexpr I32 SCALE = 400;
constexpr I32 QA  = 255;        // constante de clamp (crelu)
constexpr I32 QB  =  64;
constexpr I32 QAB = QA * QB;

//  Architecture du réseau
//  alignement sur 32 octets pour AVX2 (??)
struct Network {
    alignas(32) std::array<I16, INPUT_LAYER_SIZE * HIDDEN_LAYER_SIZE> feature_weights;
    alignas(32) std::array<I16, HIDDEN_LAYER_SIZE> feature_biases;
    alignas(32) std::array<I16, HIDDEN_LAYER_SIZE * 2> output_weights;
    I16 output_bias;
};


// Du fait de la perspective, il y a 2 accumulateurs
struct Accumulator {
    alignas(32) std::array<I16, HIDDEN_LAYER_SIZE> white;
    alignas(32) std::array<I16, HIDDEN_LAYER_SIZE> black;

    void init_biases(std::span<const I16, HIDDEN_LAYER_SIZE> biases)
    {
        std::copy(biases.begin(), biases.end(), white.begin());
        std::copy(biases.begin(), biases.end(), black.begin());
    }
};

class NNUE
{
public:
    explicit NNUE() {
        accumulator.reserve(512);
    }
    ~NNUE() = default;

    void push();
    void pop();
    void reset();
    template<Color color> int  evaluate();
    template <bool add_to_square> void update_feature(Color color, PieceType piece, int square);

private:
    std::vector<Accumulator> accumulator;   // pile des accumulateurs

    I32 activation(const std::array<I16, HIDDEN_LAYER_SIZE>& us,
                   const std::array<I16, HIDDEN_LAYER_SIZE>& them,
                   const std::array<I16, HIDDEN_LAYER_SIZE * 2>& weights);

    std::pair<Usize, Usize> get_indices(Color color, PieceType piece, int square);
    inline void add_feature(std::array<I16, HIDDEN_LAYER_SIZE> &input,
                            const std::array<I16, INPUT_LAYER_SIZE * HIDDEN_LAYER_SIZE> &weights,
                            Usize offset);
    inline void remove_feature(std::array<I16, HIDDEN_LAYER_SIZE> &input,
                               const std::array<I16, INPUT_LAYER_SIZE * HIDDEN_LAYER_SIZE> &weights,
                               Usize offset);

    //====================================
    //! \brief Calcule la valeur clampée
    //------------------------------------
    constexpr inline I32 crelu(I16 x) { return std::clamp(static_cast<I32>(x), 0, QA); }
};

#endif // NNUE_H
