#ifndef SIMD_H
#define SIMD_H

/*  AVX2
 *      __m256i = 256-bit vector containing integers = 32 octets = 16 * I16
 *
 *
 *
 *
 * _mm256_load_si256  : Moves integer values from aligned memory location to a destination vector.
 * _mm256_loadu_si256 : Moves integer values from unaligned memory location to a destination vector.
 *
 */

    // Code provenant de Integral

#if defined USE_SIMD
#include <immintrin.h>


namespace simd {

//--------------------------------------------------------------------------------- AVX512
#if defined __AVX512__

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

#endif

} // namespace simd

#endif
#endif // SIMD_H
