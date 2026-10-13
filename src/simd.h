#ifndef SIMD_H
#define SIMD_H

/*  Wrappers SIMD portables (AVX-512 / AVX2 / SSE2 / NEON) pour l'inférence NNUE.
 *
 *      Vepi16, Vepi32 : vecteurs d'entiers 16 et 32 bits (accumulateur)
 *      Vepi8, Vps     : octets et float, couches L1 à L3 (AVX2 et AVX-512 seulement)
 *
 *  Largeur : 512 bits en AVX-512, 256 en AVX2, 128 en SSE2 et NEON.
 *  Chargements alignés : ALIGN vaut 64 (NNUE.h), ce qui convient à toutes les ISA.
 */

#if defined USE_SIMD

#if defined(__ARM_NEON)
#include <arm_neon.h>
#else
#include <immintrin.h>
#endif


namespace simd {

//--------------------------------------------------------------------------------- AVX512
// __AVX512F__ donne les instructions de base (entiers 32/64 bits),
// mais les opérations sur epi16 (16 bits) comme _mm512_add_epi16, _mm512_mullo_epi16, _mm512_min_epi16, etc.
// nécessitent AVX-512BW (Byte and Word).

#if defined(__AVX512F__) && defined(__AVX512BW__)

using Vepi16 = __m512i;
using Vepi32 = __m512i;

//=======================================================
//! \brief  Vecteur I16 mis à zéro (512 bits)
//-------------------------------------------------------
inline Vepi16 ZeroEpi16() {
    return _mm512_setzero_si512();
}
//=======================================================
//! \brief  Vecteur I32 mis à zéro (512 bits)
//-------------------------------------------------------
inline Vepi32 ZeroEpi32() {
    return _mm512_setzero_si512();
}

//=======================================================
//! \brief  Charge 32 I16 depuis une adresse alignée sur ALIGN octets
//! \param[in] memory_address   adresse mémoire alignée
//! \return Vecteur I16 chargé
//-------------------------------------------------------
inline Vepi16 LoadEpi16(const int16_t* memory_address) {
    return _mm512_load_si512(reinterpret_cast<const __m512i*>(memory_address));
}

//=======================================================
//! \brief  Stocke un vecteur I16 à une adresse alignée sur ALIGN octets
//! \param[out] memory_address  adresse mémoire alignée de destination
//! \param[in]  vector          vecteur I16 à stocker
//-------------------------------------------------------
inline void StoreEpi16(void* memory_address, Vepi16 vector) {
    _mm512_store_si512(memory_address, vector);
}

//=======================================================
//! \brief  Construit un vecteur I16 dont les 32 éléments valent num
//! \param[in] num  valeur à répliquer
//! \return Vecteur I16 rempli de num
//-------------------------------------------------------
inline Vepi16 SetEpi16(int num) {
    return _mm512_set1_epi16(num);
}

//=======================================================
//! \brief  Addition élément par élément de 2 vecteurs I16
//! \param[in] v1  premier opérande
//! \param[in] v2  second opérande
//! \return v1 + v2 (I16)
//-------------------------------------------------------
inline Vepi16 AddEpi16(Vepi16 v1, Vepi16 v2) {
    return _mm512_add_epi16(v1, v2);
}

//=======================================================
//! \brief  Soustraction élément par élément de 2 vecteurs I16
//! \param[in] v1  minuende
//! \param[in] v2  soustrahende
//! \return v1 - v2 (I16)
//-------------------------------------------------------
inline Vepi16 SubEpi16(Vepi16 v1, Vepi16 v2) {
    return _mm512_sub_epi16(v1, v2);
}

//=======================================================
//! \brief  Addition élément par élément de 2 vecteurs I32
//! \param[in] v1  premier opérande
//! \param[in] v2  second opérande
//! \return v1 + v2 (I32)
//-------------------------------------------------------
inline Vepi32 AddEpi32(Vepi32 v1, Vepi32 v2) {
    return _mm512_add_epi32(v1, v2);
}

//=======================================================
//! \brief  Écrêtage : clamp(vector, 0, l1q) élément par élément
//! \param[in] vector   vecteur I16 à écrêter
//! \param[in] l1q      borne supérieure de l'écrêtage (QA)
//! \return Vecteur I16 écrêté entre 0 et l1q
//-------------------------------------------------------
inline Vepi16 Clip(Vepi16 vector, int l1q) {
    return _mm512_min_epi16(_mm512_max_epi16(vector, ZeroEpi16()), SetEpi16(l1q));
}

//---------------------------------------------------------------------
//  Pipeline dense multi-couches (FT pairwise → L1 int8 → L2/L3 float)
//---------------------------------------------------------------------

using Vepi8 = __m512i;
using Vps   = __m512;

//! \brief  min élément par élément de 2 vecteurs I16 (sans le max(0) de Clip)
inline Vepi16 MinEpi16(Vepi16 v1, Vepi16 v2) {
    return _mm512_min_epi16(v1, v2);
}

//! \brief  Multiplication I16 en gardant les 16 bits HAUTS du produit 32 bits (signée)
inline Vepi16 MulhiEpi16(Vepi16 v1, Vepi16 v2) {
    return _mm512_mulhi_epi16(v1, v2);
}

//! \brief  Décalage logique à gauche de chaque I16
inline Vepi16 SlliEpi16(Vepi16 v, int shift) {
    return _mm512_slli_epi16(v, shift);
}

//! \brief  Pack de 2 vecteurs I16 en un vecteur U8 avec saturation [0, 255].
//! ATTENTION : opère par tranches de 128 bits → l'ordre des octets est entrelacé,
//! compensé dans l'ordre des poids L1 (packus_source, NNUE.cpp).
inline Vepi8 PackusEpi16(Vepi16 v1, Vepi16 v2) {
    return _mm512_packus_epi16(v1, v2);
}

//! \brief  Construit un vecteur I32 dont tous les éléments valent num (broadcast 4 octets)
inline Vepi32 BroadcastEpi32(int32_t num) {
    return _mm512_set1_epi32(num);
}

//! \brief  Charge 64 U8 depuis une adresse alignée
inline Vepi8 LoadEpi8(const void* memory_address) {
    return _mm512_load_si512(memory_address);
}

//! \brief  Stocke un vecteur U8 à une adresse alignée
inline void StoreEpi8(void* memory_address, Vepi8 vector) {
    _mm512_store_si512(memory_address, vector);
}

//! \brief  Masque des I32 non nuls (1 bit par I32, 16 bits).
//! Utilise cmpgt SIGNÉ : valide car les U8 packés sont ≤ 127 (garanti par FT_SHIFT)
inline uint32_t NnzMask(Vepi8 v) {
    return _mm512_cmpgt_epi32_mask(v, _mm512_setzero_si512());
}

//! \brief  sum += dot4(u, w) par lane I32 (u = U8, w = I8).
//! VNNI : 1 instruction ; sinon fallback maddubs+madd (3 instructions).
inline Vepi32 DpbusdEpi32(Vepi32 sum, Vepi8 u, Vepi8 w) {
#if defined(__AVX512VNNI__)
    return _mm512_dpbusd_epi32(sum, u, w);
#else
    const __m512i product16 = _mm512_maddubs_epi16(u, w);
    return _mm512_add_epi32(sum, _mm512_madd_epi16(product16, _mm512_set1_epi16(1)));
#endif
}

//! \brief  Variante ×2 : sum += dot4(u0,w0) + dot4(u1,w1).
//! Sans VNNI, économise un madd en additionnant les 2 produits en I16 —
//! sûr uniquement si |produit| ≤ 32767/2 (garanti par FT_SHIFT + clipping poids L1)
inline Vepi32 Dpbusdx2Epi32(Vepi32 sum, Vepi8 u0, Vepi8 w0, Vepi8 u1, Vepi8 w1) {
#if defined(__AVX512VNNI__)
    return _mm512_dpbusd_epi32(_mm512_dpbusd_epi32(sum, u0, w0), u1, w1);
#else
    const __m512i product16a = _mm512_maddubs_epi16(u0, w0);
    const __m512i product16b = _mm512_maddubs_epi16(u1, w1);
    const __m512i product32  = _mm512_madd_epi16(_mm512_add_epi16(product16a, product16b), _mm512_set1_epi16(1));
    return _mm512_add_epi32(sum, product32);
#endif
}

//--- Vecteurs float (couches L1→L2→L3 en float après déquantification) ---

inline Vps   ZeroPs()                          { return _mm512_setzero_ps(); }
inline Vps   SetPs(float num)                  { return _mm512_set1_ps(num); }
inline Vps   LoadPs(const float* mem)          { return _mm512_load_ps(mem); }
inline void  StorePs(float* mem, Vps v)        { _mm512_store_ps(mem, v); }
inline Vps   MinPs(Vps v1, Vps v2)             { return _mm512_min_ps(v1, v2); }
inline Vps   MaxPs(Vps v1, Vps v2)             { return _mm512_max_ps(v1, v2); }
inline Vps   MulPs(Vps v1, Vps v2)             { return _mm512_mul_ps(v1, v2); }
inline Vps   AddPs(Vps v1, Vps v2)             { return _mm512_add_ps(v1, v2); }
inline Vps   CvtEpi32Ps(Vepi32 v)              { return _mm512_cvtepi32_ps(v); }

//--------------------------------------------------------------------------------- AVX2
#elif defined __AVX2__

using Vepi16 = __m256i;
using Vepi32 = __m256i;

//=======================================================
//! \brief  Vecteur I16 mis à zéro (256 bits)
//-------------------------------------------------------
inline Vepi16 ZeroEpi16() {
    return _mm256_setzero_si256();
}

//=======================================================
//! \brief  Vecteur I32 mis à zéro (256 bits)
//-------------------------------------------------------
inline Vepi32 ZeroEpi32() {
    return _mm256_setzero_si256();
}

//=======================================================
//! \brief  Charge 16 I16 depuis une adresse alignée sur ALIGN octets
//! \param[in] memory_address   adresse mémoire alignée
//! \return Vecteur I16 chargé
//-------------------------------------------------------
inline Vepi16 LoadEpi16(const int16_t* memory_address) {
    return _mm256_load_si256(reinterpret_cast<const __m256i*>(memory_address));
}

//=======================================================
//! \brief  Stocke un vecteur I16 à une adresse alignée sur ALIGN octets
//! \param[out] memory_address  adresse mémoire alignée de destination
//! \param[in]  vector          vecteur I16 à stocker
//-------------------------------------------------------
inline void StoreEpi16(void* memory_address, Vepi16 vector) {
    _mm256_store_si256(reinterpret_cast<__m256i*>(memory_address), vector);
}

//=======================================================
//! \brief  Construit un vecteur I16 dont les 16 éléments valent num
//! \param[in] num  valeur à répliquer
//! \return Vecteur I16 rempli de num
//-------------------------------------------------------
inline Vepi16 SetEpi16(int num) {
    return _mm256_set1_epi16(num);
}

//=======================================================
//! \brief  Addition élément par élément de 2 vecteurs I16
//! \param[in] v1  premier opérande
//! \param[in] v2  second opérande
//! \return v1 + v2 (I16)
//-------------------------------------------------------
inline Vepi16 AddEpi16(Vepi16 v1, Vepi16 v2) {
    return _mm256_add_epi16(v1, v2);
}

//=======================================================
//! \brief  Addition élément par élément de 2 vecteurs I32
//! \param[in] v1  premier opérande
//! \param[in] v2  second opérande
//! \return v1 + v2 (I32)
//-------------------------------------------------------
inline Vepi32 AddEpi32(Vepi32 v1, Vepi32 v2) {
    return _mm256_add_epi32(v1, v2);
}

//=======================================================
//! \brief  Soustraction élément par élément de 2 vecteurs I16
//! \param[in] v1  minuende
//! \param[in] v2  soustrahende
//! \return v1 - v2 (I16)
//-------------------------------------------------------
inline Vepi16 SubEpi16(Vepi16 v1, Vepi16 v2) {
    return _mm256_sub_epi16(v1, v2);
}

//=======================================================
//! \brief  Écrêtage : clamp(vector, 0, l1q) élément par élément
//! \param[in] vector   vecteur I16 à écrêter
//! \param[in] l1q      borne supérieure de l'écrêtage (QA)
//! \return Vecteur I16 écrêté entre 0 et l1q
//-------------------------------------------------------
inline Vepi16 Clip(Vepi16 vector, int l1q) {
    return _mm256_min_epi16(_mm256_max_epi16(vector, ZeroEpi16()), SetEpi16(l1q));
}


//---------------------------------------------------------------------
//  Pipeline dense multi-couches (FT pairwise → L1 int8 → L2/L3 float)
//---------------------------------------------------------------------

using Vepi8 = __m256i;
using Vps   = __m256;

//! \brief  min élément par élément de 2 vecteurs I16 (sans le max(0) de Clip)
inline Vepi16 MinEpi16(Vepi16 v1, Vepi16 v2) {
    return _mm256_min_epi16(v1, v2);
}

//! \brief  Multiplication I16 en gardant les 16 bits HAUTS du produit 32 bits (signée)
inline Vepi16 MulhiEpi16(Vepi16 v1, Vepi16 v2) {
    return _mm256_mulhi_epi16(v1, v2);
}

//! \brief  Décalage logique à gauche de chaque I16
inline Vepi16 SlliEpi16(Vepi16 v, int shift) {
    return _mm256_slli_epi16(v, shift);
}

//! \brief  Pack de 2 vecteurs I16 en un vecteur U8 avec saturation [0, 255].
//! ATTENTION : opère par tranches de 128 bits → l'ordre des octets est entrelacé,
//! compensé dans l'ordre des poids L1 (packus_source, NNUE.cpp).
inline Vepi8 PackusEpi16(Vepi16 v1, Vepi16 v2) {
    return _mm256_packus_epi16(v1, v2);
}

//! \brief  Construit un vecteur I32 dont tous les éléments valent num (broadcast 4 octets)
inline Vepi32 BroadcastEpi32(int32_t num) {
    return _mm256_set1_epi32(num);
}

//! \brief  Charge 32 U8 depuis une adresse alignée
inline Vepi8 LoadEpi8(const void* memory_address) {
    return _mm256_load_si256(reinterpret_cast<const __m256i*>(memory_address));
}

//! \brief  Stocke un vecteur U8 à une adresse alignée
inline void StoreEpi8(void* memory_address, Vepi8 vector) {
    _mm256_store_si256(reinterpret_cast<__m256i*>(memory_address), vector);
}

//! \brief  Masque des I32 non nuls (1 bit par I32, 8 bits).
//! Utilise cmpgt SIGNÉ : valide car les U8 packés sont ≤ 127 (garanti par FT_SHIFT)
inline uint32_t NnzMask(Vepi8 v) {
    return _mm256_movemask_ps(_mm256_castsi256_ps(_mm256_cmpgt_epi32(v, _mm256_setzero_si256())));
}

//! \brief  sum += dot4(u, w) par lane I32 (u = U8, w = I8).
//! Pas de VNNI en AVX2 → fallback maddubs+madd (3 instructions).
inline Vepi32 DpbusdEpi32(Vepi32 sum, Vepi8 u, Vepi8 w) {
    const __m256i product16 = _mm256_maddubs_epi16(u, w);
    return _mm256_add_epi32(sum, _mm256_madd_epi16(product16, _mm256_set1_epi16(1)));
}

//! \brief  Variante ×2 : sum += dot4(u0,w0) + dot4(u1,w1).
//! Économise un madd en additionnant les 2 produits en I16 —
//! sûr uniquement si |produit| ≤ 32767/2 (garanti par FT_SHIFT + clipping poids L1)
inline Vepi32 Dpbusdx2Epi32(Vepi32 sum, Vepi8 u0, Vepi8 w0, Vepi8 u1, Vepi8 w1) {
    const __m256i product16a = _mm256_maddubs_epi16(u0, w0);
    const __m256i product16b = _mm256_maddubs_epi16(u1, w1);
    const __m256i product32  = _mm256_madd_epi16(_mm256_add_epi16(product16a, product16b), _mm256_set1_epi16(1));
    return _mm256_add_epi32(sum, product32);
}

//--- Vecteurs float (couches L1→L2→L3 en float après déquantification) ---

inline Vps   ZeroPs()                          { return _mm256_setzero_ps(); }
inline Vps   SetPs(float num)                  { return _mm256_set1_ps(num); }
inline Vps   LoadPs(const float* mem)          { return _mm256_load_ps(mem); }
inline void  StorePs(float* mem, Vps v)        { _mm256_store_ps(mem, v); }
inline Vps   MinPs(Vps v1, Vps v2)             { return _mm256_min_ps(v1, v2); }
inline Vps   MaxPs(Vps v1, Vps v2)             { return _mm256_max_ps(v1, v2); }
inline Vps   MulPs(Vps v1, Vps v2)             { return _mm256_mul_ps(v1, v2); }
inline Vps   AddPs(Vps v1, Vps v2)             { return _mm256_add_ps(v1, v2); }
inline Vps   CvtEpi32Ps(Vepi32 v)              { return _mm256_cvtepi32_ps(v); }

//--------------------------------------------------------------------------------- SSE2
#elif defined __SSE2__

using Vepi16 = __m128i;
using Vepi32 = __m128i;

//=======================================================
//! \brief  Vecteur I16 mis à zéro (128 bits)
//-------------------------------------------------------
inline Vepi16 ZeroEpi16() {
    return _mm_setzero_si128();
}

//=======================================================
//! \brief  Vecteur I32 mis à zéro (128 bits)
//-------------------------------------------------------
inline Vepi32 ZeroEpi32() {
    return _mm_setzero_si128();
}

//=======================================================
//! \brief  Charge 8 I16 depuis une adresse alignée sur ALIGN octets
//! \param[in] memory_address   adresse mémoire alignée
//! \return Vecteur I16 chargé
//-------------------------------------------------------
inline Vepi16 LoadEpi16(const int16_t* memory_address) {
    return _mm_load_si128(reinterpret_cast<const __m128i*>(memory_address));
}

//=======================================================
//! \brief  Stocke un vecteur I16 à une adresse alignée sur ALIGN octets
//! \param[out] memory_address  adresse mémoire alignée de destination
//! \param[in]  vector          vecteur I16 à stocker
//-------------------------------------------------------
inline void StoreEpi16(void* memory_address, Vepi16 vector) {
    _mm_store_si128(reinterpret_cast<__m128i*>(memory_address), vector);
}

//=======================================================
//! \brief  Construit un vecteur I16 dont les 8 éléments valent num
//! \param[in] num  valeur à répliquer
//! \return Vecteur I16 rempli de num
//-------------------------------------------------------
inline Vepi16 SetEpi16(int num) {
    return _mm_set1_epi16(num);
}

//=======================================================
//! \brief  Addition élément par élément de 2 vecteurs I16
//! \param[in] v1  premier opérande
//! \param[in] v2  second opérande
//! \return v1 + v2 (I16)
//-------------------------------------------------------
inline Vepi16 AddEpi16(Vepi16 v1, Vepi16 v2) {
    return _mm_add_epi16(v1, v2);
}

//=======================================================
//! \brief  Addition élément par élément de 2 vecteurs I32
//! \param[in] v1  premier opérande
//! \param[in] v2  second opérande
//! \return v1 + v2 (I32)
//-------------------------------------------------------
inline Vepi32 AddEpi32(Vepi32 v1, Vepi32 v2) {
    return _mm_add_epi32(v1, v2);
}

//=======================================================
//! \brief  Soustraction élément par élément de 2 vecteurs I16
//! \param[in] v1  minuende
//! \param[in] v2  soustrahende
//! \return v1 - v2 (I16)
//-------------------------------------------------------
inline Vepi16 SubEpi16(Vepi16 v1, Vepi16 v2) {
    return _mm_sub_epi16(v1, v2);
}

//=======================================================
//! \brief  Écrêtage : clamp(vector, 0, l1q) élément par élément
//! \param[in] vector   vecteur I16 à écrêter
//! \param[in] l1q      borne supérieure de l'écrêtage (QA)
//! \return Vecteur I16 écrêté entre 0 et l1q
//-------------------------------------------------------
inline Vepi16 Clip(Vepi16 vector, int l1q) {
    // SSE2 n'a pas min/max epi16, mais SSE2 a _mm_min_epi16 (signé)
    // _mm_min_epi16 et _mm_max_epi16 existent en SSE2
    return _mm_min_epi16(_mm_max_epi16(vector, ZeroEpi16()), SetEpi16(l1q));
}

//--------------------------------------------------------------------------------- ARM NEON
// Advanced SIMD (NEON) : 128 bits, présent sur TOUT CPU AArch64 (pas de détection
// runtime nécessaire, contrairement à AVX/SSE sur x86). Même largeur que le palier
// SSE2 : 8 × I16 par vecteur. Bit-exact avec la version SSE2 (aucune saturation
// dans l'accumulateur).
// Types NEON DISTINCTS pour I16 (int16x8_t) et I32 (int32x4_t) — contrairement à
// x86 où Vepi16 et Vepi32 aliasent __m128i ; le code NNUE utilise "auto" partout
// et ne mélange jamais les deux, donc c'est transparent.
#elif defined(__ARM_NEON)

using Vepi16 = int16x8_t;
using Vepi32 = int32x4_t;

//=======================================================
//! \brief  Vecteur I16 mis à zéro (128 bits)
//-------------------------------------------------------
inline Vepi16 ZeroEpi16() {
    return vdupq_n_s16(0);
}

//=======================================================
//! \brief  Vecteur I32 mis à zéro (128 bits)
//-------------------------------------------------------
inline Vepi32 ZeroEpi32() {
    return vdupq_n_s32(0);
}

//=======================================================
//! \brief  Charge 8 I16 depuis mémoire (vld1q ne requiert pas d'alignement)
//! \param[in] memory_address   adresse mémoire
//! \return Vecteur I16 chargé
//-------------------------------------------------------
inline Vepi16 LoadEpi16(const int16_t* memory_address) {
    return vld1q_s16(memory_address);
}

//=======================================================
//! \brief  Stocke un vecteur I16 en mémoire
//! \param[out] memory_address  adresse mémoire de destination
//! \param[in]  vector          vecteur I16 à stocker
//-------------------------------------------------------
inline void StoreEpi16(void* memory_address, Vepi16 vector) {
    vst1q_s16(reinterpret_cast<int16_t*>(memory_address), vector);
}

//=======================================================
//! \brief  Construit un vecteur I16 dont les 8 éléments valent num
//! \param[in] num  valeur à répliquer
//! \return Vecteur I16 rempli de num
//-------------------------------------------------------
inline Vepi16 SetEpi16(int num) {
    return vdupq_n_s16(static_cast<int16_t>(num));
}

//=======================================================
//! \brief  Addition élément par élément de 2 vecteurs I16
//! \param[in] v1  premier opérande
//! \param[in] v2  second opérande
//! \return v1 + v2 (I16)
//-------------------------------------------------------
inline Vepi16 AddEpi16(Vepi16 v1, Vepi16 v2) {
    return vaddq_s16(v1, v2);
}

//=======================================================
//! \brief  Addition élément par élément de 2 vecteurs I32
//! \param[in] v1  premier opérande
//! \param[in] v2  second opérande
//! \return v1 + v2 (I32)
//-------------------------------------------------------
inline Vepi32 AddEpi32(Vepi32 v1, Vepi32 v2) {
    return vaddq_s32(v1, v2);
}

//=======================================================
//! \brief  Soustraction élément par élément de 2 vecteurs I16
//! \param[in] v1  minuende
//! \param[in] v2  soustrahende
//! \return v1 - v2 (I16)
//-------------------------------------------------------
inline Vepi16 SubEpi16(Vepi16 v1, Vepi16 v2) {
    return vsubq_s16(v1, v2);
}

//=======================================================
//! \brief  Écrêtage : clamp(vector, 0, l1q) élément par élément
//! \param[in] vector   vecteur I16 à écrêter
//! \param[in] l1q      borne supérieure de l'écrêtage (QA)
//! \return Vecteur I16 écrêté entre 0 et l1q
//-------------------------------------------------------
inline Vepi16 Clip(Vepi16 vector, int l1q) {
    return vminq_s16(vmaxq_s16(vector, ZeroEpi16()), SetEpi16(l1q));
}

#endif

} // namespace simd

#endif
#endif // SIMD_H
