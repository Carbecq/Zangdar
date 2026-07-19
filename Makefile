CXX = clang++
# CXX = /bin/g++-14

.DEFAULT_GOAL := openbench

# Même test que celui qui choisit les CFLAGS plus bas (findstring, pas égalité
# stricte) : sinon un CXX versionné ou en chemin absolu (clang++-18,
# /usr/bin/clang++ — ce que passe OpenBench) compile avec les flags clang mais
# produit un binaire nommé « -g++ ».
# COMP=mingw reste surchargeable en ligne de commande pour le cross Windows.
ifeq ($(findstring clang, $(CXX)), clang)
	COMP := clang
else
	COMP := g++
endif

SRC1 = src

# Utilisé uniquement par la cible pgo (compilation en un seul appel).
SRC = src/*.cpp src/pyrrhic/*.cpp

# find est récursif : src/pyrrhic est déjà inclus. Ne pas l'ajouter une 2e fois,
# cela mettait tbprobe.o en double dans OBJ (masqué au link parce que $^
# déduplique, mais visible dans clean et fragile).
CPP_FILES = $(shell find $(SRC1) -name "*.cpp")
OBJ = $(patsubst %.cpp, %.o, $(CPP_FILES))

# Fichiers de dépendances générés par -MMD (voir la règle %.o) : ils indiquent à
# make quels .o recompiler quand un header change. Sans eux, modifier un .h ne
# déclenchait aucune recompilation => .o périmés, et crashes à l'exécution.
DEP = $(OBJ:.o=.d)

### ==========================================================================
### Section 1. General Configuration (Stockfish)
### ==========================================================================

### Establish the operating system name
KERNEL := $(shell uname -s)
ifeq ($(KERNEL),Linux)
	OS := $(shell uname -o)
endif

### Target Windows OS
ifeq ($(OS),Windows_NT)
	target_windows = yes
else ifeq ($(COMP),mingw)
	target_windows = yes
endif

#---------------------------------------------------------------------
#   Defines divers
#---------------------------------------------------------------------
CONFIG = VERSION.txt
include ${CONFIG}


#---------------------------------------------------------------------
#   Définition locale des tables Syzygy 
#   1- n'est utilisé que pour le raccourci yyy (tests internes).
#   2- pour Datagen. Dans le cas d'une utilisation  sur un serveur externe,
#      il faut modifier le makefile
# Path to the folders/directories storing the Syzygy tablebase files. 
# Multiple directories are to be separated by ";" on Windows and by ":" on Unix-based operating systems. 
# Do not use spaces around the ";" or ":".
#---------------------------------------------------------------------
ifeq ($(target_windows),yes)
    SYZYGY = D:/Echecs/Syzygy/345;D:/Echecs/Syzygy/6
else
    SYZYGY = /mnt/Datas/Echecs/Syzygy/345:/mnt/Datas/Echecs/Syzygy/6
endif
DEFS += -DSYZYGY='"$(SYZYGY)"'

DEFS += $(EXTRA_DEFS)

#   make EXTRA_DEFS=-DUSE_TUNING
#   make EXTRA_DEFS="-DUSE_TUNING -DDEBUG_LOG"

#  Quelques defines utilisés en debug
# EXTRA_DEFS = -DDEBUG_LOG
# EXTRA_DEFS = -DDEBUG_TIME

#---------------------------------------------------
#  Tuning
#---------------------------------------------------
#EXTRA_DEFS = -DUSE_TUNING

#---------------------------------------------------
#  NNUE
#---------------------------------------------------
NETS_DIR  = networks
NETS_FILE = $(NETS_DIR)/$(NET_NAME)
NETS_REPO = https://github.com/Carbecq/Zangdar-Networks/releases/download

# EVALFILE peut être surchargé en ligne de commande (ex: OpenBench)
EVALFILE ?= $(NETS_FILE)

CFLAGS_NNUE = -DEVALFILE=\"$(EVALFILE)\" -DNET_NAME=\"$(NET_NAME)\"

#---------------------------------------------------------------------
#   Architecture
#---------------------------------------------------------------------
ifeq ($(ARCH), )
   ARCH = native
endif

CFLAGS_ARCH =
ZANGDAR     = Zangdar
ZANGDAR_DEV = Zangdar_dev

DEFAULT_EXE = $(ZANGDAR)

# Détection du CPU local
CPU_MODEL := $(shell cat /proc/cpuinfo 2>/dev/null | grep -m1 'model name' | sed 's/.*: //;s/(R)//g;s/(TM)//g;s/CPU //;s/ [0-9]*-Core.*//;s/Processor//;s/  */ /g;s/ *$$//;s/ /-/g;s/[^a-zA-Z0-9_-]//g')
ifeq ($(CPU_MODEL),)
    CPU_MODEL := unknown
