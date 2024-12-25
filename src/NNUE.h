#ifndef NNUE_H
#define NNUE_H

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <vector>

#if defined(__AVX512F__)
#define ALIGN   64
#elif defined(__AVX2__)
#define ALIGN   32
#elif defined(__SSE2__) || defined(__AVX__)
#define ALIGN   16
#elif defined(__ARM_NEON)
#define ALIGN   16
#endif


#if defined __AVX2__
#include <immintrin.h>
#endif

#include "types.h"

//  Description du réseau : (768->S56)x2->1, SquaredClippedReLU
//
//      entrées     : 2 (couleurs) * 6 (pièces) * 64 (cases)  = 768
//      neurones    : 128 ; 1 seule couche
//      perspective : sorties : 2 * HIDDEN_LAYER_SIZE
//      ClippedRelu : l'activation se fait en "clippant" la valeur entre 0 et QA (ici 255)
//                      int activated = std::clamp(static_cast<int>(us[i]), 0, 255);



constexpr Usize INPUT_LAYER_SIZE  = N_COLORS * 6 * N_SQUARES;      // = 768 : entrées (note : il n'y a que 6 pièces
constexpr Usize HIDDEN_LAYER_SIZE = 256;     // Hidden Layer : nombre de neuron(es) ; va de 16 à ... 1024 (plus ?)

constexpr I32 SCALE = 400;
constexpr I32 QA    = 255;      /// Hidden Layer Quantisation Factor
constexpr I32 QB    =  64;      /// Output Layer Quantisation Factor
constexpr I32 QAB   = QA * QB;

//  Architecture du réseau
struct Network {
    alignas(ALIGN) std::array<I16, INPUT_LAYER_SIZE * HIDDEN_LAYER_SIZE> feature_weights;
    alignas(ALIGN) std::array<I16, HIDDEN_LAYER_SIZE> feature_biases;
    alignas(ALIGN) std::array<I16, HIDDEN_LAYER_SIZE * 2> output_weights;
    I16 output_bias;
};


// Du fait de la perspective, il y a 2 accumulateurs
struct Accumulator {
    alignas(ALIGN) std::array<I16, HIDDEN_LAYER_SIZE> white;
    alignas(ALIGN) std::array<I16, HIDDEN_LAYER_SIZE> black;

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
    inline void reserve_nnue_capacity() { accumulator.reserve(512); }  // la capacité ne passe pas avec la copie


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

    constexpr inline I32 crelu(I16 x) { return std::clamp(static_cast<I32>(x), 0, QA); }
    constexpr inline I32 screlu(I16 x) {
        auto clamped = std::clamp(static_cast<I32>(x), 0, QA);
        return clamped * clamped;
    }

#if defined __AVX2__
#endif

};

#endif // NNUE_H
