#ifndef HUGEPAGES_H
#define HUGEPAGES_H

//  Allocation d'un gros bloc aligné, sur des pages de 2 Mo quand le système
//  les accorde. Repli silencieux sinon. Mesures et corpus : CHANGELOG 7.24.

#include <cstddef>
#include <cstdlib>
#include <memory>
#include <new>

#if defined(__linux__)
    #include <sys/mman.h>
#elif defined(_WIN32)
    #include <windows.h>
#endif

//  Une huge page vaut 2 Mo sur x86-64 et arm64.
static constexpr size_t HUGE_PAGE_SIZE = 2 * 1024 * 1024;

//==================================================
//! \brief  Arrondit une taille au multiple supérieur de "grain"
//--------------------------------------------------
inline size_t round_up(size_t size, size_t grain) noexcept
{
    return ((size + grain - 1) / grain) * grain;
}

#if defined(_WIN32)
//==================================================
//! \brief  Tente d'obtenir SeLockMemoryPrivilege, exigé par MEM_LARGE_PAGES
//! \return "true" si le privilège est acquis. L'échec est le cas courant.
//--------------------------------------------------
inline bool enable_lock_memory_privilege() noexcept
{
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token))
        return false;

    LUID luid;
    if (!LookupPrivilegeValue(nullptr, SE_LOCK_MEMORY_NAME, &luid))
    {
        CloseHandle(token);
        return false;
    }

    TOKEN_PRIVILEGES tp;
    tp.PrivilegeCount           = 1;
    tp.Privileges[0].Luid       = luid;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    const bool adjusted = AdjustTokenPrivileges(token, FALSE, &tp, sizeof(tp), nullptr, nullptr) != 0;

    // AdjustTokenPrivileges réussit même quand le privilège n'est pas détenu :
    // seul GetLastError distingue les deux cas.
    const bool granted = adjusted && GetLastError() != ERROR_NOT_ALL_ASSIGNED;

    CloseHandle(token);
    return granted;
}
#endif

//==================================================
//! \brief  Alloue "size" octets, sur des pages de 2 Mo si possible
//! \param[in]  size  taille demandée, en octets
//! \return Adresse alignée sur 2 Mo (64 octets pour les petits blocs), ou nullptr
//!
//! Mémoire NON initialisée : l'appelant doit l'écrire avant de la lire. Ce premier
//! parcours est aussi ce qui matérialise les huge pages, ne pas l'omettre.
//--------------------------------------------------
inline void* alloc_huge_pages(size_t size) noexcept
{
    if (size == 0)
        return nullptr;

    // Sous 2 Mo une huge page ne tient pas : alignement ordinaire, pas de madvise.
    const bool   big       = size >= HUGE_PAGE_SIZE;
    const size_t alignment = big ? HUGE_PAGE_SIZE : 64;

#if defined(_WIN32)
    if (big && enable_lock_memory_privilege())
    {
        const size_t minimum = GetLargePageMinimum();
        if (minimum != 0)
        {
            void* large = VirtualAlloc(nullptr, round_up(size, minimum),
                                       MEM_COMMIT | MEM_RESERVE | MEM_LARGE_PAGES, PAGE_READWRITE);
            if (large != nullptr)
                return large;
        }
    }
    return VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
    // aligned_alloc exige une taille multiple de l'alignement.
    void* memory = std::aligned_alloc(alignment, round_up(size, alignment));

    #if defined(__linux__) && defined(MADV_HUGEPAGE)
    if (memory != nullptr && big)
        madvise(memory, round_up(size, alignment), MADV_HUGEPAGE);
    #endif

    return memory;
#endif
}

//==================================================
//! \brief  Libère un bloc rendu par alloc_huge_pages()
//--------------------------------------------------
inline void free_huge_pages(void* memory) noexcept
{
    if (memory == nullptr)
        return;

#if defined(_WIN32)
    VirtualFree(memory, 0, MEM_RELEASE);
#else
    std::free(memory);
#endif
}

//==================================================
//! \brief  Deleter d'un tableau alloué par make_huge_array()
//! Transporte le nombre d'éléments : unique_ptr ne connaît pas la taille d'un tableau.
//--------------------------------------------------
template <typename T>
struct HugePageArrayDeleter
{
    size_t count = 0;

    void operator()(T* data) const noexcept
    {
        std::destroy_n(data, count);
        free_huge_pages(data);
    }
};

//! \brief  Tableau possédé, sur pages de 2 Mo. S'utilise comme un unique_ptr<T[]>.
template <typename T>
using HugeArray = std::unique_ptr<T[], HugePageArrayDeleter<T>>;

//==================================================
//! \brief  Alloue et construit un tableau de "n" objets sur des pages de 2 Mo
//! \return Tableau possédé, vide (testable par if) si l'allocation échoue
//!
//! Objets value-initialisés, comme std::make_unique<T[]>(n).
//! ORDRE IMPÉRATIF : le madvise d'alloc_huge_pages() doit précéder la
//! construction, qui est le premier accès à la mémoire.
//--------------------------------------------------
template <typename T>
inline HugeArray<T> make_huge_array(size_t n)
{
    T* data = static_cast<T*>(alloc_huge_pages(n * sizeof(T)));
    if (data == nullptr)
        return HugeArray<T>(nullptr, HugePageArrayDeleter<T>{0});

    // Pas de try/catch : la cible release compile en -fno-exceptions.
    std::uninitialized_value_construct_n(data, n);

    return HugeArray<T>(data, HugePageArrayDeleter<T>{n});
}

#endif // HUGEPAGES_H