endif

# Détection des capacités CPU
HAS_SSE2   := $(shell grep -qm1 'sse2'    /proc/cpuinfo 2>/dev/null && echo yes)
HAS_POPCNT := $(shell grep -qm1 'popcnt'  /proc/cpuinfo 2>/dev/null && echo yes)
HAS_AVX2   := $(shell grep -qm1 'avx2'    /proc/cpuinfo 2>/dev/null && echo yes)
HAS_AVX512 := $(shell grep -qm1 'avx512f' /proc/cpuinfo 2>/dev/null && echo yes)
HAS_BMI2   := $(shell grep -qm1 'bmi2'    /proc/cpuinfo 2>/dev/null && echo yes)

# SIMD activé si au moins SSE2.
# ATTENTION : déduit du CPU HÔTE (/proc/cpuinfo) => ne vaut QUE pour ARCH=native.
# Les cibles ARCH explicites (sse2/avx2/bmi2/avx512) mettent -DUSE_SIMD en dur :
# leur jeu d'instructions est connu par définition, et dépendre de l'hôte
# produisait un binaire -mavx2 SANS kernel SIMD (donc NNUE scalaire, beaucoup
# plus lent, et sans le moindre avertissement) dès que /proc/cpuinfo est absent
# ou illisible — conteneur minimal, cross-compilation, hôte non-Linux.
SIMD =
ifeq ($(HAS_AVX512), yes)
    SIMD = -DUSE_SIMD
else ifeq ($(HAS_AVX2), yes)
    SIMD = -DUSE_SIMD
else ifeq ($(HAS_SSE2), yes)
    SIMD = -DUSE_SIMD
endif

# /!\ TOUJOURS « make clean » AVANT DE CHANGER D'ARCH /!\
# Les .o sont tous dans src/ sans marqueur d'architecture, et make ne compare que
# les dates .cpp/.o : il ne voit pas que les flags ont changé. Sans clean,
#     make release ARCH=native   puis   make release ARCH=avx2
# ne recompile RIEN et se contente de relinker les objets natifs sous le nom
# « avx2 » => binaire contenant des instructions du CPU de compilation (PEXT,
# AVX-512…), donc SIGILL sur les machines que la cible avx2 est censée servir.
# Vaut aussi pour tout changement de CXX, de STATIC ou de EXTRA_DEFS.
#
# Guide des cibles ARCH publiées (échelle standard, même découpage que Stockfish).
# Chaque cible correspond à une vraie différence fonctionnelle (kernel NNUE de
# simd.h et/ou attaques PEXT), pas à de la granularité gratuite :
#   sse2   : kernel NNUE SSE2, magics. Machines anciennes (~2008-2013) et VMs sans
#            AVX. NB : -mpopcnt inclus => exige un CPU >= 2008 (Nehalem) ; un vrai
#            pre-2008 est sans espoir pour un NNUE de toute facon.
#   avx2   : kernel AVX2, magics. Le standard 2013+ (Intel Haswell+, et AMD Zen 1/2 !)
#   bmi2   : kernel AVX2 + attaques PEXT. UNIQUEMENT Intel Haswell+ et AMD Zen 3+.
#            PIÈGE : Zen 1/Zen 2 annoncent BMI2 mais leur pext est microcodé
#            (~100+ cycles au lieu de 3) => bmi2 y est PLUS LENT que avx2.
#   avx512 : kernel AVX-512 + PEXT. Zen 4/5, Intel serveur/HEDT (Ice Lake-SP+).
#            Validé bit-exact 2026-06-12 (Tiger Lake). Voir le bloc avx512 pour
#            la question VNNI.
#   native : auto-détection du CPU hôte (pour compiler soi-même).
ifeq ($(ARCH), sse2)
    DEFAULT_EXE = $(ZANGDAR)-$(VERSION)-$(ARCH)
    CFLAGS_ARCH += -msse -msse2 -mpopcnt
    CFLAGS_ARCH += -DUSE_SIMD

