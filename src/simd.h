#ifndef SIMD_H
#define SIMD_H

/*  Wrappers SIMD portables (AVX-512 / AVX2 / SSE2) pour l'inférence NNUE.
 *
 *  Types vectoriels :
 *      Vepi16 : vecteur d'entiers I16 (16 bits signés)
 *      Vepi32 : vecteur d'entiers I32 (32 bits signés)
 *
 *  Taille d'un vecteur selon l'ISA :
 *      AVX-512 : 512 bits = 32 × I16  (kChunkSize = 32)
 *      AVX2    : 256 bits = 16 × I16  (kChunkSize = 16)
 *      SSE2    : 128 bits =  8 × I16  (kChunkSize =  8)
 *
 *  Opération clé pour SCReLU — MultiplyAddEpi16 (madd_epi16) :
 *      Traite les vecteurs par paires d'I16 et produit des I32 :
 *      out[i] = v1[2i] × v2[2i] + v1[2i+1] × v2[2i+1]
 *      Utilisé dans activation() : madd(product, clipped) = clipped² × weight → I32
 *      → évite l'overflow I16 et accumule directement en I32
 *
 *  _mm256_load_si256  : chargement depuis mémoire alignée (ALIGN=32 en AVX2)
 *  _mm256_loadu_si256 : chargement depuis mémoire non alignée (plus lent)
 */

#if defined USE_SIMD
#include <immintrin.h>


