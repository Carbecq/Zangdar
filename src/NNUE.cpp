#include "NNUE.h"
#include <bit>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include "types.h"
#include "bitmask.h"
#include "simd.h"
#include "Move.h"
#include "Board.h"

/*   INCBIN est une macro (de la bibliothèque incbin) qui insère le contenu brut du fichier EVALFILE
 *   directement dans une section de données de l'exécutable, au moment de la compilation.
 *   À partir du nom network qu'on lui donne, la macro génère trois symboles :
 *      - gnetworkData  : pointeur (const unsigned char*) vers le premier octet du fichier embarqué, aligné en mémoire ;
 *      - gnetworkEnd   : pointeur juste après le dernier octet ;
 *      - gnetworkSize  : la taille en octets.
 *
 *  Après le passage du préprocesseur, il existe donc trois identifiants distincts dans ce fichier :
 *      - Network       : le type, la struct définie dans NNUE.h ;
 *      - gnetworkxxx   : les 3 symboles générés par la macro ;
 *      - network       : pointeur sur le réseau
 *
 *  Tout est dans un namespace anonyme : network est privé à NNUE.cpp.
 */

namespace {

#include "incbin/incbin.h"

//  incbin aligne les octets embarqués sur l'ISA détectée (16 en SSE2, 32 en AVX2),
//  alors que Network est alignas(64) : sans cela le reinterpret_cast ci-dessous
//  porterait sur une adresse sous-alignée. L'index 6 vaut 64 octets et corrige à la
//  fois l'attribut C et la directive assembleur. Halogen procède de même.
#undef  INCBIN_ALIGNMENT_INDEX
#define INCBIN_ALIGNMENT_INDEX 6

INCBIN(network, EVALFILE);
static_assert(INCBIN_ALIGNMENT >= alignof(Network),
              "octets embarqués moins alignés que Network : le cast serait invalide");
const Network *file_network = reinterpret_cast<const Network *>(gnetworkData);

// Couches L1-L3 réordonnées par NNUE::init, partagées en lecture seule par tous les threads
RuntimeNetwork runtime_network;
const RuntimeNetwork *network = &runtime_network;

// Le fichier embarqué doit faire exactement la taille du struct (pas de padding :
// toutes les sections précédentes sont des multiples de 64 octets)
static_assert(sizeof(Network) ==
              (KING_BUCKETS_COUNT * INPUT_LAYER_SIZE * HIDDEN_LAYER_SIZE + HIDDEN_LAYER_SIZE) * sizeof(I16)
            + OUTPUT_BUCKETS * L2_SIZE * FT_OUT_SIZE * sizeof(I08)
            + (OUTPUT_BUCKETS * (L2_SIZE + L3_SIZE * EFFECTIVE_L2_SIZE + 2 * L3_SIZE + 1)) * sizeof(float));

//-----------------------------------------------------------------------
//  Table NNZ (« non nul ») : pour chaque octet de masque, la position de ses bits à 1.
//  Sert à lister les blocs d'entrées L1 non nuls (cf. Alexandria).
struct NNZEntry {
    U16 indices[8];
    int count;
};
std::array<NNZEntry, 256> nnz_table;

#if defined NNUE_DENSE_SIMD

//=========================================================================
//! \brief  Neurone FT (ordre du fichier) rangé en position pos par la
//! conversion 16 → 8 bits (instruction packus).
//! Cette instruction travaille par moitiés de 128 bits, ce qui permute les
//! blocs de 8 octets : AVX2 {0,2,1,3}, AVX-512 {0,4,1,5,2,6,3,7}.
//-------------------------------------------------------------------------
constexpr size_t packus_source(size_t pos)
{
    constexpr size_t group = 2 * sizeof(simd::Vepi16) / sizeof(I16);   // octets produits par une conversion
    constexpr size_t nb    = group / 8;

    const size_t base  = pos - pos % group;
    const size_t block = (pos % group) / 8;
    const size_t src   = (block % 2 == 0) ? block / 2 : nb / 2 + block / 2;

    return base + src * 8 + pos % 8;
}

#endif

} // namespace

//======================================================
//! \brief  Copie les couches L1 à L3 du fichier embarqué, réordonnées pour le
//! calcul SIMD ; le FT est lu directement dans le fichier.
//! À appeler UNE seule fois, avant la création des threads.
//------------------------------------------------------
void NNUE::init()
{
    // Le fichier bullet = struct Network + padding final ("bullet" répété)
    // jusqu'au multiple de 64 octets. Un fichier PLUS PETIT = mauvais réseau
    // (mauvaise archi / mauvaises tailles) → refus net.
    assert(gnetworkSize >= sizeof(Network));

    // Table NNZ
    for (int mask = 0; mask < 256; ++mask)
    {
        nnz_table[mask].count = 0;
        for (int bit = 0; bit < 8; ++bit)
            if (mask & (1 << bit))
                nnz_table[mask].indices[nnz_table[mask].count++] = bit;
    }

    RuntimeNetwork& rt = runtime_network;

    // FT : lu sur place dans le fichier embarqué (pages partagées entre processus)

#if defined NNUE_DENSE_SIMD
    // L1 : [bucket][sortie j][entrée i] → [bucket][bloc de 4 entrées][sortie j][4],
    // pour lire d'un coup les poids des 16 sorties d'un bloc.
    // Les entrées suivent l'ordre permuté de la sortie FT (cf. packus_source).
    for (size_t bucket = 0; bucket < OUTPUT_BUCKETS; ++bucket)
    {
        const size_t offset = bucket * L2_SIZE * FT_OUT_SIZE;

        for (size_t chunk = 0; chunk < FT_OUT_SIZE / 4; ++chunk)
            for (size_t j = 0; j < L2_SIZE; ++j)
                for (size_t k = 0; k < 4; ++k)
                    rt.l1_weights[offset + (chunk * L2_SIZE + j) * 4 + k]
                        = file_network->l1_weights[offset + j * FT_OUT_SIZE + packus_source(chunk * 4 + k)];
    }
#else
    // Chemin scalaire : layout fichier inchangé [bucket][sortie][entrée]
    std::memcpy(rt.l1_weights.data(), file_network->l1_weights.data(), sizeof(rt.l1_weights));
#endif

    std::memcpy(rt.l1_biases.data(), file_network->l1_biases.data(), sizeof(rt.l1_biases));

    // L2 : [bucket][sortie o][entrée i] → [bucket][entrée i][sortie o],
    // pour multiplier chaque entrée par les poids de toutes les sorties d'un coup
    for (size_t bucket = 0; bucket < OUTPUT_BUCKETS; ++bucket)
        for (size_t o = 0; o < L3_SIZE; ++o)
            for (size_t i = 0; i < EFFECTIVE_L2_SIZE; ++i)
                rt.l2_weights[bucket * EFFECTIVE_L2_SIZE * L3_SIZE + i * L3_SIZE + o]
                    = file_network->l2_weights[bucket * L3_SIZE * EFFECTIVE_L2_SIZE + o * EFFECTIVE_L2_SIZE + i];

    std::memcpy(rt.l2_biases.data(),  file_network->l2_biases.data(),  sizeof(rt.l2_biases));
    std::memcpy(rt.l3_weights.data(), file_network->l3_weights.data(), sizeof(rt.l3_weights));
    std::memcpy(rt.l3_biases.data(),  file_network->l3_biases.data(),  sizeof(rt.l3_biases));
}

