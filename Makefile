CXX = clang++
# CXX = /bin/g++-14

.DEFAULT_GOAL := openbench

ifeq ($(findstring clang, $(CXX)), clang)
	COMP := clang
else
	COMP := g++
endif

# trié : l'ordre de find dépend du système de fichiers
CPP_FILES = $(sort $(shell find src -name '*.cpp'))
OBJ       = $(patsubst %.cpp, %.o, $(CPP_FILES))

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

EVALFILE ?= $(NETS_FILE)
CFLAGS_NNUE = -DEVALFILE=\"$(EVALFILE)\" -DNET_NAME=\"$(NET_NAME)\"

#---------------------------------------------------------------------
#   Architecture
#---------------------------------------------------------------------
# kernel SIMD partout : simd.h choisit AVX-512, AVX2, SSE2 ou NEON selon la cible
CFLAGS_ARCH = -DUSE_SIMD
ZANGDAR     = Zangdar
ZANGDAR_DEV = Zangdar_dev

# Détection du CPU local
CPUINFO = /proc/cpuinfo
CPU_MODEL := $(shell grep -m1 'model name' $(CPUINFO) 2>/dev/null | sed 's/.*: //;s/(R)//g;s/(TM)//g;s/CPU //;s/ [0-9]*-Core.*//;s/Processor//;s/  */ /g;s/ *$$//;s/ /-/g;s/[^a-zA-Z0-9_-]//g')
ifeq ($(CPU_MODEL),)
    CPU_MODEL := unknown
endif

# Détection des capacités CPU
HAS_SSE2   := $(shell grep -qm1 'sse2'    $(CPUINFO) 2>/dev/null && echo yes)
HAS_POPCNT := $(shell grep -qm1 'popcnt'  $(CPUINFO) 2>/dev/null && echo yes)
HAS_AVX2   := $(shell grep -qm1 'avx2'    $(CPUINFO) 2>/dev/null && echo yes)
HAS_AVX512 := $(shell grep -qm1 'avx512f' $(CPUINFO) 2>/dev/null && echo yes)
HAS_BMI2   := $(shell grep -qm1 'bmi2'    $(CPUINFO) 2>/dev/null && echo yes)
# AMD avant Zen 3 (famille < 25) : pext microcodé, très lent
SLOW_PEXT  := $(shell grep -qm1 -E 'AuthenticAMD|HygonGenuine' $(CPUINFO) 2>/dev/null && \
                      [ "$$(grep -m1 'cpu family' $(CPUINFO) | grep -o '[0-9]*')" -lt 25 ] && echo yes)

# Guide des cibles ARCH publiées
#   sse2   : kernel NNUE SSE2, magics. Tout x86-64, même sans popcnt (avant 2008)
#   avx2   : kernel AVX2, magics. Le standard 2013+ (Intel Haswell+, et AMD Zen 1/2 !)
#   bmi2   : kernel AVX2 + attaques PEXT. UNIQUEMENT Intel Haswell+ et AMD Zen 3+.
#            PIÈGE : Zen 1/Zen 2 annoncent BMI2 mais leur pext est microcodé
#   avx512 : kernel AVX-512 + PEXT. Zen 4/5 ; Intel Skylake-X, Ice/Tiger/Rocket Lake, Xeon (pas Alder Lake+)
#   vnni512 : avx512 + VNNI, L1 int8 en 1 instruction (dpbusd). Seulement sur matériel VNNI (Zen 4/5, Ice Lake-SP+)
#   native : auto-détection du CPU hôte (pour compiler soi-même).

# options x86 cumulatives ; SSE2 est dans le socle x86-64, -mavx2 implique SSE3 à AVX
FLAGS_AVX2   = -mpopcnt -mavx2 -mfma
FLAGS_BMI2   = $(FLAGS_AVX2) -mbmi -mbmi2
FLAGS_AVX512 = $(FLAGS_BMI2) -mavx512bw -mavx512dq -mavx512vl

DEFAULT_EXE = $(ZANGDAR)-$(VERSION)-$(ARCH)

ifeq ($(ARCH), sse2)
    # socle x86-64 : rien à ajouter

else ifeq ($(ARCH), avx2)
    CFLAGS_ARCH += $(FLAGS_AVX2)

else ifeq ($(ARCH), bmi2)
    CFLAGS_ARCH += $(FLAGS_BMI2) -DUSE_PEXT

else ifeq ($(ARCH), avx512)
    CFLAGS_ARCH += $(FLAGS_AVX512) -DUSE_PEXT