else ifeq ($(ARCH), avx2)
    DEFAULT_EXE = $(ZANGDAR)-$(VERSION)-$(ARCH)
    CFLAGS_ARCH += -msse -msse2
    CFLAGS_ARCH += -msse3 -mpopcnt
    CFLAGS_ARCH += -msse4.1 -msse4.2
    CFLAGS_ARCH += -mssse3
    CFLAGS_ARCH += -mmmx
    CFLAGS_ARCH += -mavx
    CFLAGS_ARCH += -mavx2 -mfma
    CFLAGS_ARCH += -DUSE_SIMD

else ifeq ($(ARCH), bmi2)
    DEFAULT_EXE = $(ZANGDAR)-$(VERSION)-$(ARCH)
    CFLAGS_ARCH += -msse -msse2
    CFLAGS_ARCH += -msse3 -mpopcnt
    CFLAGS_ARCH += -msse4.1 -msse4.2
    CFLAGS_ARCH += -mssse3
    CFLAGS_ARCH += -mmmx
    CFLAGS_ARCH += -mavx
    CFLAGS_ARCH += -mavx2 -mfma
    CFLAGS_ARCH += -mbmi -mbmi2
    CFLAGS_ARCH += -DUSE_PEXT -DUSE_SIMD

else ifeq ($(ARCH), avx512)
    # AVX-512 = surensemble de bmi2 (avx2 + bmi2 + pext) + AVX-512 F/BW/DQ/VL.
    # Le code NNUE active son chemin 512 bits sur __AVX512F__ && __AVX512BW__ (src/NNUE.h, src/simd.h).
    #
    # PAS de -mavx512vnni ici, pour DEUX raisons (avant d'ajouter une cible vnni512, lire ceci) :
    # 1. Portabilité : le binaire publié doit tourner sur tout CPU AVX-512 (Skylake-X+,
    #    Ice/Tiger Lake, Zen 4+), y compris ceux sans VNNI.
    # 2. Inutile en l'état : simd.h n'a AUCUN chemin dpbusd — l'inférence SCReLU actuelle
    #    travaille en int16 (mullo/madd_epi16) et elle est memory-bound (mesuré 2026-06-12
    #    sur Tiger Lake : 0 gain NPS du 512 bits, bit-exact 24 805 988). Le flag seul ne
    #    changerait rien. Le VNNI ne paiera qu'avec une inférence écrite pour lui (poids
    #    int8 + accumulation dpbusd, i.e. le chantier multi-couches) ; ce jour-là, créer
    #    une cible vnni512 dédiée AVEC le nouveau code.
    # NB : les macros affichées par la commande UCI "systeme" (__AVX512VNNI__, __BMI2__,
    # USE_PEXT…) sont compile-time, pas une détection du CPU hôte.
    DEFAULT_EXE = $(ZANGDAR)-$(VERSION)-$(ARCH)
    CFLAGS_ARCH += -msse -msse2
    CFLAGS_ARCH += -msse3 -mpopcnt
    CFLAGS_ARCH += -msse4.1 -msse4.2
    CFLAGS_ARCH += -mssse3
    CFLAGS_ARCH += -mmmx
    CFLAGS_ARCH += -mavx
    CFLAGS_ARCH += -mavx2 -mfma
    CFLAGS_ARCH += -mbmi -mbmi2
    CFLAGS_ARCH += -mavx512f -mavx512bw -mavx512dq -mavx512vl
    CFLAGS_ARCH += -DUSE_PEXT -DUSE_SIMD

else ifeq ($(ARCH), arm64)
    # Cross-compilation Linux aarch64 : make ARCH=arm64 CXX=aarch64-linux-gnu-g++ STATIC=yes
    # (paquet crossbuild-essential-arm64 pour aarch64-linux-gnu-g++ + libs statiques)
    # USE_SIMD => kernel NEON de simd.h (Advanced SIMD, présent sur tout CPU armv8-a,
    #             pas de détection runtime nécessaire). 128 bits = équivalent SSE2.
    # Pas de PEXT (x86 only) => magic bitboards (Attacks.h) utilisés à la place.
    # HAS_SSE2/AVX2/AVX512/BMI2/CPU_MODEL viennent de /proc/cpuinfo de la machine hôte
    # (x86, celle qui compile) : sans objet pour la cible arm64, donc ignorés.
    DEFAULT_EXE = $(ZANGDAR)-$(VERSION)-$(ARCH)
    CFLAGS_ARCH += -march=armv8-a -DUSE_SIMD