//=========================================================================
//  Pipeline forward multi-couches
//=========================================================================

namespace {

#if defined NNUE_DENSE_SIMD

constexpr size_t kChunk16 = sizeof(simd::Vepi16) / sizeof(I16);
constexpr size_t kChunk32 = sizeof(simd::Vepi32) / sizeof(I32);
constexpr size_t kChunkPs = sizeof(simd::Vps)    / sizeof(float);

//=========================================================================
//! \brief  Sortie FT d'une perspective : produit, neurone à neurone, des deux
//! moitiés de l'accumulateur, ramené sur 8 bits → HL/2 octets.
//! (a × b) >> FT_SHIFT se calcule par mulhi(a << (16 − FT_SHIFT), b) : mulhi
//! garde les 16 bits hauts du produit (cf. Stockfish, Alexandria).
//! La conversion en octets (packus) ramène les produits négatifs à 0 et
//! permute l'ordre des neurones ; les poids L1 en tiennent compte.
//! \param[in]  acc   accumulateur de la perspective (HL I16)
//! \param[out] out   HL/2 octets
//-------------------------------------------------------------------------
void ft_activation(const std::array<I16, HIDDEN_LAYER_SIZE>& acc, U08* out)
{
    const auto ft_max = simd::SetEpi16(QA);

    for (size_t i = 0; i < HIDDEN_LAYER_SIZE / 2; i += 2 * kChunk16)
    {
        const auto in0a = simd::LoadEpi16(&acc[i]);
        const auto in0b = simd::LoadEpi16(&acc[i + kChunk16]);
        const auto in1a = simd::LoadEpi16(&acc[i + HIDDEN_LAYER_SIZE / 2]);
        const auto in1b = simd::LoadEpi16(&acc[i + HIDDEN_LAYER_SIZE / 2 + kChunk16]);

        const auto c0a = simd::Clip(in0a, QA);
        const auto c0b = simd::Clip(in0b, QA);
        const auto c1a = simd::MinEpi16(in1a, ft_max);
        const auto c1b = simd::MinEpi16(in1b, ft_max);

        const auto pa = simd::MulhiEpi16(simd::SlliEpi16(c0a, 16 - FT_SHIFT), c1a);
        const auto pb = simd::MulhiEpi16(simd::SlliEpi16(c0b, 16 - FT_SHIFT), c1b);

        simd::StoreEpi8(&out[i], simd::PackusEpi16(pa, pb));
    }
}

//=========================================================================
//! \brief  Liste les blocs de 4 octets non nuls de la sortie FT, les seuls que
//! la L1 calcule (la sortie FT compte en général 70 à 90 % de zéros).
//! \param[in]  in       sortie FT (FT_OUT_SIZE octets)
//! \param[out] indices  numéros des blocs non nuls
//! \return Nombre d'indices non nuls
//-------------------------------------------------------------------------
int find_nnz(const U08* in, U16* indices)
{
    int count = 0;

    const __m128i increment = _mm_set1_epi16(8);
    __m128i base = _mm_setzero_si128();

    constexpr size_t stride    = sizeof(simd::Vepi8);       // 32 (AVX2) / 64 (AVX-512)
    constexpr int    nb_chunks = int(stride / sizeof(I32)); // 8 / 16

    static_assert(FT_OUT_SIZE % (2 * stride) == 0);

    // Deux vecteurs par tour, un seul masque (16 bits AVX2, 32 bits AVX-512)
    for (size_t i = 0; i < FT_OUT_SIZE; i += 2 * stride)
    {
        const U32 mask = simd::NnzMask(simd::LoadEpi8(&in[i]))
                       | simd::NnzMask(simd::LoadEpi8(&in[i + stride])) << nb_chunks;

        for (int j = 0; j < 2 * nb_chunks; j += 8)
        {
            const U32 slice = (mask >> j) & 0xFF;

            const __m128i offsets = _mm_loadu_si128(reinterpret_cast<const __m128i*>(nnz_table[slice].indices));
            _mm_storeu_si128(reinterpret_cast<__m128i*>(&indices[count]), _mm_add_epi16(base, offsets));

            // popcount plutôt que le compte de la table : ne dépend que du masque
            count += std::popcount(slice);
            base = _mm_add_epi16(base, increment);
        }
    }

    return count;
}

//=========================================================================
//! \brief  L1 (poids int8) sur les seuls blocs non nuls, puis double activation.
//! Chaque bloc de 4 entrées est multiplié par les poids des 16 sorties
//! (Dpbusd : produits octet × int8 sommés par 4), deux blocs à la fois
//! (Dpbusdx2) pour économiser une instruction sans VNNI.
//! Passage en float + biais → z, puis out[j] = clamp(z, 0, 1) et
//! out[j + 16] = min(z², 1) : le carré garde l'information des z négatifs.
//! \param[in]  in         sortie FT (FT_OUT_SIZE octets)
//! \param[in]  nnz        numéros des blocs non nuls
//! \param[in]  nnz_count  nombre de blocs non nuls
//! \param[in]  bucket     output bucket
//! \param[out] out        EFFECTIVE_L2_SIZE float
//-------------------------------------------------------------------------
void propagate_l1(const U08* in, const U16* nnz, int nnz_count, int bucket, float* out)
{
    simd::Vepi32 sums[L2_SIZE / kChunk32];
    for (auto& s : sums)
        s = simd::ZeroEpi32();

    const I32* in32     = reinterpret_cast<const I32*>(in);
    const I08* weights  = &network->l1_weights[size_t(bucket) * L2_SIZE * FT_OUT_SIZE];

    int i = 0;

#if defined(__AVX512VNNI__)
    // VNNI : dpbusd attend le résultat du précédent (~5 cycles) ; quatre chaînes
    // indépendantes, additionnées à la fin (cf. Stockfish)
    constexpr int nb_chains = 4;
    simd::Vepi32 chains[nb_chains][L2_SIZE / kChunk32];
    for (auto& chain : chains)
        for (auto& s : chain)
            s = simd::ZeroEpi32();

    for (; i + nb_chains - 1 < nnz_count; i += nb_chains)
    {
        for (int c = 0; c < nb_chains; ++c)
        {
            const auto input = simd::BroadcastEpi32(in32[nnz[i + c]]);
            const I08* w = &weights[size_t(nnz[i + c]) * 4 * L2_SIZE];

            for (size_t j = 0; j < L2_SIZE / kChunk32; ++j)
                chains[c][j] = simd::DpbusdEpi32(chains[c][j], input, simd::LoadEpi8(&w[j * sizeof(simd::Vepi8)]));
        }
    }

    for (size_t j = 0; j < L2_SIZE / kChunk32; ++j)
        sums[j] = simd::AddEpi32(simd::AddEpi32(chains[0][j], chains[1][j]),
                                 simd::AddEpi32(chains[2][j], chains[3][j]));
#else
    for (; i + 1 < nnz_count; i += 2)
    {
        const auto input_a = simd::BroadcastEpi32(in32[nnz[i]]);
        const auto input_b = simd::BroadcastEpi32(in32[nnz[i + 1]]);
        const I08* wa = &weights[size_t(nnz[i])     * 4 * L2_SIZE];
        const I08* wb = &weights[size_t(nnz[i + 1]) * 4 * L2_SIZE];

        for (size_t j = 0; j < L2_SIZE / kChunk32; ++j)
            sums[j] = simd::Dpbusdx2Epi32(sums[j],
                                          input_a, simd::LoadEpi8(&wa[j * sizeof(simd::Vepi8)]),
                                          input_b, simd::LoadEpi8(&wb[j * sizeof(simd::Vepi8)]));
    }
#endif

    for (; i < nnz_count; ++i)
    {
        const auto input = simd::BroadcastEpi32(in32[nnz[i]]);
        const I08* w = &weights[size_t(nnz[i]) * 4 * L2_SIZE];

        for (size_t j = 0; j < L2_SIZE / kChunk32; ++j)
            sums[j] = simd::DpbusdEpi32(sums[j], input, simd::LoadEpi8(&w[j * sizeof(simd::Vepi8)]));
    }

    const auto dequant = simd::SetPs(L1_MUL);
    const auto zero    = simd::ZeroPs();
    const auto one     = simd::SetPs(1.0f);
    const float* biases = &network->l1_biases[size_t(bucket) * L2_SIZE];

    for (size_t j = 0; j < L2_SIZE; j += kChunkPs)
    {
        const auto z       = simd::AddPs(simd::MulPs(simd::CvtEpi32Ps(sums[j / kChunk32]), dequant), simd::LoadPs(&biases[j]));
        const auto linear  = simd::MinPs(simd::MaxPs(z, zero), one);
        const auto squared = simd::MinPs(simd::MulPs(z, z), one);

        simd::StorePs(&out[j], linear);
        simd::StorePs(&out[L2_SIZE + j], squared);
    }
}

//=========================================================================
//! \brief  L2 (float) : 32 → 32, activation SCReLU = clamp(x, 0, 1)².
//! \param[in]  in      EFFECTIVE_L2_SIZE float (sortie de la L1)
//! \param[in]  bucket  output bucket
//! \param[out] out     L3_SIZE float
//-------------------------------------------------------------------------
void propagate_l2(const float* in, int bucket, float* out)
{
    simd::Vps acc[L3_SIZE / kChunkPs];

    const float* biases  = &network->l2_biases[size_t(bucket) * L3_SIZE];
    const float* weights = &network->l2_weights[size_t(bucket) * EFFECTIVE_L2_SIZE * L3_SIZE];

    for (size_t j = 0; j < L3_SIZE / kChunkPs; ++j)
        acc[j] = simd::LoadPs(&biases[j * kChunkPs]);

    for (size_t i = 0; i < EFFECTIVE_L2_SIZE; ++i)
    {
        const auto input = simd::SetPs(in[i]);

        for (size_t j = 0; j < L3_SIZE / kChunkPs; ++j)
            acc[j] = simd::AddPs(acc[j], simd::MulPs(input, simd::LoadPs(&weights[i * L3_SIZE + j * kChunkPs])));
    }

    const auto zero = simd::ZeroPs();
    const auto one  = simd::SetPs(1.0f);

    for (size_t j = 0; j < L3_SIZE / kChunkPs; ++j)
    {
        const auto clipped = simd::MinPs(simd::MaxPs(acc[j], zero), one);
        simd::StorePs(&out[j * kChunkPs], simd::MulPs(clipped, clipped));
    }
}

#else

//=========================================================================
//  Version scalaire (sans SIMD, ou SSE2 seul). Poids dans l'ordre du fichier.
//  Mêmes calculs, dans le même ordre, que la version SIMD : bench identique
//  sur toutes les cibles.
//=========================================================================

// Mêmes résultats que MinPs / MaxPs (SIMD), y compris pour -0
inline float min1(float x)   { return x < 1.0f ? x : 1.0f; }
inline float clip01(float x) { return min1(x > 0.0f ? x : 0.0f); }

void ft_activation(const std::array<I16, HIDDEN_LAYER_SIZE>& acc, U08* out)
{
    for (size_t i = 0; i < HIDDEN_LAYER_SIZE / 2; ++i)
    {
        const I32 c0 = std::clamp(static_cast<I32>(acc[i]), 0, QA);
        const I32 c1 = std::min(static_cast<I32>(acc[i + HIDDEN_LAYER_SIZE / 2]), QA);

        // >> arithmétique (C++20) : un produit négatif reste négatif, puis est écrêté à 0
        const I32 product = (c0 * c1) >> FT_SHIFT;
        out[i] = static_cast<U08>(std::clamp(product, 0, 255));
    }
}

void propagate_l1(const U08* in, int bucket, float* out)
{
    I32 sums[L2_SIZE] = {};
    const I08* weights = &network->l1_weights[size_t(bucket) * L2_SIZE * FT_OUT_SIZE];

    for (size_t j = 0; j < L2_SIZE; ++j)
        for (size_t i = 0; i < FT_OUT_SIZE; ++i)
            sums[j] += static_cast<I32>(in[i]) * weights[j * FT_OUT_SIZE + i];

    const float* biases = &network->l1_biases[size_t(bucket) * L2_SIZE];

    for (size_t j = 0; j < L2_SIZE; ++j)
    {
        const float z = static_cast<float>(sums[j]) * L1_MUL + biases[j];
        out[j]           = clip01(z);
        out[j + L2_SIZE] = min1(z * z);
    }
}

void propagate_l2(const float* in, int bucket, float* out)
{
    float sums[L3_SIZE];

    const float* biases  = &network->l2_biases[size_t(bucket) * L3_SIZE];
    const float* weights = &network->l2_weights[size_t(bucket) * EFFECTIVE_L2_SIZE * L3_SIZE];

    for (size_t o = 0; o < L3_SIZE; ++o)
        sums[o] = biases[o];

    for (size_t i = 0; i < EFFECTIVE_L2_SIZE; ++i)
        for (size_t o = 0; o < L3_SIZE; ++o)
            sums[o] = sums[o] + in[i] * weights[i * L3_SIZE + o];

    for (size_t o = 0; o < L3_SIZE; ++o)
    {
        const float clipped = clip01(sums[o]);
        out[o] = clipped * clipped;
    }
}

#endif

//=========================================================================
//! \brief  L3 (float) : 32 → 1, sommé dans un ordre fixe (8 sommes partielles
//! puis deux à deux) : même résultat sur toutes les cibles.
//! \param[in] in      L3_SIZE float
//! \param[in] bucket  output bucket
//! \return Sortie brute du réseau (avant SCALE)
//-------------------------------------------------------------------------
float propagate_l3(const float* in, int bucket)
{
    const float* weights = &network->l3_weights[size_t(bucket) * L3_SIZE];

    float sums[8];
    for (size_t l = 0; l < 8; ++l)
        sums[l] = in[l] * weights[l];

    for (size_t i = 8; i < L3_SIZE; ++i)
        sums[i % 8] = sums[i % 8] + in[i] * weights[i];

    const float a = (sums[0] + sums[4]) + (sums[2] + sums[6]);
    const float b = (sums[1] + sums[5]) + (sums[3] + sums[7]);

    return (a + b) + network->l3_biases[bucket];
}

} // namespace

