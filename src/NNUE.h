#ifndef NNUE_H
#define NNUE_H

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include "types.h"
#include "bitmask.h"

#if defined __AVX2__
#include <immintrin.h>
#endif

//  Alignement des poids et des accumulateurs : une ligne de cache, sur toutes les architectures.
#define ALIGN 64

//  Padding maximal toléré dans le FICHIER réseau lors de la vérification
constexpr size_t ALIGN_MAX = 64;


//------------------------------------------------------------------------------
//  Le multi-couches en bref
//
//  Mono-couche : l'accumulateur (des centaines de neurones, chacun sensible à un motif)
//  est lu par une seule somme pondérée, qui donne l'éval. Une somme ne sait qu'additionner :
//  « roi exposé » −50 et « dame adverse active » −30 donnent −80, jamais −300 ensemble.
//
//  Multi-couches : entre l'accumulateur et la sortie, deux petites couches (16 puis 32
//  neurones) combinent les motifs. Chaque neurone ne garde que le positif, donc ne réagit
//  que si plusieurs signaux sont présents ensemble. À taille égale, le réseau est plus fin.
//
//  Le coût : l'accumulateur se met à jour coup par coup, pas les couches suivantes, qui
//  relisent tout à chaque évaluation. D'où leur petite taille, et trois astuces :
//    - produit par paires : neurone i × neurone i + HL/2, déjà un « et », moitié moins à lire ;
//    - on saute les zéros : la L1 ne lit que les produits non nuls, par blocs de 4 (find_nnz) ;
//    - 8 jeux de couches selon le matériel restant (buckets de sortie), un seul calculé.
//
//------------------------------------------------------------------------------
//  Description du réseau MULTI-COUCHES :
//      (768×16 buckets de roi → 768)×2, produit par paires → L1 16 (double activation → 32) → L2 32 → 1, ×8 buckets de sortie
//
//      Entrées              : 768 = 2 couleurs × 6 pièces × 64 cases
//      Buckets de roi       : 16, avec symétrie horizontale
//      Accumulateur (FT)    : 768 neurones par perspective, mis à jour coup par coup
//                             FT (« feature transformer ») = cette première couche, des pièces vers l'accumulateur
//      Buckets de sortie    : 8, selon le nombre de pièces restantes
//      Factoriser           : poids communs à tous les buckets à l'entraînement, ajoutés à chacun à l'export
//
//  Calcul de l'évaluation (comme Alexandria 9 / Astra 7) :
//    1. Accumulateur (I16) : acc[blanc/noir] = biais + Σ poids des pièces présentes
//    2. Sortie FT, produit par paires : out[i] = (clamp(acc[i], 0, QA) × min(acc[i + HL/2], QA)) >> FT_SHIFT, négatif → 0
//       → HL/2 octets par perspective, camp au trait d'abord = HL octets
//    3. L1 (poids int8) : z = Σ out[i] × l1w[i] × L1_MUL + l1b, sur les seules entrées non nulles
//       Double activation : clamp(z, 0, 1) et min(z², 1) → 32 float
//    4. L2 (float) : SCReLU(in × l2w + l2b) → 32 float, SCReLU(x) = clamp(x, 0, 1)²
//    5. L3 (float) : in · l3w + l3b → éval = résultat × SCALE
//
//  Quantification : FT ×QA (I16), L1 ×QB (I8, poids bornés à ±1,98 à l'entraînement), L2/L3 en float.
//  FT_SHIFT = 10 borne les octets à 63, si bien que l'addition de deux paires de produits
//  dans Dpbusdx2 (maddubs) ne déborde pas d'un I16 : 127 × 63 × 4 = 32004 ≤ 32767.

// https://www.chessprogramming.org/NNUE#Output_Buckets

constexpr size_t INPUT_LAYER_SIZE  = N_COLORS * 6 * N_SQUARES;   // = 768
constexpr size_t HIDDEN_LAYER_SIZE = 768;                         // neurones par perspective (FT)
constexpr size_t FT_OUT_SIZE       = HIDDEN_LAYER_SIZE;           // sortie FT totale : (HL/2 par perspective) × 2
constexpr size_t L2_SIZE           = 16;                          // largeur physique de L1
constexpr size_t EFFECTIVE_L2_SIZE = 2 * L2_SIZE;                 // = 32 après dual activation
constexpr size_t L3_SIZE           = 32;
constexpr size_t OUTPUT_BUCKETS    = 8;
constexpr size_t BUCKET_DIVISOR    = (32 + OUTPUT_BUCKETS - 1) / OUTPUT_BUCKETS;   // = 4

constexpr I32 SCALE    = 400;
constexpr I32 QA       = 255;   // facteur de quantification FT (accumulateur)
constexpr I32 QB       =  64;   // facteur de quantification L1 (int8)
constexpr int FT_SHIFT = 10;    // décalage du produit par paires (résultat ≤ 63)

// Déquantification de la sortie L1 : sum est à l'échelle (QA²>>FT_SHIFT)×QB
constexpr float L1_MUL = float(1 << FT_SHIFT) / float(QA * QA * QB);

// Pas de débordement dans Dpbusdx2 sans VNNI (cf. Alexandria) ; la borne ±1,98
// des poids L1 est imposée à l'entraînement (scripts bullet zangdar_*_ml*.rs)
static_assert(127 * ((QA * QA) >> FT_SHIFT) * 4 <= 32767);

// Couches L1 à L3 en SIMD : AVX2 minimum. En dessous (sse2, sse41), seul l'accumulateur
// est en SIMD ; L1 à L3 sont scalaires, poids L1 dans l'ordre du fichier.
#if defined(USE_SIMD) && (defined(__AVX2__) || (defined(__AVX512F__) && defined(__AVX512BW__)))
  #define NNUE_DENSE_SIMD