else ifeq ($(ARCH), native)
    DEFAULT_EXE = $(ZANGDAR)-$(VERSION)-$(CPU_MODEL)
    CFLAGS_ARCH += -march=native -mtune=native
    ifeq ($(HAS_BMI2), yes)
        CFLAGS_ARCH += -DUSE_PEXT
    endif
    CFLAGS_ARCH += $(SIMD)

else
    ARCH = native
    DEFAULT_EXE = $(ZANGDAR)-$(VERSION)-$(CPU_MODEL)
    CFLAGS_ARCH += -march=native -mtune=native
    ifeq ($(HAS_BMI2), yes)
        CFLAGS_ARCH += -DUSE_PEXT
    endif
    CFLAGS_ARCH += $(SIMD)

endif

$(info Version   = $(VERSION))
$(info CXX       = $(CXX))
$(info ARCH      = $(ARCH))
$(info CPU       = $(CPU_MODEL))
# Capacités du CPU HÔTE (informatif ; ne décrit la cible que si ARCH=native)
$(info SSE2      = $(if $(HAS_SSE2),yes,no)   (hôte))
$(info POPCNT    = $(if $(HAS_POPCNT),yes,no)   (hôte))
$(info AVX2      = $(if $(HAS_AVX2),yes,no)   (hôte))
$(info AVX512    = $(if $(HAS_AVX512),yes,no)   (hôte))
$(info BMI2      = $(if $(HAS_BMI2),yes,no)   (hôte))
# Ce qui est RÉELLEMENT compilé : lu dans CFLAGS_ARCH, pas déduit de l'hôte.
$(info SIMD      = $(if $(findstring USE_SIMD,$(CFLAGS_ARCH)),yes,no))
$(info PEXT      = $(if $(findstring USE_PEXT,$(CFLAGS_ARCH)),yes,no))
$(info OS        = $(OS))
$(info Evalfile  = $(NET_NAME))
$(info Tuning    = $(if $(findstring USE_TUNING,$(DEFS)),yes,no))

### Executable name
ifeq ($(target_windows),yes)
	SUF := .exe
else
	SUF :=
endif

#---------------------------------------------------------------------
#   Options de compilation
#	https://gcc.gnu.org/onlinedocs/gcc/Option-Index.html
#	https://clang.llvm.org/docs/DiagnosticsReference.html
#   Test : https://godbolt.org/
#---------------------------------------------------------------------

# -march=X: (execution domain) Generate code that can use instructions available in the architecture X
# -mtune=X: (optimization domain) Optimize for the microarchitecture X, but does not change the ABI or make assumptions about available instructions

ifeq ($(findstring clang, $(CXX)), clang)

CFLAGS_REL1  = -O3 -flto -ftree-vectorize -funroll-loops -fno-exceptions -DNDEBUG -pthread -Wdisabled-optimization -Wall -Wextra \
               -finline-functions -fno-rtti -fstrict-aliasing  \
               -fvectorize -fslp-vectorize
CFLAGS_WARN1 = -Wmissing-declarations -Wredundant-decls -Wshadow -Wundef -Wuninitialized -pedantic

CFLAGS_WARN2 = -Wcast-align -Wcast-qual -Wctor-dtor-privacy -Wswitch -Wswitch-default -Wswitch-enum -Wenum-compare -Wenum-conversion
CFLAGS_WARN3 = -Wformat=2 -Winit-self -Wmissing-include-dirs
CFLAGS_WARN4 = -Woverloaded-virtual -Warray-bounds
CFLAGS_WARN5 = -Wsign-promo -Wstrict-overflow=5 -Wno-unused
CFLAGS_WARN6 = -Wattributes -Waddress -Wfloat-equal -Wmissing-prototypes -Wconditional-uninitialized
# CFLAGS_WARN7 = -Wsign-conversion

# -fwhole-program-vtables : clang uniquement, et pas sous Windows.
ifneq ($(target_windows),yes)
    CFLAGS_REL1 += -fwhole-program-vtables
endif

PGO_GEN   = -fprofile-instr-generate
PGO_MERGE = llvm-profdata merge -output=zangdar.profdata *.profraw
PGO_USE   = -fprofile-instr-use=zangdar.profdata

else

CFLAGS_REL1  = -O3 -flto=auto -ftree-vectorize -funroll-loops -fno-exceptions -DNDEBUG -pthread -fwhole-program -Wdisabled-optimization -Wall -Wextra \
               -finline-functions -fno-rtti -fstrict-aliasing
CFLAGS_WARN1 = -Wmissing-declarations -Wredundant-decls -Wshadow -Wundef -Wuninitialized -pedantic