//======================================================
//! \brief  Vérifie la taille du réseau embarqué (cf. NNUE.h)
//!
//! On compare à NETWORK_DATA_SIZE et non à sizeof(Network) : l'alignas ajoute un
//! bourrage de queue que le fichier n'a pas. Seul un bourrage de QUEUE est toléré,
//! celui que l'export a pu ajouter.
//! Tout écart plus grand signale une autre architecture.
//------------------------------------------------------
void verify_network()
{
    if (gnetworkSize < NETWORK_DATA_SIZE || gnetworkSize - NETWORK_DATA_SIZE >= ALIGN_MAX)
    {
        std::cerr << "Erreur : le réseau embarqué (" << EVALFILE << ") ne correspond pas à "
                     "l'architecture compilée." << std::endl;
        std::cerr << "  attendu : " << NETWORK_DATA_SIZE << " octets de données"
                     " (plus un bourrage de queue < " << ALIGN_MAX << ")" << std::endl;
        std::cerr << "  trouvé  : " << gnetworkSize << " octets" << std::endl;
        std::exit(EXIT_FAILURE);
    }
}

//======================================================
//! \brief  Retourne l'évaluation du réseau
//!
//! \param[in] current  accumulateur courant (2 perspectives)
//! \param[in] count    nombre de pièces restantes (pour l'output bucket)
//!
//! \return Score en centipions du point de vue de "color"
//------------------------------------------------------
template<Color color>
int NNUE::evaluate(const Accumulator& current, size_t count)
{
    const int bucket = get_bucket(count);

    alignas(ALIGN) U08   ft_out[FT_OUT_SIZE];
    alignas(ALIGN) float l1_out[EFFECTIVE_L2_SIZE];
    alignas(ALIGN) float l2_out[L3_SIZE];

    // Perspective du camp au trait d'abord (ordre du concat à l'entraînement)
    if constexpr (color == Color::WHITE)
    {
        ft_activation(current.white, &ft_out[0]);
        ft_activation(current.black, &ft_out[FT_OUT_SIZE / 2]);
    }
    else
    {
        ft_activation(current.black, &ft_out[0]);
        ft_activation(current.white, &ft_out[FT_OUT_SIZE / 2]);
    }

#if defined NNUE_DENSE_SIMD
    // +8 : find_nnz stocke par paquets de 8 même pour le dernier indice
    alignas(ALIGN) U16 nnz[FT_OUT_SIZE / 4 + 8];
    const int nnz_count = find_nnz(ft_out, nnz);

    propagate_l1(ft_out, nnz, nnz_count, bucket, l1_out);
#else
    propagate_l1(ft_out, bucket, l1_out);
#endif

    propagate_l2(l1_out, bucket, l2_out);

    return static_cast<int>(propagate_l3(l2_out, bucket) * float(SCALE));
}