else ifeq ($(ARCH), vnni512)
    CFLAGS_ARCH += $(FLAGS_AVX512) -mavx512vnni -DUSE_PEXT

else ifeq ($(ARCH), arm64)
    # Cross-compilation Linux aarch64 : make ARCH=arm64 CXX=aarch64-linux-gnu-g++ STATIC=yes
    CFLAGS_ARCH += -march=armv8-a

else
    # native, ARCH vide ou inconnu : auto-détection du CPU hôte
    ARCH = native
    DEFAULT_EXE = $(ZANGDAR)-$(VERSION)-$(CPU_MODEL)
    CFLAGS_ARCH += -march=native
    ifeq ($(HAS_BMI2):$(SLOW_PEXT), yes:)
        CFLAGS_ARCH += -DUSE_PEXT
    endif

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
$(info BMI2      = $(if $(HAS_BMI2),yes,no)$(if $(and $(HAS_BMI2),$(SLOW_PEXT)), (pext microcodé))   (hôte))
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

ifeq ($(findstring clang, $(CXX)), clang)

CFLAGS_REL1  = -O3 -flto -funroll-loops -fno-exceptions -DNDEBUG -pthread -Wdisabled-optimization -Wall -Wextra \
               -fno-rtti
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

CFLAGS_REL1  = -O3 -flto=auto -funroll-loops -fno-exceptions -DNDEBUG -pthread -fwhole-program -Wdisabled-optimization -Wall -Wextra \
               -fno-rtti
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

# -ffp-contract=off : pas de FMA implicite, l'éval flottante est la même sur toutes les cibles
CFLAGS_COM  = -pipe -std=c++23 -ffp-contract=off -DVERSION=\"$(VERSION)\" $(DEFS) $(CFLAGS_NNUE)
CFLAGS_REL  = $(CFLAGS_REL1) $(CFLAGS_WARN1) -fomit-frame-pointer
CFLAGS_DBG  = -g -O2
# -Wall -Wextra en tête : le -Wno-unused de CFLAGS_WARN5 doit rester le dernier mot
CFLAGS_WARN = -Wall -Wextra $(CFLAGS_WARN1) $(CFLAGS_WARN2) $(CFLAGS_WARN3) $(CFLAGS_WARN4) $(CFLAGS_WARN5) $(CFLAGS_WARN6)
CFLAGS_PERF = $(CFLAGS_REL1) $(CFLAGS_WARN1) -g -fno-omit-frame-pointer -DUSE_PROFILING

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
                   $(CFLAGS_WARN1) -fomit-frame-pointer

# Release-nolto + auto-init des variables stack à zéro.
# Si le bench devient déterministe avec ce flag, c'est confirmé que la cause
# du non-déterminisme est une lecture de variable locale non initialisée.
CFLAGS_REL_AUTOINIT = $(CFLAGS_REL_NOLTO) -ftrivial-auto-var-init=zero

LDFLAGS_REL  = -s -flto
LDFLAGS_PERF = -flto
LDFLAGS_SAN  = -fsanitize=undefined,address
LDFLAGS_REL_NOLTO = -s

PGO_FLAGS = -fno-asynchronous-unwind-tables

#---------------------------------------------------------------------
#   Targets
#---------------------------------------------------------------------

### Executable statique.
### - Windows : toujours (pas de DLL runtime à trimballer).
### - Linux (natif ou cross) : à la demande, make STATIC=yes ...
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
debug: LDFLAGS = $(LDFLAGS_STATIC) $(LDFLAGS_STACK)

# Pas de LDFLAGS_STATIC : -static est incompatible avec ASan.
sanitize: EXEC    = $(EXE)-san$(SUF)
sanitize: CFLAGS  = $(CFLAGS_COM) $(CFLAGS_ARCH) $(CFLAGS_SAN)
sanitize: LDFLAGS = $(LDFLAGS_SAN)

sanitize-thread: EXEC    = $(EXE)-tsan$(SUF)
sanitize-thread: CFLAGS  = $(CFLAGS_COM) $(CFLAGS_ARCH) $(CFLAGS_TSAN)
sanitize-thread: LDFLAGS = -fsanitize=thread

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
release-valgrind: LDFLAGS = $(LDFLAGS_STATIC) $(LDFLAGS_STACK)

