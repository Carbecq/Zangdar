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
//  Description du réseau : (768×8kb → 1024)×2 → 1×8ob  SCReLU
//
//      Entrées              : 768 = 2 couleurs × 6 pièces × 64 cases
//      Input Buckets        : 8 (king buckets, avec horizontal mirroring)
//      Accumulateur         : 1024 neurones par perspective (incrémental)
//      Output Buckets       : 8 (MaterialCount : selon le nb de pièces restantes)
//      Activation           : SCReLU = clamp(x, 0, QA)² / QA  (Squared Clipped ReLU)
//      Factoriser           : poids partagés entre buckets (l0f dans Bullet), mergés au save
//
//  Pipeline d'évaluation :
//    1. Accumulateur (incrémental) : acc[white/black] ← biases + Σ feature_weights
//    2. SCReLU SIMD : clipped = clamp(acc, 0, QA) ; output = clipped × clipped (I16 safe)
//    3. produit matrice-vecteur (activations × output_weights) : sum = Σ(output × output_weights[bucket]) via madd_epi16 → I32
//    4. Mise à l'échelle : eval = (sum/QA + bias) × SCALE / QAB

// https://www.chessprogramming.org/NNUE#Output_Buckets

constexpr size_t INPUT_LAYER_SIZE  = N_COLORS * 6 * N_SQUARES;   // = 768
constexpr size_t HIDDEN_LAYER_SIZE = 1024;                        // neurones par perspective
constexpr size_t OUTPUT_BUCKETS    = 8;
constexpr size_t BUCKET_DIVISOR    = (32 + OUTPUT_BUCKETS - 1) / OUTPUT_BUCKETS;   // = 4

constexpr I32 SCALE = 400;
constexpr I32 QA    = 255;      // facteur de quantification L0 (accumulateur)
constexpr I32 QB    =  64;      // facteur de quantification output weights
constexpr I32 QAB   = QA * QB;  // = 16320 : scale du biais de sortie

// King buckets : chaque position du roi détermine quel jeu de poids utiliser.
// La symétrie horizontale est gérée par get_square() (mirroring du fichier),
// donc le layout est symétrique autour du centre (fichiers D/E identiques).
// Vue du blanc (rank 1 = rangée du roi blanc) :
//   rank 1 : coins (a/h) et flancs (b/g) = 0-3 (4 zones fines près de la base)
//   rank 2 : flancs = 4, centre = 5
//   rank 3-4 : tout = 6 (milieu de terrain)
//   rank 5-8 : tout = 7 (terrain ennemi, roi avancé)
constexpr std::array<int, N_SQUARES> king_buckets_map = {
    0, 1, 2, 3, 3, 2, 1, 0,   // rank 1
    4, 4, 5, 5, 5, 5, 4, 4,   // rank 2
    6, 6, 6, 6, 6, 6, 6, 6,   // rank 3
    6, 6, 6, 6, 6, 6, 6, 6,   // rank 4
    7, 7, 7, 7, 7, 7, 7, 7,   // rank 5
    7, 7, 7, 7, 7, 7, 7, 7,   // rank 6
    7, 7, 7, 7, 7, 7, 7, 7,   // rank 7
    7, 7, 7, 7, 7, 7, 7, 7,   // rank 8
};
constexpr int KING_BUCKETS_COUNT = *std::ranges::max_element(king_buckets_map) + 1;

//-----------------------------------------------------------------------
//  Layout mémoire du réseau (chargé depuis le fichier .bin via incbin)
struct Network {
    // feature_weights[bucket][feature][neurone] — layout Bullet "l0w" (après merge factoriser)
    // feature ∈ [0, INPUT_LAYER_SIZE), neurone ∈ [0, HIDDEN_LAYER_SIZE)
    alignas(ALIGN) std::array<I16, KING_BUCKETS_COUNT * INPUT_LAYER_SIZE * HIDDEN_LAYER_SIZE> feature_weights;
    alignas(ALIGN) std::array<I16, HIDDEN_LAYER_SIZE>                                         feature_biases;

    // output_weights layout Bullet "transposed" : [OUTPUT_BUCKETS][N_COLORS * HIDDEN_LAYER_SIZE]
    // → adresse = bucket * N_COLORS * HIDDEN_LAYER_SIZE + color * HIDDEN_LAYER_SIZE + i
    // Bullet génère cette disposition depuis la v1.x (flag .transpose() dans save_format)
    alignas(ALIGN) std::array<I16, N_COLORS * HIDDEN_LAYER_SIZE * OUTPUT_BUCKETS> output_weights;
    alignas(ALIGN) std::array<I16, OUTPUT_BUCKETS>                                output_bias;  // à l'échelle QAB
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
    Accumulator() {}

    alignas(ALIGN) std::array<I16, HIDDEN_LAYER_SIZE> white;
    alignas(ALIGN) std::array<I16, HIDDEN_LAYER_SIZE> black;

    // Lazy Updates
    bool updated[N_COLORS]      = {false, false};
    DirtyPieces dirtyPieces     = DirtyPieces{};
    std::array<SQUARE, N_COLORS> king_square = {SQUARE_NONE, SQUARE_NONE};     // position des rois

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

    // Output bucket selon le nb de pièces restantes (MaterialCount).
    // count ∈ [2, 32] → bucket ∈ [0, 7]  (BUCKET_DIVISOR = 4)
    // count=2 → bucket=0 (finale), count=32 → bucket≤7 (partie pleine)
    inline int get_bucket(size_t count) const {
        assert(count >= 2);
        return (count - 2) / BUCKET_DIVISOR;
    }

     //! \brief Retourne la case relative à la perspective
    template <Color side>
    inline SQUARE get_relative_square(SQUARE square) const {
        if constexpr (side == WHITE)
            return square;
        else
            return SQ::mirrorVertically(square);
    }

    // Horizontal mirroring : si le roi est sur les fichiers E-H (bit 2 = 1),
    // on miroire horizontalement toutes les cases pour ramener le roi côté A-D.
    // Cela permet de partager les poids entre les deux flancs (symétrie du roi).
    // Note : le test inverse (miroir si A-D) est empiriquement moins bon en Elo.
    inline SQUARE get_square(SQUARE square, SQUARE king_square) const {
        return king_square & 0b100 ? SQ::mirrorHorizontally(square) : square;
    }

    std::pair<size_t, size_t> get_indices(Piece piece, SQUARE square, SQUARE wking, SQUARE bking);
    template <Color side>U32 get_indice(Piece piece, SQUARE square, SQUARE king);


    // SCReLU scalaire (fallback non-SIMD) : clamp(x, 0, QA)²
    // Le résultat est à l'échelle QA² → divisé par QA dans activation()
    inline I32 screlu(I16 x) {
        auto clamped = std::clamp(static_cast<I32>(x), 0, QA);
        return clamped * clamped;
    }

};

#endif // NNUE_H