//====================================================
//! \brief  Initialise NNUE pour une nouvelle recherche
//!
//! \param[in] board    position de départ de la recherche
//----------------------------------------------------
void NNUE::start_search(const Board& board)
{
    head_idx = 0;

    for (int i = 0; i < 2; i++)
        for (int j = 0; j < KING_BUCKETS_COUNT; j++)
            finny[i][j].init();

    Accumulator& head = stack[0];

    init_accumulator(head);

    const SQUARE wking = board.king_square[WHITE];
    const SQUARE bking = board.king_square[BLACK];
    SQUARE square;

    Bitboard occupied = board.occupancy_all();
    while (occupied)
    {
        square = BB::pop_lsb(occupied);
        add(head, board.piece_square[square], square, wking, bking);
    }

    head.updated[WHITE] = true;
    head.updated[BLACK] = true;

    head.king_square = board.king_square;
}

//====================================================
//! \brief  Rebase le stack NNUE :
//!         1) copie l'accumulateur courant en position 0 et remet head_idx à 0.
//!         2) Réinitialise les finny tables (elles référencent des positions
//!             de stack qui ne sont plus valides).
//!         3) N'est utilisé que par DataGen pour éviter l'overflow du stack
//!             car start_search() n'est appelé qu'une fois par partie.
//!
//! \param[in] board    position courante (pour appliquer les lazy updates en attente)
//!
//----------------------------------------------------
void NNUE::rebase(const Board& board)
{
    if (head_idx > 0)
    {
        // Assurer que l'accumulateur courant est à jour
        Accumulator& current = stack[head_idx];
        lazy_updates(board, current);

        stack[0] = current;
        head_idx = 0;
    }

    // Réinitialiser les finny tables car elles peuvent contenir
    // des bitboards obsolètes après le rebase
    for (int i = 0; i < 2; i++)
        for (int j = 0; j < KING_BUCKETS_COUNT; j++)
            finny[i][j].init();
}