# Release-nolto + UBSan bounds/object-size + FORTIFY=3 : détecte à l'exécution
# les indices hors bornes Y COMPRIS intra-structure (invisibles pour ASan et
# valgrind) et les memcpy débordant un membre (update_pv). Layout quasi-release
# (pas de shadow memory) => bien moins heisenbug-prone que ASan. Abort + core
# à la 1ère violation.
release-bounds: EXEC    = $(EXE)-rel-bounds$(SUF)
release-bounds: CFLAGS  = $(CFLAGS_COM) $(CFLAGS_ARCH) $(CFLAGS_REL_NOLTO) -g -fsanitize=bounds,object-size -fno-sanitize-recover=all -D_FORTIFY_SOURCE=3
release-bounds: LDFLAGS = $(LDFLAGS_STATIC) $(LDFLAGS_STACK) -fsanitize=bounds,object-size

perf: EXEC    = $(EXE)-perf$(SUF)
perf: CFLAGS  = $(CFLAGS_COM) $(CFLAGS_ARCH) $(CFLAGS_PERF)
perf: LDFLAGS = $(LDFLAGS_PERF) $(LDFLAGS_STATIC) $(LDFLAGS_STACK)

pgo: EXEC    = $(EXE)-pgo$(SUF)
pgo: CFLAGS  = $(CFLAGS_COM) $(CFLAGS_ARCH) $(CFLAGS_REL)
pgo: LDFLAGS = $(LDFLAGS_REL) $(LDFLAGS_STATIC) $(LDFLAGS_STACK)

PGO_BENCH = ./$(EXEC) bench 20 > /dev/null 2>&1

#---------------------------------------------------------------------
#	Dépendances
#---------------------------------------------------------------------

.PHONY: openbench release debug sanitize sanitize-thread \
        release-nolto release-autoinit release-valgrind release-bounds \
        perf pgo download-net clean mrproper

openbench release perf debug: $(OBJ)
	@$(CXX) -o $(EXEC) $^ $(LDFLAGS)
	@cp $(EXEC) $(ZANGDAR_DEV)
	@echo "Génération $@ : $(EXEC)"

sanitize sanitize-thread release-nolto release-autoinit release-valgrind release-bounds: $(OBJ)
	@$(CXX) -o $(EXEC) $^ $(LDFLAGS)
	@echo "Génération $@ : $(EXEC)"
	@$(if $(HINT),echo "Lance avec : $(HINT)")

sanitize:        HINT = UBSAN_OPTIONS=print_stacktrace=1 ASAN_OPTIONS=detect_leaks=0 ./$(EXEC) bench 30 1 128
sanitize-thread: HINT = TSAN_OPTIONS=suppressions=tsan_suppressions.txt ./$(EXEC) bench 14 4
release-bounds:  HINT = UBSAN_OPTIONS=print_stacktrace=1,abort_on_error=1 ./$(EXEC) bench 27

#---------------------------------------------------------------------
#	PGO (code venant d'Ethereal)
#---------------------------------------------------------------------
pgo:
	@rm -f *.gcda *.profdata *.profraw
	$(CXX) $(PGO_GEN) $(CFLAGS) $(PGO_FLAGS) $(CPP_FILES) $(LDFLAGS) -o $(EXEC)
	$(PGO_BENCH)
	$(PGO_MERGE)
	$(CXX) $(PGO_USE) $(CFLAGS) $(PGO_FLAGS) $(CPP_FILES) $(LDFLAGS) -o $(EXEC)
	@rm -f *.gcda *.profdata *.profraw
	@cp $(EXEC) $(ZANGDAR_DEV)
	@echo "Génération en mode pgo : $(EXEC)"

#----------------------------------------------------------------------

src/NNUE.o: $(EVALFILE)

# pyrrhic (Fathom) est du code TIERS, on ne le modifie pas.
src/pyrrhic/tbprobe.o: CFLAGS += -Wno-cast-align -Wno-strict-overflow

%.o: %.cpp
	@$(CXX) -o $@ -c $< $(CFLAGS)

.DELETE_ON_ERROR:

$(NETS_FILE):
	@mkdir -p $(NETS_DIR)
	$(info Downloading network $(NET_NAME))
	curl -sSfL -o $@ $(NETS_REPO)/$(NET_NAME)/$(NET_NAME)

download-net: $(NETS_FILE)

#---------------------------------------------------------------------
# Nettoie TOUTES les architectures, pas seulement l'ARCH courant
clean:
	@find src -name '*.o' -delete
	@rm -f $(ZANGDAR_DEV)
	@rm -f $(ZANGDAR)-*
	@rm -f *.gcda *.profdata *.profraw

# clean efface déjà tous les Zangdar-*
mrproper: clean