namespace simd {

//--------------------------------------------------------------------------------- AVX512
// __AVX512F__ donne les instructions de base (entiers 32/64 bits),
// mais les opérations sur epi16 (16 bits) comme _mm512_add_epi16, _mm512_mullo_epi16, _mm512_min_epi16, etc.
// nécessitent AVX-512BW (Byte and Word).

#if defined(__AVX512F__) && defined(__AVX512BW__)

using Vepi16 = __m512i;
using Vepi32 = __m512i;

inline Vepi16 ZeroEpi16() {
    return _mm512_setzero_si512();
}
inline Vepi32 ZeroEpi32() {
    return _mm512_setzero_si512();
}

inline Vepi16 LoadEpi16(const int16_t* memory_address) {
    return _mm512_load_si512(reinterpret_cast<const __m512i*>(memory_address));
}

inline Vepi32 LoadEpi32(const int32_t* memory_address) {
    return _mm512_load_si512(reinterpret_cast<const __m512i*>(memory_address));
}

inline void StoreEpi16(void* memory_address, Vepi16 vector) {
    _mm512_store_si512(memory_address, vector);
}

inline Vepi16 SetEpi16(int num) {
    return _mm512_set1_epi16(num);
}
inline Vepi32 SetEpi32(int num) {
    return _mm512_set1_epi32(num);
}

inline Vepi16 AddEpi16(Vepi16 v1, Vepi16 v2) {
    return _mm512_add_epi16(v1, v2);
}

inline Vepi16 SubEpi16(Vepi16 v1, Vepi16 v2) {
    return _mm512_sub_epi16(v1, v2);
}

inline Vepi32 AddEpi32(Vepi32 v1, Vepi32 v2) {
    return _mm512_add_epi32(v1, v2);
}

inline Vepi16 MultiplyEpi16(Vepi16 v1, Vepi16 v2) {
    return _mm512_mullo_epi16(v1, v2);
}

inline Vepi32 MultiplyAddEpi16(Vepi16 v1, Vepi16 v2) {
    return _mm512_madd_epi16(v1, v2);
}

inline Vepi16 Clip(Vepi16 vector, int l1q) {
    return _mm512_min_epi16(_mm512_max_epi16(vector, ZeroEpi16()), SetEpi16(l1q));
}

inline int ReduceAddEpi32(Vepi32 v) {
    return _mm512_reduce_add_epi32(v);
}

//--------------------------------------------------------------------------------- AVX2
#elif defined __AVX2__

using Vepi16 = __m256i;
using Vepi32 = __m256i;

inline Vepi16 ZeroEpi16() {
    return _mm256_setzero_si256();
}

inline Vepi32 ZeroEpi32() {
    return _mm256_setzero_si256();
}

inline Vepi16 LoadEpi16(const int16_t* memory_address) {
    return _mm256_load_si256(reinterpret_cast<const __m256i*>(memory_address));
}

inline Vepi32 LoadEpi32(const int32_t* memory_address) {
    return _mm256_load_si256(reinterpret_cast<const __m256i*>(memory_address));
}

inline void StoreEpi16(void* memory_address, Vepi16 vector) {
    _mm256_store_si256(reinterpret_cast<__m256i*>(memory_address), vector);
}

inline Vepi16 SetEpi16(int num) {
    return _mm256_set1_epi16(num);
}

inline Vepi32 SetEpi32(int num) {
    return _mm256_set1_epi32(num);
}

inline Vepi16 AddEpi16(Vepi16 v1, Vepi16 v2) {
    return _mm256_add_epi16(v1, v2);
}

inline Vepi32 AddEpi32(Vepi32 v1, Vepi32 v2) {
    return _mm256_add_epi32(v1, v2);
}

inline Vepi16 SubEpi16(Vepi16 v1, Vepi16 v2) {
    return _mm256_sub_epi16(v1, v2);
}

inline Vepi16 MultiplyEpi16(Vepi16 v1, Vepi16 v2) {
    return _mm256_mullo_epi16(v1, v2);
}

inline Vepi32 MultiplyAddEpi16(Vepi16 v1, Vepi16 v2) {
    return _mm256_madd_epi16(v1, v2);
}

inline Vepi16 Clip(Vepi16 vector, int l1q) {
    return _mm256_min_epi16(_mm256_max_epi16(vector, ZeroEpi16()), SetEpi16(l1q));
}


// Implementation from Alexandria
inline int ReduceAddEpi32(Vepi32 vector) {
    // Split the __m256i into 2 __m128i vectors, and add them together
    __m128i lower128 = _mm256_castsi256_si128(vector);
    __m128i upper128 = _mm256_extracti128_si256(vector, 1);
    __m128i sum128 = _mm_add_epi32(lower128, upper128);

    // Store the high 64 bits of sum128 in the low 64 bits of this vector,
    // then add them together (only the lowest 64 bits are relevant)
    __m128i upper64 = _mm_unpackhi_epi64(sum128, sum128);
    __m128i sum64 = _mm_add_epi32(upper64, sum128);

    // Store the high 32 bits of the relevant part of sum64 in the low 32 bits of
    // the vector, and then add them together (only the lowest 32 bits are
    // relevant)
    __m128i upper32 = _mm_shuffle_epi32(sum64, 1);
    __m128i sum32 = _mm_add_epi32(upper32, sum64);

    // Return the bottom 32 bits of sum32
    return _mm_cvtsi128_si32(sum32);
}

//--------------------------------------------------------------------------------- SSE2
#elif defined __SSE2__

using Vepi16 = __m128i;
using Vepi32 = __m128i;

inline Vepi16 ZeroEpi16() {
    return _mm_setzero_si128();
}

inline Vepi32 ZeroEpi32() {
    return _mm_setzero_si128();
}

inline Vepi16 LoadEpi16(const int16_t* memory_address) {
    return _mm_load_si128(reinterpret_cast<const __m128i*>(memory_address));
}

inline Vepi32 LoadEpi32(const int32_t* memory_address) {
    return _mm_load_si128(reinterpret_cast<const __m128i*>(memory_address));
}

inline void StoreEpi16(void* memory_address, Vepi16 vector) {
    _mm_store_si128(reinterpret_cast<__m128i*>(memory_address), vector);
}

inline Vepi16 SetEpi16(int num) {
    return _mm_set1_epi16(num);
}

inline Vepi32 SetEpi32(int num) {
    return _mm_set1_epi32(num);
}

inline Vepi16 AddEpi16(Vepi16 v1, Vepi16 v2) {
    return _mm_add_epi16(v1, v2);
}

inline Vepi32 AddEpi32(Vepi32 v1, Vepi32 v2) {
    return _mm_add_epi32(v1, v2);
}

inline Vepi16 SubEpi16(Vepi16 v1, Vepi16 v2) {
    return _mm_sub_epi16(v1, v2);
}

inline Vepi16 MultiplyEpi16(Vepi16 v1, Vepi16 v2) {
    return _mm_mullo_epi16(v1, v2);
}

inline Vepi32 MultiplyAddEpi16(Vepi16 v1, Vepi16 v2) {
    return _mm_madd_epi16(v1, v2);
}

inline Vepi16 Clip(Vepi16 vector, int l1q) {
    // SSE2 n'a pas min/max epi16, mais SSE2 a _mm_min_epi16 (signé)
    // _mm_min_epi16 et _mm_max_epi16 existent en SSE2
    return _mm_min_epi16(_mm_max_epi16(vector, ZeroEpi16()), SetEpi16(l1q));
}

inline int ReduceAddEpi32(Vepi32 vector) {
    // 128 bits → réduction horizontale
    __m128i upper64 = _mm_unpackhi_epi64(vector, vector);
    __m128i sum64 = _mm_add_epi32(upper64, vector);
    __m128i upper32 = _mm_shuffle_epi32(sum64, 1);
    __m128i sum32 = _mm_add_epi32(upper32, sum64);
    return _mm_cvtsi128_si32(sum32);
}

#endif

} // namespace simd

#endif
#endif // SIMD_H
