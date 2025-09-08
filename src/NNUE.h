#ifndef NNUE_H
#define NNUE_H

#include <algorithm>
#include <array>

#if defined __AVX2__
#include <immintrin.h>
#endif

#include "types.h"
#include "bitmask.h"
#include "Accumulator.h"


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

//-----------------------------------------------------------------------
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


//-----------------------------------------------------------------------
//  Finny Tables
//  https://www.chessprogramming.org/NNUE#Accumulator_Refresh_Table

struct FinnyEntry {
    Array2D<Bitboard, N_COLORS, N_PIECE_TYPE> typePiecesBB  = {{}};
    Array2D<Bitboard, N_COLORS, N_COLORS>     colorPiecesBB = {{}};
    Accumulator accumulator = {};

    void init();
};


//-----------------------------------------------------------------------

class NNUE
{
public:
    explicit NNUE() : head_idx(0) {}
    ~NNUE() = default;


    inline const Accumulator& get_accumulator() const { return stack[head_idx]; }
    inline Accumulator& get_accumulator()             { return stack[head_idx]; }

    void init();
    void push();
    void pop();
    void init_accumulator(Accumulator &acc);
    void set_accumulator(const Board* board, Accumulator& acc);
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
    std::array<Accumulator, MAX_HISTO> stack = {Accumulator{}};   // pile des accumulateurs
    Usize head_idx;                                               // accumulateur utilisé (= stack_size - 1)
    static const int N_KING_BUCKETS = 1;                            // pour le futur !!
    FinnyEntry finny[N_COLORS][N_KING_BUCKETS] = {};                // tables Finny

    template <Color side> void lazy_update(const Board* board, Accumulator& head);
    template <Color side> void update(const Accumulator &src, Accumulator& dst, int king);
    bool need_refresh(int oldKing, int newKing) ;

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
    inline int get_bucket(Usize count) {
        return (count - 2) / BUCKET_DIVISOR;
    }

    // Curieusement, le test inverse est pire (en elo)
    inline int get_square(int square, int king_square) {
        return SQ::file(king_square) > FILE_D ? SQ::mirrorHorizontally(square) : square;
    }

    std::pair<Usize, Usize> get_indices(Piece piece, int square, int wking, int bking);
    template <Color side>Usize get_indice(Piece piece, int square, int king);


    constexpr inline I32 screlu(I16 x) {
        auto clamped = std::clamp(static_cast<I32>(x), 0, QA);
        return clamped * clamped;
    }

};

#endif // NNUE_H