//====================================================
//! \brief  Ajoute 1 nouvel accumulateur
//----------------------------------------------------
void NNUE::push()
{
    assert(head_idx < stack.size()-1);  // < 128
    head_idx++;
}

//====================================================
//! \brief  Supprime 1 accumulateur
//----------------------------------------------------
void NNUE::pop()
{
    assert(head_idx > 0);
    head_idx--;
}

//====================================================
//! \brief  Re-initalise toutes les perspectives
//!
//! \param[out] acc   accumulateur à réinitialiser (biais du réseau)
//----------------------------------------------------
void NNUE::init_accumulator(Accumulator& acc)
{
    acc.init_biases(file_network->feature_biases);
}

//====================================================
//! \brief  Re-initalise les tables Finny
//----------------------------------------------------
void FinnyEntry::init()
{
    typePiecesBB.fill({});
    colorPiecesBB.fill({});
    accumulator.init_biases(file_network->feature_biases);
}

//========================================================================
//! \brief  Ajout d'une feature (variante non-SIMD, les 2 perspectives à la fois)
//!
//! \param[in,out] accu     accumulateur mis à jour (white et black)
//! \param[in]     piece    pièce ajoutée
//! \param[in]     from     case de cette pièce
//! \param[in]     wking    case du roi blanc
//! \param[in]     bking    case du roi noir
//------------------------------------------------------------------------
void NNUE::add(Accumulator& accu, Piece piece, SQUARE from, SQUARE wking, SQUARE bking)
{
    const auto [white_idx, black_idx] = get_indices(piece, from, wking, bking);

    // Non-SIMD volontairement : appelé une seule fois par pièce au début
    // de la recherche (start_search), pas dans la boucle critique.
    for (size_t i = 0; i < HIDDEN_LAYER_SIZE; ++i)
        accu.white[i] += file_network->feature_weights[white_idx * HIDDEN_LAYER_SIZE + i];

    for (size_t i = 0; i < HIDDEN_LAYER_SIZE; ++i)
        accu.black[i] += file_network->feature_weights[black_idx * HIDDEN_LAYER_SIZE + i];
}


//============================================================
//! \brief  Utilisation de l'optimisation Lazy Updates
//! https://www.chessprogramming.org/NNUE#Lazy_Updates
//!
//! \param[in]     board    position courante
//! \param[in,out] acc      accumulateur à mettre à jour (2 perspectives)
//------------------------------------------------------------
void NNUE::lazy_updates(const Board& board, Accumulator& acc)
{
    lazy_update<WHITE>(board, acc);
    lazy_update<BLACK>(board, acc);
}

//============================================================
//! \brief  Met à jour paresseusement une perspective de l'accumulateur courant
//!
//! Remonte le stack à la recherche du dernier accumulateur à jour pour
//! cette perspective. Si un changement de king bucket ou de flanc est
//! détecté en chemin, déclenche un rafraîchissement complet (Finny table)
//! au lieu d'accumuler les updates incrémentales.
//!
//! \param[in]     board    position courante (pour le rafraîchissement complet)
//! \param[in,out] acc      accumulateur courant (sommet du stack) à mettre à jour
//------------------------------------------------------------
template <Color side>
void NNUE::lazy_update(const Board &board, Accumulator& acc)
{
    if (acc.updated[side])
        return;

    assert(head_idx > 0);

    const SQUARE king = acc.king_square[side];

    for (int iter = int(head_idx) - 1; iter >= 0; --iter)
    {
        // Recherche d'un accumulateur à raffraichir
        if (NNUE::need_refresh<side>(stack[iter].king_square[side], king) == true)
        {
            // Comme le cout d'un raffraichissement est important
            // autant le faire sur le dernier accumulateur : acc
            refresh_accumulator<side>(board, acc);
            break;
        }

        // On a trouvé le dernier accumulateur updaté
        if (stack[iter].updated[side] == true)
        {
            size_t last_updated = iter;

            // on va remonter les accumulateurs;
            // et les updater un par un
            while (last_updated != head_idx)
            {
                update<side>(stack[last_updated], stack[last_updated+1], king);
                last_updated++;
            }
            break;
        }
    }
}

//======================================================
//! \brief  Faut-il raffraichir l'accumulateur ?
//!
//! \param[in] old_king     ancienne case du roi de cette perspective
//! \param[in] new_king     nouvelle case du roi de cette perspective
//!
//! \return true si le changement de flanc (mirroring) ou de king bucket
//!         impose un rafraîchissement complet plutôt qu'une update incrémentale
//------------------------------------------------------
template <Color side>
bool NNUE::need_refresh(SQUARE old_king, SQUARE new_king)
{
    assert(SQ::is_ok(old_king));
    assert(SQ::is_ok(new_king));

    // 1) Horizontal Mirroring
    //      est-ce qu'on a changé de côté ?
    if ((old_king & 0b100) != (new_king & 0b100))
        return true;

    // 2) King Bucket
    //      est-ce qu'on a change de bucket ?
    return king_buckets_map[get_relative_square<side>(old_king)] != king_buckets_map[get_relative_square<side>(new_king)];
}

//===================================================================
//! \brief  Prise en compte des modifications apportées à la perspective
//!
//! Répartit vers sub_add / sub_sub_add / sub_sub_add_add selon le type
//! de coup enregistré dans dst.dirtyPieces (NORMAL / CAPTURE / CASTLING).
//!
//! \param[in]  src     accumulateur source (précédent, à jour)
//! \param[out] dst     accumulateur destination (contient les dirtyPieces du coup)
//! \param[in]  king    case du roi de cette perspective
//--------------------------------------------------------------------
template <Color side>
void NNUE::update(const Accumulator& src, Accumulator& dst, SQUARE king)
{
    const DirtyPieces& dp = dst.dirtyPieces;

    if (dp.type == DirtyPieces::CASTLING)
    {
        sub_sub_add_add<side>(src, dst,
                              dp.sub_1.piece, dp.sub_1.square,
                              dp.sub_2.piece, dp.sub_2.square,
                              dp.add_1.piece, dp.add_1.square,
                              dp.add_2.piece, dp.add_2.square,
                              king);
    }
    else if (dp.type == DirtyPieces::CAPTURE)
    {
        sub_sub_add<side>(src, dst,
                          dp.sub_1.piece, dp.sub_1.square,
                          dp.sub_2.piece, dp.sub_2.square,
                          dp.add_1.piece, dp.add_1.square,
                          king);
    }
    else
    {
        sub_add<side>(src, dst,
                      dp.sub_1.piece, dp.sub_1.square,
                      dp.add_1.piece, dp.add_1.square,
                      king);
    }

    dst.updated[side] = true;
}

