#ifndef NNUE_H
#define NNUE_H

#include <algorithm>
#include <array>
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
#include "bitmask.h"

//  Description du réseau : (768 -> 768)x2 -> 1x8, SquaredClippedReLU
//
//      entrées              : 2 (couleurs) * 6 (pièces) * 64 (cases)  = 768
//      neurons              : 768 ; 1 seule couche
//      perspective          : sorties : 2 * HIDDEN_LAYER_SIZE * OUTPUT_BUCKETS
//      Output Buckets       :
//      Horizontal Mirroring :
//      SquareClippedRelu    : l'activation se fait en "clippant" la valeur entre 0 et QA (ici 255)


// https://www.chessprogramming.org/NNUE#Output_Buckets

constexpr Usize INPUT_LAYER_SIZE  = N_COLORS * 6 * N_SQUARES;   // = 768 : entrées (note : il n'y a que 6 pièces
constexpr Usize HIDDEN_LAYER_SIZE = 768;                        // Hidden Layer : nombre de neuron(es) ; va de 16 à ... 1024 (plus ?)
constexpr Usize OUTPUT_BUCKETS    = 8;  //TODO à voir ?
constexpr Usize BUCKET_DIVISOR    = (32 + OUTPUT_BUCKETS - 1) / OUTPUT_BUCKETS;   // 4.875 -> 4


constexpr I32 SCALE = 400;
constexpr I32 QA    = 255;      /// Hidden Layer Quantisation Factor
constexpr I32 QB    =  64;      /// Output Layer Quantisation Factor
constexpr I32 QAB   = QA * QB;

constexpr std::array<int, N_SQUARES> buckets = {    // tableau servant à connaitre si la roi a changé de coté
        0, 0, 0, 0, 1, 1, 1, 1,
        0, 0, 0, 0, 1, 1, 1, 1,
        0, 0, 0, 0, 1, 1, 1, 1,
        0, 0, 0, 0, 1, 1, 1, 1,
        0, 0, 0, 0, 1, 1, 1, 1,
        0, 0, 0, 0, 1, 1, 1, 1,
        0, 0, 0, 0, 1, 1, 1, 1,
        0, 0, 0, 0, 1, 1, 1, 1,
};

//  Architecture du réseau
struct Network {
    alignas(ALIGN) std::array<I16, INPUT_LAYER_SIZE * HIDDEN_LAYER_SIZE>  feature_weights;
    alignas(ALIGN) std::array<I16, HIDDEN_LAYER_SIZE>                     feature_biases;

    /* structure provenant de Bullet :
     * on a 2 possibilités :
     *      non transposed : output_weights[N_COLORS][HIDDEN_SIZE][OUTPUT_BUCKETS];
     *      transposed     : output_weights[OUTPUT_BUCKETS][N_COLORS][HIDDEN_SIZE];
     * Bullet sort maintenant la version "transposed"
     */

    alignas(ALIGN) std::array<I16, N_COLORS * HIDDEN_LAYER_SIZE * OUTPUT_BUCKETS> output_weights;
    alignas(ALIGN) std::array<I16, OUTPUT_BUCKETS>                                output_bias;
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

    template <Color US>
    void reset(std::span<const I16, HIDDEN_LAYER_SIZE> biases) {
        if constexpr (US == WHITE) {
            std::copy(biases.begin(), biases.end(), white.begin());
        } else {
            std::copy(biases.begin(), biases.end(), black.begin());
        }
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
    template <Color US> void reset_current();

    template<Color color> int  evaluate(Usize count);
    inline void reserve_capacity() { accumulator.reserve(512); }  // la capacité ne passe pas avec la copie

    void add(Piece piece, int from, int wking, int bking);
    void sub_add(Piece from_piece, int from, Piece to_piece, int dest, int wking, int bking);
    void sub_sub_add(Piece sub_piece_1, int from, Piece sub_piece_2, Piece add_piece, int dest, int wking, int bking);
    void sub_sub_add(Piece sub_piece_1, int from, Piece sub_piece_2, int sub, Piece add_piece, int dest, int wking, int bking);

    template <Color US> void add(Piece piece, int from, int king);

private:

    std::vector<Accumulator> accumulator;   // pile des accumulateurs

    I32 activation(const std::array<I16, HIDDEN_LAYER_SIZE>& us,
                   const std::array<I16, HIDDEN_LAYER_SIZE>& them,
                   const std::array<I16, N_COLORS * HIDDEN_LAYER_SIZE * OUTPUT_BUCKETS>& weights,
                   const int bucket);

    std::pair<Usize, Usize> get_indices(Piece piece, int square, int wking, int bking);
    template <Color US> Usize get_indice(Piece piece, int square, int king);


    constexpr inline I32 screlu(I16 x) {
        auto clamped = std::clamp(static_cast<I32>(x), 0, QA);
        return clamped * clamped;
    }

    // count = 32 : b = 7.5
    //          2 :     0
    inline int get_bucket(Usize count) {
        return (count - 2) / BUCKET_DIVISOR;
    }

    // Curieusement, le test inverse est pire (en elo)
    inline int get_square(int square, int king_square) {
        return SQ::file(king_square) > FILE_D ? SQ::mirrorHorizontally(square) : square;
    }
};

#endif // NNUE_H
