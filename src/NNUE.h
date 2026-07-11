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

#if defined(__AVX512F__) && defined(__AVX512BW__)
  #define ALIGN   64
#elif defined(__AVX2__)
  #define ALIGN   32
#elif defined(__SSE2__) || defined(__AVX__)
  #define ALIGN   16
#elif defined(__ARM_NEON)
  #define ALIGN   16
#else
  #define ALIGN 16
#endif


//------------------------------------------------------------------------------
//  Description du réseau : (768×16kb → 1024)×2 → 1×8ob  SCReLU
//
//      Entrées              : 768 = 2 couleurs × 6 pièces × 64 cases
//      Input Buckets        : 16 (king buckets, avec horizontal mirroring)
//      Accumulateur         : 1024 neurones par perspective (incrémental)
//      Output Buckets       : 8 (MaterialCount : selon le nb de pièces restantes)
//      Activation           : SCReLU = clamp(x, 0, QA)² / QA  (Squared Clipped ReLU)

// https://www.chessprogramming.org/NNUE#Output_Buckets

constexpr size_t INPUT_LAYER_SIZE  = N_COLORS * 6 * N_SQUARES;   // = 768
constexpr size_t HIDDEN_LAYER_SIZE = 1024;                        // neurones par perspective
constexpr size_t OUTPUT_BUCKETS    = 8;
constexpr size_t BUCKET_DIVISOR    = (32 + OUTPUT_BUCKETS - 1) / OUTPUT_BUCKETS;   // = 4

constexpr I32 SCALE = 400;
constexpr I32 QA    = 255;      // facteur de quantification L0 (accumulateur)
constexpr I32 QB    =  64;      // facteur de quantification output weights
constexpr I32 QAB   = QA * QB;  // = 16320 : échelle du biais de sortie

// King buckets : chaque position du roi détermine quel jeu de poids utiliser.
//                avec symétrie horizontale.
// Map 16 buckets "standard" (Alexandria/Tarnished/Berserk/Viridithas/Stormphrax) :
constexpr std::array<int, N_SQUARES> king_buckets_map = {
     0,  1,  2,  3,  3,  2,  1,  0,   // rank 1
     4,  5,  6,  7,  7,  6,  5,  4,   // rank 2
     8,  9, 10, 11, 11, 10,  9,  8,   // rank 3
     8,  9, 10, 11, 11, 10,  9,  8,   // rank 4
    12, 12, 13, 13, 13, 13, 12, 12,   // rank 5
    12, 12, 13, 13, 13, 13, 12, 12,   // rank 6
    14, 14, 15, 15, 15, 15, 14, 14,   // rank 7
    14, 14, 15, 15, 15, 15, 14, 14,   // rank 8
};
constexpr int KING_BUCKETS_COUNT = *std::ranges::max_element(king_buckets_map) + 1;

//-----------------------------------------------------------------------
//  Architecture du réseau
struct Network {
    alignas(ALIGN) std::array<I16, KING_BUCKETS_COUNT * INPUT_LAYER_SIZE * HIDDEN_LAYER_SIZE> feature_weights;
    alignas(ALIGN) std::array<I16, HIDDEN_LAYER_SIZE>                                         feature_biases;

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
    SQUARE square = Square::SQUARE_NONE;
    Piece  piece  = Piece::PIECE_NONE;
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
    //! \brief  Construit un accumulateur non initialisé (biais ajoutés via init_biases)
    Accumulator() {}

    alignas(ALIGN) std::array<I16, HIDDEN_LAYER_SIZE> white{};
    alignas(ALIGN) std::array<I16, HIDDEN_LAYER_SIZE> black{};

    // Lazy Updates
    bool updated[N_COLORS]      = {false, false};
    DirtyPieces dirtyPieces     = DirtyPieces{};
    std::array<SQUARE, N_COLORS> king_square = {SQUARE_NONE, SQUARE_NONE};     // position des rois

    //! \brief  Initialise les 2 perspectives (white/black) avec les biais du réseau
    //! \param[in] biases   biais de la couche d'entrée (feature_biases du réseau)
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
    //! \brief  Construit NNUE avec un stack d'accumulateurs vide (head_idx = 0)
    explicit NNUE() : head_idx(0) {}
    ~NNUE() = default;

    //! \brief  Retourne l'accumulateur courant (sommet du stack), en lecture seule
    inline const Accumulator& get_accumulator() const { return stack[head_idx]; }
    //! \brief  Retourne l'accumulateur courant (sommet du stack), modifiable
    inline Accumulator& get_accumulator()             { return stack[head_idx]; }