//========================================================================
//! \brief  Calcule l'indice du triplet (couleur, piece, case)
//! dans l'Input Layer
//!
//! \param[in] piece    pièce concernée
//! \param[in] square   case de cette pièce
//! \param[in] wking    case du roi blanc
//! \param[in] bking    case du roi noir
//!
//! \return Paire (indice perspective blanche, indice perspective noire)
//------------------------------------------------------------------------
std::pair<size_t, size_t> NNUE::get_indices(Piece piece, SQUARE square, SQUARE wking, SQUARE bking)
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

    assert(piece  != Piece::PIECE_NONE);
    assert(square != Square::SQUARE_NONE);

    constexpr U32 color_stride = N_SQUARES * 6;
    constexpr U32 piece_stride = N_SQUARES;

    const auto base  = Move::type(piece);
    const auto color = Move::color(piece);

    //TODO : refaire le codage des pièces ?
    //  moi  : PAWN = 1
    //  NNUE : PAWN = 0
    const auto base_nnue = base - 1;

    const auto white_indice = king_buckets_map[wking] * INPUT_LAYER_SIZE
            + color * color_stride
            + base_nnue * piece_stride
            + get_square(square, wking);


    const auto black_indice = king_buckets_map[SQ::mirrorVertically(bking)] * INPUT_LAYER_SIZE
            + !color * color_stride
            + base_nnue * piece_stride
            + SQ::mirrorVertically(get_square(square, bking));

    return {white_indice, black_indice};
}

//========================================================================
//! \brief  Calcule l'indice du triplet (couleur, piece, case)
//!         pour une perspective
//! \tparam     side     perspective
//! \param[in] piece    pièce concernée
//! \param[in] square   case de cette pièce
//! \param[in] king     case du roi de cette perspective
//!
//! \return Indice dans l'Input Layer (feature_weights)
//------------------------------------------------------------------------
template <Color side>
U32 NNUE::get_indice(Piece piece, SQUARE square, SQUARE king)
{
    assert(piece  != Piece::PIECE_NONE);
    assert(square != Square::SQUARE_NONE);

    constexpr U32 color_stride = N_SQUARES * 6;
    constexpr U32 piece_stride = N_SQUARES;

    const auto base  = Move::type(piece);
    const auto color = Move::color(piece);

    const auto base_nnue = base - 1;

    if (side == WHITE)
    {
        return king_buckets_map[king] * INPUT_LAYER_SIZE
                + color * color_stride
                + base_nnue * piece_stride
                + get_square(square, king);
    }
    else
    {
        return king_buckets_map[SQ::mirrorVertically(king)] * INPUT_LAYER_SIZE
                + !color * color_stride
                + base_nnue * piece_stride
                + SQ::mirrorVertically(get_square(square, king));
    }
}

//=================================================================================
//! \brief  Update combiné : Ajout + Suppression
//! \param[in]  src         accumulateur source (précédent, à jour)
//! \param[out] dst         accumulateur destination
//! \param[in]  sub_piece   pièce supprimée de from
//! \param[in]  sub         case de la pièce supprimée
//! \param[in]  add_piece   pièce ajoutée en dest
//! \param[in]  add         case de la pièce ajoutée
//! \param[in]  king        position du roi de couleur "US"
//---------------------------------------------------------------------------------
template <Color side>
void NNUE::sub_add(const Accumulator& src, Accumulator& dst,
                   Piece sub_piece, SQUARE sub,
                   Piece add_piece, SQUARE add,
                   SQUARE king)
{
    // Optimisations de : https://cosmo.tardis.ac/files/2024-06-01-nnue.html

    const auto sub_idx = get_indice<side>(sub_piece, sub, king);
    const auto add_idx = get_indice<side>(add_piece, add, king);

#if defined USE_SIMD
    constexpr int simd_width = sizeof(simd::Vepi16) / sizeof(I16);

    for (size_t i = 0; i < HIDDEN_LAYER_SIZE; i += simd_width)
    {
        if constexpr (side == WHITE)
        {
            auto cur_w  = simd::LoadEpi16(&src.white[i]);
            //TODO sub_add ou add_sub ??
            cur_w       = simd::SubEpi16(cur_w, simd::LoadEpi16(&file_network->feature_weights[sub_idx * HIDDEN_LAYER_SIZE + i]));
            cur_w       = simd::AddEpi16(cur_w, simd::LoadEpi16(&file_network->feature_weights[add_idx * HIDDEN_LAYER_SIZE + i]));
            simd::StoreEpi16(&dst.white[i], cur_w);
        }
        else
        {
            auto cur_w  = simd::LoadEpi16(&src.black[i]);
            //TODO sub_add ou add_sub ??
            cur_w       = simd::SubEpi16(cur_w, simd::LoadEpi16(&file_network->feature_weights[sub_idx * HIDDEN_LAYER_SIZE + i]));
            cur_w       = simd::AddEpi16(cur_w, simd::LoadEpi16(&file_network->feature_weights[add_idx * HIDDEN_LAYER_SIZE + i]));
            simd::StoreEpi16(&dst.black[i], cur_w);
        }
    }
#else
    if constexpr (side == WHITE)
    {
        for (size_t i = 0; i < HIDDEN_LAYER_SIZE; ++i)
        {
            dst.white[i] = src.white[i]
                    + file_network->feature_weights[add_idx * HIDDEN_LAYER_SIZE + i]
                    - file_network->feature_weights[sub_idx * HIDDEN_LAYER_SIZE + i];
        }
    }
    else
    {
        for (size_t i = 0; i < HIDDEN_LAYER_SIZE; ++i)
        {
            dst.black[i] = src.black[i]
                    + file_network->feature_weights[add_idx * HIDDEN_LAYER_SIZE + i]
                    - file_network->feature_weights[sub_idx * HIDDEN_LAYER_SIZE + i];
        }
    }
#endif
}