#endif

// King buckets : chaque position du roi détermine quel jeu de poids utiliser.
//                avec symétrie horizontale.
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
//  Layout du FICHIER réseau (sortie brute du save_format Bullet, via incbin).
//  Sections dans l'ordre d'écriture du trainer (scripts bullet zangdar_*_ml*.rs) :
//  l0w, l0b (I16 ×QA) ; l1w (I8 ×QB, transposé [bucket][sortie][entrée]) ;
//  l1b, l2w, l2b, l3w, l3b (float bruts, matrices transposées = row-major [sortie][entrée])
struct Network {
    // feature_weights[bucket][feature][neurone] — layout Bullet "l0w" (après merge factoriser)
    std::array<I16, KING_BUCKETS_COUNT * INPUT_LAYER_SIZE * HIDDEN_LAYER_SIZE> feature_weights;
    std::array<I16, HIDDEN_LAYER_SIZE>                                         feature_biases;

    std::array<I08,   OUTPUT_BUCKETS * L2_SIZE * FT_OUT_SIZE>           l1_weights;   // [bucket][sortie 16][entrée FT_OUT]
    std::array<float, OUTPUT_BUCKETS * L2_SIZE>                         l1_biases;    // [bucket][16]
    std::array<float, OUTPUT_BUCKETS * L3_SIZE * EFFECTIVE_L2_SIZE>     l2_weights;   // [bucket][sortie 32][entrée 32]
    std::array<float, OUTPUT_BUCKETS * L3_SIZE>                         l2_biases;    // [bucket][32]
    std::array<float, OUTPUT_BUCKETS * L3_SIZE>                         l3_weights;   // [bucket][entrée 32]
    std::array<float, OUTPUT_BUCKETS>                                   l3_biases;    // [bucket]
};

//-----------------------------------------------------------------------
//  Couches L1 à L3 telles que le calcul les lit, remplies par NNUE::init() :
//  - le FT n'y est pas : il est lu directement dans le fichier embarqué
//  - l1_weights réordonnés [bucket][bloc de 4 entrées][sortie][4], entrées dans
//    l'ordre permuté de la sortie FT (cf. packus_source)
//  - l2_weights transposés [bucket][entrée][sortie]
//  (en scalaire — sans NNUE_DENSE_SIMD — l1 garde l'ordre du fichier)
struct RuntimeNetwork {
    alignas(ALIGN) std::array<I08,   OUTPUT_BUCKETS * L2_SIZE * FT_OUT_SIZE>       l1_weights;
    alignas(ALIGN) std::array<float, OUTPUT_BUCKETS * L2_SIZE>                     l1_biases;
    alignas(ALIGN) std::array<float, OUTPUT_BUCKETS * EFFECTIVE_L2_SIZE * L3_SIZE> l2_weights;
    alignas(ALIGN) std::array<float, OUTPUT_BUCKETS * L3_SIZE>                     l2_biases;
    alignas(ALIGN) std::array<float, OUTPUT_BUCKETS * L3_SIZE>                     l3_weights;
    alignas(ALIGN) std::array<float, OUTPUT_BUCKETS>                               l3_biases;
};

//! \brief  Taille des DONNÉES du réseau (fichier embarqué).
//! Network n'a ni alignas ni bourrage intérieur (vérifié par static_assert dans
//! NNUE.cpp) : sa taille est celle des données du fichier.
constexpr size_t NETWORK_DATA_SIZE = sizeof(Network);

//! \brief  Vérifie que le réseau embarqué a bien la taille attendue.
void verify_network();

//------------------------------------------------------------------------------
// Lazy Updates
struct SquarePiece {
    SQUARE square = Square::SQUARE_NONE;
    Piece  piece  = Piece::PIECE_NONE;
};

// Pièces du dernier coup : sub_1/add_1 = pièce jouée, sub_2 = pièce prise ou tour, add_2 = tour
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
    //! \brief  Construit un accumulateur à zéro (biais ajoutés par init_biases)
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
//  finny[miroir][bucket] ; bitboards et accumulateur rangés par perspective

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

    //! \brief  Copie et réordonne les couches L1 à L3 du fichier embarqué.
    //! À appeler UNE fois au démarrage, avant toute recherche (main).
    static void init();

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

private:
    std::array<Accumulator, MAX_PLY+1> stack;                       // pile des accumulateurs

    size_t head_idx;                                                // accumulateur utilisé (= stack_size - 1)
    FinnyEntry finny[N_COLORS][KING_BUCKETS_COUNT] = {};            // tables Finny

    template <Color side> void lazy_update(const Board& board, Accumulator& head);
    template <Color side> void update(const Accumulator &src, Accumulator& dst, SQUARE king);
    template <Color side> bool need_refresh(SQUARE oldKing, SQUARE newKing) ;

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

    //! \brief  Output bucket : (count − 2) / 4, de 0 (2 pièces) à 7 (32 pièces)
    //! \param[in] count    nombre de pièces sur l'échiquier
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

    //! \brief  Retourne la case, en miroir horizontal si le roi est en colonne e à h
    //! (convention de l'entraînement)
    //! \param[in] square       case à éventuellement mirrorer
    //! \param[in] king_square  case du roi qui détermine le miroir
    inline SQUARE get_square(SQUARE square, SQUARE king_square) const {
        return king_square & 0b100 ? SQ::mirrorHorizontally(square) : square;
    }

    std::pair<size_t, size_t> get_indices(Piece piece, SQUARE square, SQUARE wking, SQUARE bking);
    template <Color side>U32 get_indice(Piece piece, SQUARE square, SQUARE king);

};

#endif // NNUE_H
