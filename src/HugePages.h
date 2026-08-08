#ifndef HUGEPAGES_H
#define HUGEPAGES_H

//  Allocation d'un gros bloc aligné, sur des pages de 2 Mo quand le système
//  les accorde. Utilisé par la table de transposition.
//
//  Pourquoi : au-delà de ~8 Mo, chaque sonde de TT manque le TLB (2048 entrées
//  de 4 Ko sur Zen 3, soit 8 Mo de couverture). Les pages de 2 Mo divisent par
//  512 l'empreinte des tables de pages et portent cette couverture à 4 Go.
//  Mesures et corpus dans References/CHANGELOG.md.
//
//  Le repli est SILENCIEUX et toujours fonctionnel : sans privilège (Windows)
//  ou sans support noyau, on obtient de la mémoire ordinaire, jamais un échec.

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
//! \brief  Tente d'obtenir SeLockMemoryPrivilege, indispensable à MEM_LARGE_PAGES
//! \return "true" si le privilège est acquis
//!
//! Ce privilège n'est PAS accordé par défaut ; il se donne dans la stratégie de
//! sécurité locale ("Verrouiller les pages en mémoire"). L'échec est le cas
//! courant, et il est normal : l'appelant se rabat sur une allocation ordinaire.
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
//! \return Adresse alignée sur 2 Mo (ou sur 64 octets pour les petits blocs),
//!         ou nullptr si l'allocation échoue.
//!
//! La mémoire rendue n'est PAS initialisée : l'appelant doit l'écrire avant de
//! la lire. C'est aussi ce premier parcours (first touch) qui matérialise les
//! huge pages sous THP=madvise — sans lui les pages restent en 4 Ko.
//--------------------------------------------------
inline void* alloc_huge_pages(size_t size) noexcept
{
    if (size == 0)
        return nullptr;

    // Sous 2 Mo une huge page ne tient pas : alignement ordinaire, pas de madvise.
    // Sur-aligner un petit bloc ne servirait qu'à gaspiller (cf. Stormphrax).
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
//!
//! Il transporte le nombre d'éléments : sans lui, impossible d'appeler les
//! destructeurs, unique_ptr ne connaissant pas la taille d'un tableau.
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
//! Les objets sont value-initialisés, comme le fait std::make_unique<T[]>(n) :
//! le remplacer par cette fonction ne change donc pas leur état de départ. Ce
//! que make_unique ne sait pas faire, c'est passer par un allocateur à nous —
//! d'où cette fonction, et non un simple deleter.
//!
//! L'ordre compte : le madvise est posé par alloc_huge_pages() AVANT la
//! construction, et c'est la construction qui touche la mémoire en premier.
//! Un madvise posé après coup n'aurait plus aucun effet — les pages seraient
//! déjà montées en 4 Ko.
//--------------------------------------------------
template <typename T>
inline HugeArray<T> make_huge_array(size_t n)
{
    T* data = static_cast<T*>(alloc_huge_pages(n * sizeof(T)));
    if (data == nullptr)
        return HugeArray<T>(nullptr, HugePageArrayDeleter<T>{0});

    // Pas de try/catch pour libérer le bloc si un constructeur lance : la cible
    // release compile en -fno-exceptions, un lancer y termine le programme.
    std::uninitialized_value_construct_n(data, n);

    return HugeArray<T>(data, HugePageArrayDeleter<T>{n});
}

#endif // HUGEPAGES_H