CFLAGS_WARN2 = -Wcast-align=strict -Wcast-qual -Wctor-dtor-privacy -Wswitch -Wswitch-default -Wswitch-enum -Wenum-compare -Wenum-conversion
CFLAGS_WARN3 = -Wformat=2 -Winit-self -Wlogical-op -Wmissing-include-dirs
CFLAGS_WARN4 = -Wnoexcept -Woverloaded-virtual -Warray-bounds=2
CFLAGS_WARN5 = -Wsign-promo -Wstrict-null-sentinel -Wstrict-overflow=5 -Wno-unused
CFLAGS_WARN6 = -Wattributes -Waddress -Wfloat-equal -Wbidi-chars -Warray-compare -Wuninitialized
# CFLAGS_WARN7 = -Wsign-conversion

PGO_GEN   = -fprofile-generate
PGO_USE   = -fprofile-use

endif

#---------------------------------------------------------------------
#   Taille de pile (indépendant du compilateur : clang comme g++/mingw)
#---------------------------------------------------------------------
# Windows : la pile est fixée dans l'exécutable au link, il FAUT ce flag.
#           16 Mo (16*1024*1024). Syntaxe passthrough vers ld, valable
#           pour clang-cl comme pour mingw g++.
# Linux   : la pile du thread principal est fixée par le shell, pas par le
#           binaire => rien à mettre ici, utiliser « ulimit -s <taille> »
#           ou « ulimit -s unlimited » avant de lancer.
#           (-Wl,-z,stack-size ne concerne que les threads, pas le main.)
LDFLAGS_STACK =
ifeq ($(target_windows),yes)
    LDFLAGS_STACK = -Wl,--stack,16777216
endif

# https://stackoverflow.com/questions/5088460/flags-to-enable-thorough-and-verbose-g-warnings
# CFLAGS  : -fsanitize=undefined -fsanitize=memory and -fsanitize=address and maybe even -fsanitize=thread
# LDFLAGS : -fsanitize=address -static-libsan

#   Pour perf record / perf report, il faut des symboles de debug et pas de frame pointer omission :
#       -g -fno-omit-frame-pointer -O3
#   -pg c'est pour gprof, pas perf
#

CFLAGS_COM  = -pipe -std=c++23 -DVERSION=\"$(VERSION)\" $(DEFS) $(CFLAGS_NNUE)
CFLAGS_REL  = $(CFLAGS_REL1) $(CFLAGS_WARN1) -DNDEBUG -fomit-frame-pointer
CFLAGS_DBG  = -g -O2
CFLAGS_WARN = $(CFLAGS_WARN1) $(CFLAGS_WARN2) $(CFLAGS_WARN3) $(CFLAGS_WARN4) $(CFLAGS_WARN5) $(CFLAGS_WARN6)
CFLAGS_PERF = $(CFLAGS_REL1) $(CFLAGS_WARN1)  -DNDEBUG -g -fno-omit-frame-pointer -DUSE_PROFILING

# Sanitizer build : ASan + UBSan combinés (compatibles entre eux)
# Pas de -flto, pas de -fomit-frame-pointer, pas de -static (incompatibles ASan).
# -fno-sanitize-recover=all → le programme abort dès la 1re erreur (signal clair).
CFLAGS_SAN  = -pthread -g -O1 -fno-omit-frame-pointer -fno-optimize-sibling-calls \
              -fsanitize=undefined,address -fno-sanitize-recover=all \
              $(CFLAGS_WARN1)

# ThreadSanitizer : détection des data races (incompatible avec ASan, d'où
# une cible séparée). -O2 pour un débit acceptable (TSan ralentit 5-15x).
# Les races VOLONTAIRES du Lazy SMP (TT lockless, lecture des compteurs
# nodes pendant la recherche) sont masquées via tsan_suppressions.txt.
CFLAGS_TSAN = -pthread -g -O2 -fno-omit-frame-pointer \
              -fsanitize=thread \
              $(CFLAGS_WARN1)

# Release sans LTO ni whole-program-vtables : pour isoler les UB démasquées par
# l'optimisation inter-modules. Conserve -O3 et le reste des flags release.
CFLAGS_REL_NOLTO = $(filter-out -flto -fwhole-program-vtables -flto=auto -fwhole-program, $(CFLAGS_REL1)) \
                   $(CFLAGS_WARN1) -DNDEBUG -fomit-frame-pointer