//=================================================================================
//! \brief  Update combiné : Ajout + Suppression + Suppression
//! La pièce supprimée en dest appartient au camp ennemi
//! Cette routine est utilisée pour la prise en passant
//! \param[in]  src             accumulateur source (précédent, à jour)
//! \param[out] dst             accumulateur destination
//! \param[in]  sub_piece_1     pièce supprimée de from
//! \param[in]  sub_1           case de la pièce supprimée de from
//! \param[in]  sub_piece_2     pièce ennemie supprimée de sub
//! \param[in]  sub_2           case de la pièce ennemie supprimée
//! \param[in]  add_piece_1     pièce ajoutée en dest
//! \param[in]  add_1           case de la pièce ajoutée
//! \param[in]  king            position du roi de couleur "US"
//!
//---------------------------------------------------------------------------------
template <Color side>
void NNUE::sub_sub_add(const Accumulator& src, Accumulator& dst,
                       Piece sub_piece_1, SQUARE sub_1,
                       Piece sub_piece_2, SQUARE sub_2,
                       Piece add_piece_1, SQUARE add_1,
                       SQUARE king)
{
    const auto sub1_idx = get_indice<side>(sub_piece_1, sub_1, king);
    const auto sub2_idx = get_indice<side>(sub_piece_2, sub_2, king);
    const auto add1_idx = get_indice<side>(add_piece_1, add_1, king);

#if defined USE_SIMD
    constexpr int simd_width = sizeof(simd::Vepi16) / sizeof(I16);

    if constexpr (side == WHITE)
    {
        for (size_t i = 0; i < HIDDEN_LAYER_SIZE; i += simd_width)
        {
            auto cur_w = simd::LoadEpi16(&src.white[i]);
            cur_w      = simd::AddEpi16(cur_w, simd::LoadEpi16(&file_network->feature_weights[add1_idx * HIDDEN_LAYER_SIZE + i]));
            cur_w      = simd::SubEpi16(cur_w, simd::LoadEpi16(&file_network->feature_weights[sub1_idx * HIDDEN_LAYER_SIZE + i]));
            cur_w      = simd::SubEpi16(cur_w, simd::LoadEpi16(&file_network->feature_weights[sub2_idx * HIDDEN_LAYER_SIZE + i]));
            simd::StoreEpi16(&dst.white[i], cur_w);
        }
    }
    else
    {
        for (size_t i = 0; i < HIDDEN_LAYER_SIZE; i += simd_width)
        {
            auto cur_w = simd::LoadEpi16(&src.black[i]);
            cur_w      = simd::AddEpi16(cur_w, simd::LoadEpi16(&file_network->feature_weights[add1_idx * HIDDEN_LAYER_SIZE + i]));
            cur_w      = simd::SubEpi16(cur_w, simd::LoadEpi16(&file_network->feature_weights[sub1_idx * HIDDEN_LAYER_SIZE + i]));
            cur_w      = simd::SubEpi16(cur_w, simd::LoadEpi16(&file_network->feature_weights[sub2_idx * HIDDEN_LAYER_SIZE + i]));
            simd::StoreEpi16(&dst.black[i], cur_w);
        }
    }
#else
    if constexpr (side == WHITE)
    {
        for (size_t i = 0; i < HIDDEN_LAYER_SIZE; ++i)
        {
            dst.white[i] = src.white[i]
                    + file_network->feature_weights[add1_idx * HIDDEN_LAYER_SIZE + i]
                    - file_network->feature_weights[sub1_idx * HIDDEN_LAYER_SIZE + i]
                    - file_network->feature_weights[sub2_idx * HIDDEN_LAYER_SIZE + i];
        }
    }
    else
    {
        for (size_t i = 0; i < HIDDEN_LAYER_SIZE; ++i)
        {
            dst.black[i] = src.black[i]
                    + file_network->feature_weights[add1_idx * HIDDEN_LAYER_SIZE + i]
                    - file_network->feature_weights[sub1_idx * HIDDEN_LAYER_SIZE + i]
                    - file_network->feature_weights[sub2_idx * HIDDEN_LAYER_SIZE + i];
        }
    }
#endif
}

//=================================================================================
//! \brief  Update combiné : Ajout + Ajout + Suppression + Suppression
//! Utilisé pour le roque (tour et roi bougent tous les deux)
//! \param[in]  src             accumulateur source (précédent, à jour)
//! \param[out] dst             accumulateur destination
//! \param[in]  sub_piece_1     1ère pièce supprimée (position de départ)
//! \param[in]  sub_1           case de la 1ère pièce supprimée
//! \param[in]  sub_piece_2     2ème pièce supprimée (position de départ)
//! \param[in]  sub_2           case de la 2ème pièce supprimée
//! \param[in]  add_piece_1     1ère pièce ajoutée (position d'arrivée)
//! \param[in]  add_1           case de la 1ère pièce ajoutée
//! \param[in]  add_piece_2     2ème pièce ajoutée (position d'arrivée)
//! \param[in]  add_2           case de la 2ème pièce ajoutée
//! \param[in]  king            position du roi de couleur "US"
//---------------------------------------------------------------------------------
template <Color side>
void NNUE::sub_sub_add_add(const Accumulator& src, Accumulator& dst,
                           Piece sub_piece_1, SQUARE sub_1,
                           Piece sub_piece_2, SQUARE sub_2,
                           Piece add_piece_1, SQUARE add_1,
                           Piece add_piece_2, SQUARE add_2,
                           SQUARE king)
{
    const auto sub1_idx = get_indice<side>(sub_piece_1, sub_1, king);
    const auto sub2_idx = get_indice<side>(sub_piece_2, sub_2, king);
    const auto add1_idx = get_indice<side>(add_piece_1, add_1, king);
    const auto add2_idx = get_indice<side>(add_piece_2, add_2, king);

#if defined USE_SIMD
    constexpr int simd_width = sizeof(simd::Vepi16) / sizeof(I16);

    if constexpr (side == WHITE)
    {
        for (size_t i = 0; i < HIDDEN_LAYER_SIZE; i += simd_width)
        {
            auto cur_w = simd::LoadEpi16(&src.white[i]);
            cur_w      = simd::AddEpi16(cur_w, simd::LoadEpi16(&file_network->feature_weights[add1_idx * HIDDEN_LAYER_SIZE + i]));
            cur_w      = simd::AddEpi16(cur_w, simd::LoadEpi16(&file_network->feature_weights[add2_idx * HIDDEN_LAYER_SIZE + i]));
            cur_w      = simd::SubEpi16(cur_w, simd::LoadEpi16(&file_network->feature_weights[sub1_idx * HIDDEN_LAYER_SIZE + i]));
            cur_w      = simd::SubEpi16(cur_w, simd::LoadEpi16(&file_network->feature_weights[sub2_idx * HIDDEN_LAYER_SIZE + i]));
            simd::StoreEpi16(&dst.white[i], cur_w);
        }
    }
    else
    {
        for (size_t i = 0; i < HIDDEN_LAYER_SIZE; i += simd_width)
        {
            auto cur_w = simd::LoadEpi16(&src.black[i]);
            cur_w      = simd::AddEpi16(cur_w, simd::LoadEpi16(&file_network->feature_weights[add1_idx * HIDDEN_LAYER_SIZE + i]));
            cur_w      = simd::AddEpi16(cur_w, simd::LoadEpi16(&file_network->feature_weights[add2_idx * HIDDEN_LAYER_SIZE + i]));
            cur_w      = simd::SubEpi16(cur_w, simd::LoadEpi16(&file_network->feature_weights[sub1_idx * HIDDEN_LAYER_SIZE + i]));
            cur_w      = simd::SubEpi16(cur_w, simd::LoadEpi16(&file_network->feature_weights[sub2_idx * HIDDEN_LAYER_SIZE + i]));
            simd::StoreEpi16(&dst.black[i], cur_w);
        }
    }
#else
    if constexpr (side == WHITE)
    {
        for (size_t i = 0; i < HIDDEN_LAYER_SIZE; ++i)
        {
            dst.white[i] = src.white[i]
                    + file_network->feature_weights[add1_idx * HIDDEN_LAYER_SIZE + i]
                    + file_network->feature_weights[add2_idx * HIDDEN_LAYER_SIZE + i]
                    - file_network->feature_weights[sub1_idx * HIDDEN_LAYER_SIZE + i]
                    - file_network->feature_weights[sub2_idx * HIDDEN_LAYER_SIZE + i];
        }
    }
    else
    {
        for (size_t i = 0; i < HIDDEN_LAYER_SIZE; ++i)
        {
            dst.black[i] = src.black[i]
                    + file_network->feature_weights[add1_idx * HIDDEN_LAYER_SIZE + i]
                    + file_network->feature_weights[add2_idx * HIDDEN_LAYER_SIZE + i]
                    - file_network->feature_weights[sub1_idx * HIDDEN_LAYER_SIZE + i]
                    - file_network->feature_weights[sub2_idx * HIDDEN_LAYER_SIZE + i];
        }

    }
#endif
}