    void start_search(const Board& board);
    void rebase(const Board& board);
    void push();
    void pop();
    void init_accumulator(Accumulator &acc);
    template <Color side> void refresh_accumulator(const Board& board, Accumulator& acc);

    template<Color color> int  evaluate(const Accumulator& current, size_t count);

    void add(Accumulator &accu,
              Piece piece, SQUARE from,
              SQUARE wking, SQUARE bking);
    void lazy_updates(const Board &board, Accumulator &acc);


    I32 activation(const std::array<I16, HIDDEN_LAYER_SIZE>& us,
                   const std::array<I16, HIDDEN_LAYER_SIZE>& them,
                   const std::array<I16, HIDDEN_LAYER_SIZE * N_COLORS * OUTPUT_BUCKETS>& weights,
                   const int bucket);

    template <Color side> void add(Accumulator& accu, Piece piece, SQUARE from, SQUARE king);

private:
    std::array<Accumulator, MAX_PLY+1> stack;                       // pile des accumulateurs

    size_t head_idx;                                                // accumulateur utilisé (= stack_size - 1)
    FinnyEntry finny[N_COLORS][KING_BUCKETS_COUNT] = {};            // tables Finny

    template <Color side> void lazy_update(const Board& board, Accumulator& head);
    template <Color side> void update(const Accumulator &src, Accumulator& dst, SQUARE king);
    template <Color side> bool need_refresh(SQUARE oldKing, SQUARE newKing) ;

    template <Color side>
    void sub(Accumulator& accu, Piece piece, SQUARE from, SQUARE king);

    template <Color side>
    void sub_add(const Accumulator& src, Accumulator& dst,
                       Piece sub_piece, SQUARE sub,
                       Piece add_piece, SQUARE add,
                       SQUARE king);

    template <Color side>
    void sub_sub_add(const Accumulator &src, Accumulator &dst,
                     Piece sub_piece_1, SQUARE sub_1,
                     Piece sub_piece_2, SQUARE sub_2,
                     Piece add_piece_1, SQUARE add_1,
                     SQUARE king);

    template <Color side>
    void sub_sub_add_add(const Accumulator &src, Accumulator &dst,
                         Piece sub_piece_1, SQUARE sub_1,
                         Piece sub_piece_2, SQUARE sub_2,
                         Piece add_piece_1, SQUARE add_1,
                         Piece add_piece_2, SQUARE add_2,
                         SQUARE king);

    //! \brief  Output bucket selon le nb de pièces restantes (MaterialCount).
    //! count ∈ [2, 32] → bucket ∈ [0, 7]  (BUCKET_DIVISOR = 4)
    //! count=2 → bucket=0 (finale), count=32 → bucket≤7 (partie pleine)
    //! \param[in] count    nombre de pièces restantes sur l'échiquier
    //! \return Indice du bucket de sortie
    inline int get_bucket(size_t count) const {
        assert(count >= 2);
        return (count - 2) / BUCKET_DIVISOR;
    }

     //! \brief Retourne la case relative à la perspective
     //! \param[in] square   case à convertir
     //! \return Case inchangée pour WHITE, miroir vertical pour BLACK
    template <Color side>
    inline SQUARE get_relative_square(SQUARE square) const {
        if constexpr (side == WHITE)
            return square;
        else
            return SQ::mirrorVertically(square);
    }

    //! \brief  Retourne la case, en tenant compte du Horizontal Mirroring
    //! \param[in] square       case à éventuellement mirrorer
    //! \param[in] king_square  case du roi qui détermine le camp du miroir
    //! \note Curieusement, le test inverse est pire (en elo)
    //      king_SQUARE & 0b100 = 0 [FILE_A ... FILE_D] , 4 [FILE_D ... FILE_H]
    inline SQUARE get_square(SQUARE square, SQUARE king_square) const {
        return king_square & 0b100 ? SQ::mirrorHorizontally(square) : square;
    }

    std::pair<size_t, size_t> get_indices(Piece piece, SQUARE square, SQUARE wking, SQUARE bking);
    template <Color side>U32 get_indice(Piece piece, SQUARE square, SQUARE king);


    //! \brief  SCReLU scalaire
    //! \param[in] x    valeur brute de l'accumulateur (I16)
    //! \return         valeur calculée (I32)
    inline I32 screlu(I16 x) {
        auto clamped = std::clamp(static_cast<I32>(x), 0, QA);
        return clamped * clamped;
    }

};

#endif // NNUE_H
