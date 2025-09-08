#ifndef ACCUMULATOR_H
#define ACCUMULATOR_H

#include <span>
#include "types.h"
#include "bitmask.h"

#if defined(__AVX512F__)
#define ALIGN   64
#elif defined(__AVX2__)
#define ALIGN   32
#elif defined(__SSE2__) || defined(__AVX__)
#define ALIGN   16
#elif defined(__ARM_NEON)
#define ALIGN   16
#endif

//------------------------------------------------------------------------------
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

//------------------------------------------------------------------------------
// Lazy Updates
struct SquarePiece {
    int   square = Square::SQUARE_NONE;
    Piece  piece = Piece::NONE;
};

struct DirtyPieces {
    SquarePiece sub_1, add_1, sub_2, add_2;

    enum Type {
        NORMAL,
        CAPTURE,
        CASTLING
    } type = NORMAL;
};

class Board;



//------------------------------------------------------------------------------
//  Un accumulateur contient 2 perspectives.

class Accumulator
{
public:
    Accumulator() {}

    alignas(ALIGN) std::array<I16, HIDDEN_LAYER_SIZE> white;
    alignas(ALIGN) std::array<I16, HIDDEN_LAYER_SIZE> black;

    // Lazy Updates
    bool updated[N_COLORS]      = {false, false};
    DirtyPieces dirtyPieces     = DirtyPieces{};
    std::array<int, N_COLORS> king_square = {SQUARE_NONE, SQUARE_NONE};     // position des rois

    void init_biases(std::span<const I16, HIDDEN_LAYER_SIZE> biases)
    {
        std::copy(biases.begin(), biases.end(), white.begin());
        std::copy(biases.begin(), biases.end(), black.begin());
    }

private:

};

#endif // ACCUMULATOR_H