# Release-nolto + auto-init des variables stack à zéro.
# Si le bench devient déterministe avec ce flag, c'est confirmé que la cause
# du non-déterminisme est une lecture de variable locale non initialisée.
CFLAGS_REL_AUTOINIT = $(CFLAGS_REL_NOLTO) -ftrivial-auto-var-init=zero

LDFLAGS_REL  = -s -flto -lm
LDFLAGS_DBG  = -lm
LDFLAGS_PERF = -pg -flto -lm
LDFLAGS_SAN  = -fsanitize=undefined,address -lm
LDFLAGS_REL_NOLTO = -s -lm

PGO_FLAGS = -fno-asynchronous-unwind-tables

#---------------------------------------------------------------------
#   Targets
#---------------------------------------------------------------------

### Executable statique.
### - Windows : toujours (pas de DLL runtime à trimballer).
### - Linux (natif ou cross) : à la demande, make STATIC=yes ...
###   Utile pour distribuer un binaire qui tourne sur n'importe quelle distro
###   (pas de dépendance à la glibc/libstdc++ de la machine de compilation),
###   typiquement pour OpenBench ou une release GitHub.
###   Le binaire prend le suffixe -static pour ne pas écraser la version dynamique.
### NB : incompatible avec les cibles sanitize/sanitize-thread (ASan/TSan exigent
###      un link dynamique), qui n'utilisent donc pas LDFLAGS_STATIC.
LDFLAGS_STATIC =
STATIC_SUF     =

ifeq ($(target_windows),yes)
    LDFLAGS_STATIC += -static
else ifeq ($(STATIC),yes)
    LDFLAGS_STATIC += -static
    STATIC_SUF     := -static
endif

# ASan/TSan exigent un link dynamique : un STATIC=yes passé en ligne de commande
# est ignoré ici (sinon le binaire porterait un suffixe -static mensonger).
ifneq ($(filter sanitize sanitize-thread,$(MAKECMDGOALS)),)
    LDFLAGS_STATIC =
    STATIC_SUF     =
endif

$(info Static    = $(if $(LDFLAGS_STATIC),yes,no))

EXE  = $(DEFAULT_EXE)-$(COMP)$(STATIC_SUF)

openbench: EXEC    = $(EXE)$(SUF)
openbench: CFLAGS  = $(CFLAGS_COM) $(CFLAGS_ARCH) $(CFLAGS_REL)
openbench: LDFLAGS = $(LDFLAGS_REL) $(LDFLAGS_STATIC) $(LDFLAGS_STACK)

release: EXEC    = $(EXE)-rel$(SUF)
release: CFLAGS  = $(CFLAGS_COM) $(CFLAGS_ARCH) $(CFLAGS_REL)
release: LDFLAGS = $(LDFLAGS_REL) $(LDFLAGS_STATIC) $(LDFLAGS_STACK)

debug: EXEC    = $(EXE)-dbg$(SUF)
debug: CFLAGS  = $(CFLAGS_COM) $(CFLAGS_ARCH) $(CFLAGS_WARN) $(CFLAGS_DBG)
debug: LDFLAGS = $(LDFLAGS_DBG) $(LDFLAGS_STATIC) $(LDFLAGS_STACK)

# Pas de LDFLAGS_STATIC : -static est incompatible avec ASan.
sanitize: EXEC    = $(EXE)-san$(SUF)
sanitize: CFLAGS  = $(CFLAGS_COM) $(CFLAGS_ARCH) $(CFLAGS_SAN)
sanitize: LDFLAGS = $(LDFLAGS_SAN)

sanitize-thread: EXEC    = $(EXE)-tsan$(SUF)
sanitize-thread: CFLAGS  = $(CFLAGS_COM) $(CFLAGS_ARCH) $(CFLAGS_TSAN)
sanitize-thread: LDFLAGS = -fsanitize=thread -lm

# Release sans LTO ni whole-program — pour test déterminisme bench
release-nolto: EXEC    = $(EXE)-rel-nolto$(SUF)
release-nolto: CFLAGS  = $(CFLAGS_COM) $(CFLAGS_ARCH) $(CFLAGS_REL_NOLTO)
release-nolto: LDFLAGS = $(LDFLAGS_REL_NOLTO) $(LDFLAGS_STATIC) $(LDFLAGS_STACK)