//==========================================================================
//! \brief  Raffaichissement de l'accumulateur
//! De façon à minimiser le travail, on utilise les finny tables.
//! Une seule passe : chaque tranche de l'entrée reste en registres pendant
//! tous les retraits et ajouts, puis va dans l'entrée et dans l'accumulateur.
//! \param[in]  board   position courante
//! \param[out] acc     accumulateur de cette perspective à rafraîchir
//--------------------------------------------------------------------------
template <Color side>
void NNUE::refresh_accumulator(const Board &board, Accumulator& acc)
{
    const SQUARE king_square = board.get_king_square<side>();
    const int    king_bucket = king_buckets_map[get_relative_square<side>(king_square)];
    const int    mirrored    = SQ::file(king_square) > FILE_D;

    FinnyEntry& entry = finny[mirrored][king_bucket];

    // Lignes du FT à retirer de l'entrée et à y ajouter
    std::array<const I16*, 32> subs;
    std::array<const I16*, 32> adds;
    int nb_subs = 0;
    int nb_adds = 0;

    for (Color color : {Color::WHITE, Color::BLACK})
    {
        for (PieceType piece : {PieceType::PAWN, PieceType::KNIGHT, PieceType::BISHOP, PieceType::ROOK,
             PieceType::QUEEN, PieceType::KING })
        {
            const Bitboard old_pieces = entry.colorPiecesBB[side][color] & entry.typePiecesBB[side][piece];
            const Bitboard new_pieces = board.colorPiecesBB[color]       & board.typePiecesBB[piece];

            Bitboard to_remove = ~new_pieces & old_pieces;
            while (to_remove)
            {
                const SQUARE square = BB::pop_lsb(to_remove);
                subs[nb_subs++] = &file_network->feature_weights[get_indice<side>(Move::make_piece(color, piece), square, king_square) * HIDDEN_LAYER_SIZE];
            }

            Bitboard to_add = new_pieces & ~old_pieces;
            while (to_add)
            {
                const SQUARE square = BB::pop_lsb(to_add);
                adds[nb_adds++] = &file_network->feature_weights[get_indice<side>(Move::make_piece(color, piece), square, king_square) * HIDDEN_LAYER_SIZE];
            }
        }
    }

    entry.colorPiecesBB[side] = board.colorPiecesBB;
    entry.typePiecesBB[side]  = board.typePiecesBB;

    auto& cached = (side == WHITE) ? entry.accumulator.white : entry.accumulator.black;
    auto& target = (side == WHITE) ? acc.white : acc.black;

#if defined USE_SIMD
    constexpr size_t simd_width = sizeof(simd::Vepi16) / sizeof(I16);
    constexpr size_t nb_regs    = 8;                                    // 4, 8 et 12 : même vitesse
    constexpr size_t tile       = nb_regs * simd_width;
    static_assert(HIDDEN_LAYER_SIZE % tile == 0);

    for (size_t i = 0; i < HIDDEN_LAYER_SIZE; i += tile)
    {
        simd::Vepi16 regs[nb_regs];

        for (size_t r = 0; r < nb_regs; ++r)
            regs[r] = simd::LoadEpi16(&cached[i + r * simd_width]);

        for (int k = 0; k < nb_subs; ++k)
            for (size_t r = 0; r < nb_regs; ++r)
                regs[r] = simd::SubEpi16(regs[r], simd::LoadEpi16(&subs[k][i + r * simd_width]));

        for (int k = 0; k < nb_adds; ++k)
            for (size_t r = 0; r < nb_regs; ++r)
                regs[r] = simd::AddEpi16(regs[r], simd::LoadEpi16(&adds[k][i + r * simd_width]));

        for (size_t r = 0; r < nb_regs; ++r)
        {
            simd::StoreEpi16(&cached[i + r * simd_width], regs[r]);
            simd::StoreEpi16(&target[i + r * simd_width], regs[r]);
        }
    }
#else
    for (size_t i = 0; i < HIDDEN_LAYER_SIZE; ++i)
    {
        I16 value = cached[i];

        for (int k = 0; k < nb_subs; ++k)
            value = static_cast<I16>(value - subs[k][i]);
        for (int k = 0; k < nb_adds; ++k)
            value = static_cast<I16>(value + adds[k][i]);

        cached[i] = value;
        target[i] = value;
    }
#endif

    acc.updated[side] = true;
}

template int NNUE::evaluate<WHITE>(const Accumulator& current, size_t count);
template int NNUE::evaluate<BLACK>(const Accumulator& current, size_t count);



