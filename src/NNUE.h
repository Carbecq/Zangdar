#ifndef NNUE_H
#define NNUE_H

#include <algorithm>
#include <array>
#include <span>
#include "types.h"
#include "bitmask.h"

#if defined __AVX2__
#include <immintrin.h>
#endif

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
//  Description du réseau : (768x4hm -> 768)x2 -> 1x8  SquaredClippedReLU
//
//      entrées              : 2 (couleurs) * 6 (pièces) * 64 (cases)  = 768
//      Input Buckets        : 4
//      Horizontal Mirroring :
//      neurons              : 768 ; 1 seule couche
//      perspective          : sorties : 2 * HIDDEN_LAYER_SIZE * OUTPUT_BUCKETS
//      Output Buckets       : 8
//      SquareClippedRelu    : l'activation se fait en "clippant" la valeur entre 0 et QA (ici 255)


// https://www.chessprogramming.org/NNUE#Output_Buckets

constexpr Usize INPUT_LAYER_SIZE  = N_COLORS * 6 * N_SQUARES;   // = 768 : entrées (note : il n'y a que 6 pièces
constexpr Usize HIDDEN_LAYER_SIZE = 1024;                        // Hidden Layer : nombre de neuron(es) ; va de 16 à ... 1024 (plus ?)
constexpr Usize OUTPUT_BUCKETS    = 8;  //TODO à voir ?
constexpr Usize BUCKET_DIVISOR    = (32 + OUTPUT_BUCKETS - 1) / OUTPUT_BUCKETS;   // 4.875 -> 4

constexpr I32 SCALE = 400;
constexpr I32 QA    = 255;      /// Hidden Layer Quantisation Factor
constexpr I32 QB    =  64;      /// Output Layer Quantisation Factor
constexpr I32 QAB   = QA * QB;

constexpr int KING_BUCKETS_COUNT = 4;    // max king bucket
constexpr std::array<int, N_SQUARES> king_buckets_map = {    // tableau servant à connaitre si la roi a changé de bucket
                                                  0, 0, 1, 1, 1, 1, 0, 0,
                                                  2, 2, 2, 2, 2, 2, 2, 2,
                                                  3, 3, 3, 3, 3, 3, 3, 3,
                                                  3, 3, 3, 3, 3, 3, 3, 3,
                                                  3, 3, 3, 3, 3, 3, 3, 3,
                                                  3, 3, 3, 3, 3, 3, 3, 3,
                                                  3, 3, 3, 3, 3, 3, 3, 3,
                                                  3, 3, 3, 3, 3, 3, 3, 3,
};


//-----------------------------------------------------------------------
//  Architecture du réseau
struct Network {
    alignas(ALIGN) std::array<I16, KING_BUCKETS_COUNT * INPUT_LAYER_SIZE * HIDDEN_LAYER_SIZE>  feature_weights;
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

//-----------------------------------------------------------------------
//  Finny Tables
//  https://www.chessprogramming.org/NNUE#Accumulator_Refresh_Table

struct FinnyEntry {
    Array2D<Bitboard, N_COLORS, N_PIECE_TYPE> typePiecesBB  = {{}};
    Array2D<Bitboard, N_COLORS, N_COLORS>     colorPiecesBB = {{}};
    Accumulator accumulator = {};

    void init();
};

class Board;

//-----------------------------------------------------------------------

class NNUE
{
public:
    explicit NNUE() : head_idx(0) {}
    ~NNUE() = default;

    inline const Accumulator& get_accumulator() const { return stack[head_idx]; }
    inline Accumulator& get_accumulator()             { return stack[head_idx]; }

    void init();
    void start_search(const Board& board);
    void push();
    void pop();
    void init_accumulator(Accumulator &acc);
    template <Color side> void refresh_accumulator(const Board* board, Accumulator& acc);

    template<Color color> int  evaluate(const Accumulator& current, Usize count);

    void add(Accumulator &accu,
             Piece piece, int from,
             int wking, int bking);

    void lazy_updates(const Board *board, Accumulator &acc);


    I32 activation(const std::array<I16, HIDDEN_LAYER_SIZE>& us,
                   const std::array<I16, HIDDEN_LAYER_SIZE>& them,
                   const std::array<I16, N_COLORS * HIDDEN_LAYER_SIZE * OUTPUT_BUCKETS>& weights,
                   const int bucket);


    template <Color side> void add(Accumulator& accu, Piece piece, int from, int king);


private:
    std::array<Accumulator, MAX_PLY> stack = {Accumulator{}};       // pile des accumulateurs
    Usize head_idx;                                                 // accumulateur utilisé (= stack_size - 1)
    FinnyEntry finny[N_COLORS][KING_BUCKETS_COUNT] = {};            // tables Finny

    template <Color side> void lazy_update(const Board* board, Accumulator& head);
    template <Color side> void update(const Accumulator &src, Accumulator& dst, int king);
    template <Color side> bool need_refresh(int oldKing, int newKing) ;

    template <Color side>
    void sub(Accumulator& accu, Piece piece, int from, int king);

    template <Color side>
    void sub_add(const Accumulator& src, Accumulator& dst,
                       Piece sub_piece, int sub,
                       Piece add_piece, int add,
                       int king);

    template <Color side>
    void sub_sub_add(const Accumulator &src, Accumulator &dst,
                     Piece sub_piece_1, int sub_1,
                     Piece sub_piece_2, int sub_2,
                     Piece add_piece_1, int add_1,
                     int king);

    template <Color side>
    void sub_sub_add_add(const Accumulator &src, Accumulator &dst,
                         Piece sub_piece_1, int sub_1,
                         Piece sub_piece_2, int sub_2,
                         Piece add_piece_1, int add_1,
                         Piece add_piece_2, int add_2,
                         int king);

    // count = 32 : b = 7.5
    //          2 :     0
    inline int get_bucket(Usize count) const {
        return (count - 2) / BUCKET_DIVISOR;
    }

     //! \brief Retourne la case relative à la perspective
    template <Color side>
    inline int get_relative_square(int square) const {
        if constexpr (side == WHITE)
            return square;
        else
            return SQ::mirrorVertically(square);
    }

    //  Retourne la case, en tenant compte du Horizontal Mirroring
    //  Curieusement, le test inverse est pire (en elo)
    //      king_square & 0b100 = 0 [FILE_A ... FILE_D] , 4 [FILE_D ... FILE_H]
    inline int get_square(int square, int king_square) const {
        return king_square & 0b100 ? SQ::mirrorHorizontally(square) : square;
    }

    std::pair<Usize, Usize> get_indices(Piece piece, int square, int wking, int bking);
    template <Color side>Usize get_indice(Piece piece, int square, int king);


    constexpr inline I32 screlu(I16 x) {
        auto clamped = std::clamp(static_cast<I32>(x), 0, QA);
        return clamped * clamped;
    }

};

#endif // NNUE_H