# Release-nolto avec auto-init des stack vars à zéro
release-autoinit: EXEC    = $(EXE)-rel-autoinit$(SUF)
release-autoinit: CFLAGS  = $(CFLAGS_COM) $(CFLAGS_ARCH) $(CFLAGS_REL_AUTOINIT)
release-autoinit: LDFLAGS = $(LDFLAGS_REL_NOLTO) $(LDFLAGS_STATIC) $(LDFLAGS_STACK)

# Release-nolto + -g, SANS auto-init et SANS strip : pour valgrind memcheck
# (auto-init masquerait les lectures non-init ; -g pour les stack traces).
release-valgrind: EXEC    = $(EXE)-rel-vg$(SUF)
release-valgrind: CFLAGS  = $(CFLAGS_COM) $(CFLAGS_ARCH) $(CFLAGS_REL_NOLTO) -g
release-valgrind: LDFLAGS = -lm $(LDFLAGS_STATIC) $(LDFLAGS_STACK)

# Release-nolto + UBSan bounds/object-size + FORTIFY=3 : détecte à l'exécution
# les indices hors bornes Y COMPRIS intra-structure (invisibles pour ASan et
# valgrind) et les memcpy débordant un membre (update_pv). Layout quasi-release
# (pas de shadow memory) => bien moins heisenbug-prone que ASan. Abort + core
# à la 1ère violation.
release-bounds: EXEC    = $(EXE)-rel-bounds$(SUF)
release-bounds: CFLAGS  = $(CFLAGS_COM) $(CFLAGS_ARCH) $(CFLAGS_REL_NOLTO) -g -fsanitize=bounds,object-size -fno-sanitize-recover=all -D_FORTIFY_SOURCE=3
release-bounds: LDFLAGS = -lm $(LDFLAGS_STATIC) $(LDFLAGS_STACK) -fsanitize=bounds,object-size

perf: EXEC    = $(EXE)-perf$(SUF)
perf: CFLAGS  = $(CFLAGS_COM) $(CFLAGS_ARCH) $(CFLAGS_PERF)
perf: LDFLAGS = $(LDFLAGS_PERF) $(LDFLAGS_STATIC) $(LDFLAGS_STACK)

pgo: EXEC    = $(EXE)-pgo$(SUF)
pgo: CFLAGS  = $(CFLAGS_COM) $(CFLAGS_ARCH) $(CFLAGS_REL)
pgo: LDFLAGS = $(LDFLAGS_REL) $(LDFLAGS_STATIC) $(LDFLAGS_STACK)

# Bench d'entraînement : exécuté entre la passe -generate et la passe -use.
# (PGO_CLEAN/PGO_DIR, hérités d'Ethereal, ont été supprimés : PGO_DIR n'était
#  défini nulle part et PGO_CLEAN n'était appelé par aucune recette. Le ménage
#  est fait en dur dans la cible pgo, et il suffit : gcc comme clang déposent
#  leurs fichiers de profil dans le répertoire courant, jamais dans src/.)
# /dev/null y compris sous Windows : les recettes sont exécutées par $(SHELL)
# (bash sous MSYS2), pas par cmd.exe — « > nul » y créerait un fichier nommé
# « nul ». De toute façon ce Makefile exige un shell Unix : ses recettes
# utilisent rm, mv, cp, mkdir, find, test et curl.
PGO_BENCH = ./$(EXE) bench 20 > /dev/null 2>&1

#---------------------------------------------------------------------
#	Dépendances
#---------------------------------------------------------------------

# Ces cibles ne produisent pas de fichier portant leur nom : sans .PHONY, un
# fichier homonyme dans le répertoire les rendrait inopérantes (« clean est à
# jour »). Cela force aussi make à toujours exécuter leur recette.
.PHONY: openbench release debug sanitize sanitize-thread \
        release-nolto release-autoinit release-valgrind release-bounds \
        perf pgo download-net clean mrproper

# Target pour OpenBench : compile directement vers $(EXE) sans renommage
# OpenBench appelle : make -j EXE=<chemin> [EVALFILE=<réseau>] [CXX=<compilateur>]
openbench: $(EXE)
	@if [ -n "$(SUF)" ] && [ -f "$(EXE)" ]; then mv $(EXE) $(EXEC); fi
	@cp $(EXEC) $(ZANGDAR_DEV)
	@echo "Génération pour OpenBench : $(EXEC)"

release: $(EXE)
	@mv $(EXE) $(EXEC)
	@cp $(EXEC) $(ZANGDAR_DEV)
	@echo "Génération en mode release : $(EXEC)"

perf: $(EXE)
	@mv $(EXE) $(EXEC)
	@cp $(EXEC) $(ZANGDAR_DEV)
	@echo "Génération en mode test perf : $(EXEC)"

debug: $(EXE)
	@mv $(EXE) $(EXEC)
	@cp $(EXEC) $(ZANGDAR_DEV)
	@echo "Génération en mode debug : $(EXEC)"

sanitize: $(EXE)
	@mv $(EXE) $(EXEC)
	@echo "Génération en mode sanitize : $(EXEC)"
	@echo "Lance avec : UBSAN_OPTIONS=print_stacktrace=1 ASAN_OPTIONS=detect_leaks=0 ./$(EXEC) bench 30 1 128"

sanitize-thread: $(EXE)
	@mv $(EXE) $(EXEC)
	@echo "Génération en mode ThreadSanitizer : $(EXEC)"
	@echo "Lance avec : TSAN_OPTIONS=suppressions=tsan_suppressions.txt ./$(EXEC) bench 14 4"

release-nolto: $(EXE)
	@mv $(EXE) $(EXEC)
	@echo "Génération en mode release sans LTO : $(EXEC)"

release-autoinit: $(EXE)
	@mv $(EXE) $(EXEC)
	@echo "Génération release-nolto + auto-init stack : $(EXEC)"

release-valgrind: $(EXE)
	@mv $(EXE) $(EXEC)
	@echo "Génération release-nolto + -g (sans auto-init, pour valgrind) : $(EXEC)"

release-bounds: $(EXE)
	@mv $(EXE) $(EXEC)
	@echo "Génération release-nolto + UBSan bounds (intra-struct) + FORTIFY : $(EXEC)"
	@echo "Lance avec : UBSAN_OPTIONS=print_stacktrace=1,abort_on_error=1 ./$(EXEC) bench 27"

#---------------------------------------------------------------------
#	PGO (code venant d'Ethereal)
#---------------------------------------------------------------------
pgo:
	@rm -f *.gcda *.profdata *.profraw
	$(CXX) $(PGO_GEN) $(CFLAGS) $(PGO_FLAGS) $(SRC) $(LDFLAGS) -o $(EXE)
	$(PGO_BENCH)
	$(PGO_MERGE)
	$(CXX) $(PGO_USE) $(CFLAGS) $(PGO_FLAGS) $(SRC) $(LDFLAGS) -o $(EXE)
	@rm -f *.gcda *.profdata *.profraw
	@mv $(EXE) $(EXEC)
	@cp $(EXEC) $(ZANGDAR_DEV)
	@echo "Génération en mode pgo : $(EXEC)"

#----------------------------------------------------------------------

$(EXE): $(OBJ)
	@$(CXX) -o $@ $^ $(LDFLAGS)

src/NNUE.o: $(EVALFILE)

# -MMD : génère, à côté de chaque .o, un .d listant les headers dont il dépend.
#        (MMD et non MD : on ignore les headers système, qui ne bougent pas.)
# -MP  : ajoute une cible bidon pour chaque header. Indispensable ici : sans lui,
#        supprimer ou renommer un header (typiquement en changeant de branche)
#        casse le build avec « No rule to make target 'src/xxx.h' » au lieu de
#        simplement recompiler.
%.o: %.cpp
	@$(CXX) -o $@ -c $< $(CFLAGS) -MMD -MP

# Prise en compte des dépendances calculées au build précédent.
# Le « - » les rend optionnelles (premier build : aucun .d n'existe encore).
-include $(DEP)

.DELETE_ON_ERROR:

$(NETS_FILE):
	@mkdir -p $(NETS_DIR)
	$(info Downloading network $(NET_NAME))
	curl -sSfL -o $@ $(NETS_REPO)/$(NET_NAME)/$(NET_NAME)

download-net: $(NETS_FILE)

#---------------------------------------------------------------------

# Nettoie TOUTES les architectures, pas seulement l'ARCH courant : $(EXE) dépend
# de ARCH, donc « make clean » seul ne voyait que les binaires natifs et laissait
# traîner les Zangdar-<version>-avx2/bmi2/sse2-*. Le glob Zangdar-* les couvre
# tous (il n'attrape ni Zangdar.pro ni Zangdar_dev, traités à part).
clean:
	@rm -f $(OBJ) $(DEP) $(ZANGDAR_DEV)
	@rm -f $(ZANGDAR)-*
	@rm -f *.gcda *.profdata *.profraw

mrproper: clean
	@rm -rf $(EXE)*

