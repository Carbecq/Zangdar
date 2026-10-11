CXX = clang++

.DEFAULT_GOAL := openbench

ifeq ($(findstring clang, $(CXX)), clang)
	COMP := clang
else
	COMP := g++
endif

# trié ; wildcard plutôt que find : ne dépend pas du shell (cmd sous Windows)
CPP_FILES = $(sort $(wildcard src/*.cpp src/*/*.cpp))
OBJ       = $(patsubst %.cpp, %.o, $(CPP_FILES))

### Establish the operating system name
KERNEL := $(shell uname -s)

### Target Windows OS
ifeq ($(OS),Windows_NT)
	target_windows = yes
else ifeq ($(COMP),mingw)
	target_windows = yes
endif

#---------------------------------------------------
#  Version et nom du réseau (VERSION.txt)
#---------------------------------------------------
include VERSION.txt

#---------------------------------------------------
#  Tables Syzygy locales : commande UCI « yyy » et datagen
#  (à changer sur une autre machine)
#  Répertoires séparés par « ; » sous Windows, « : » sous Unix, sans espace
#---------------------------------------------------
ifeq ($(target_windows),yes)
    SYZYGY = D:/Echecs/Syzygy/345;D:/Echecs/Syzygy/6
else
    SYZYGY = /mnt/Datas/Echecs/Syzygy/345:/mnt/Datas/Echecs/Syzygy/6
endif
DEFS += -DSYZYGY='"$(SYZYGY)"'

DEFS += $(EXTRA_DEFS)

#---------------------------------------------------
#  Tuning (SPSA) : paramètres réglables par UCI
#  à décommenter, ou make EXTRA_DEFS=-DUSE_TUNING
#---------------------------------------------------
#EXTRA_DEFS = -DUSE_TUNING

#---------------------------------------------------
#  Debug : traces
#    DEBUG_LOG  : recherche et threads, dans un fichier log
#    DEBUG_TIME : gestion du temps, sur la sortie standard
#  à décommenter, ou make EXTRA_DEFS="-DDEBUG_LOG -DDEBUG_TIME"
#---------------------------------------------------
#EXTRA_DEFS = -DDEBUG_LOG
#EXTRA_DEFS = -DDEBUG_TIME

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
# Noyaux SIMD de simd.h (AVX-512, AVX2, SSE2 ou NEON selon la cible).
# SIMD=no : code scalaire, avec n'importe quel ARCH ; référence, doit donner le même bench
SIMD ?= yes
CFLAGS_ARCH = $(if $(filter yes,$(SIMD)),-DUSE_SIMD)
ZANGDAR     = Zangdar
ZANGDAR_DEV = Zangdar_dev$(SUF)

# Capacités du CPU hôte, lues dans les macros du compilateur (Linux, Windows, macOS, ARM)
HOST_DEFINES := $(shell echo | $(CXX) -march=native -dM -E - 2>/dev/null)
HAS_SSE2     := $(if $(findstring __SSE2__,$(HOST_DEFINES)),yes)
HAS_SSE41    := $(if $(findstring __SSE4_1__,$(HOST_DEFINES)),yes)
HAS_POPCNT   := $(if $(findstring __POPCNT__,$(HOST_DEFINES)),yes)
HAS_AVX2     := $(if $(findstring __AVX2__,$(HOST_DEFINES)),yes)
HAS_AVX512   := $(if $(findstring __AVX512F__,$(HOST_DEFINES)),yes)
HAS_BMI2     := $(if $(findstring __BMI2__,$(HOST_DEFINES)),yes)
# AMD Zen 1/2 (et Hygon) : pext microcodé, très lent
SLOW_PEXT    := $(if $(or $(findstring __znver1__,$(HOST_DEFINES)),$(findstring __znver2__,$(HOST_DEFINES))),yes)

# Guide des cibles ARCH publiées
#   sse2    : kernel NNUE SSE2, magics. Tout x86-64, même sans popcnt (avant 2008), et AMD K10/Phenom
#   sse41   : sse2 + SSSE3, SSE4.1/4.2, popcnt (x86-64-v2) : Intel Nehalem à Ivy Bridge, AMD Bulldozer,
#              Pentium/Celeron/Atom sans AVX2. Pas AMD K10/Phenom (SSE4a seulement : sse2).
#              La L1 scalaire s'y vectorise : −40 % d'instructions contre sse2
#   avx2    : kernel AVX2, magics. Le standard 2013+ (Intel Haswell+, et AMD Zen 1/2 !)
#   bmi2    : kernel AVX2 + attaques PEXT. UNIQUEMENT Intel Haswell+ et AMD Zen 3+.
#              PIÈGE : Zen 1/Zen 2 annoncent BMI2 mais leur pext est microcodé
#   avx512  : kernel AVX-512 + PEXT. Zen 4/5 ; Intel Skylake-X, Ice/Tiger/Rocket Lake, Xeon (pas Alder Lake+)
#   vnni512 : avx512 + VNNI, L1 int8 en 1 instruction (dpbusd). Seulement sur matériel VNNI (Zen 4/5, Ice Lake-SP+)
#   native  : auto-détection du CPU hôte (pour compiler soi-même).

# options x86 cumulatives ; SSE2 est dans le socle x86-64, -mavx2 implique SSE3 à AVX
FLAGS_SSE41  = -mpopcnt -mssse3 -msse4.1 -msse4.2
FLAGS_AVX2   = $(FLAGS_SSE41) -mavx2 -mbmi
FLAGS_BMI2   = $(FLAGS_AVX2) -mbmi2
FLAGS_AVX512 = $(FLAGS_BMI2) -mavx512bw -mavx512dq -mavx512vl

DEFAULT_EXE = $(ZANGDAR)-$(VERSION)-$(ARCH)

ifeq ($(ARCH), sse2)
    # socle x86-64 : rien à ajouter

else ifeq ($(ARCH), sse41)
    CFLAGS_ARCH += $(FLAGS_SSE41)

else ifeq ($(ARCH), avx2)
    CFLAGS_ARCH += $(FLAGS_AVX2)

else ifeq ($(ARCH), bmi2)
    CFLAGS_ARCH += $(FLAGS_BMI2) -DUSE_PEXT

else ifeq ($(ARCH), avx512)
    CFLAGS_ARCH += $(FLAGS_AVX512) -DUSE_PEXT

else ifeq ($(ARCH), vnni512)
    CFLAGS_ARCH += $(FLAGS_AVX512) -mavx512vnni -DUSE_PEXT

else ifeq ($(ARCH), arm64)
    # Cross-compilation Linux aarch64 : make ARCH=arm64 CXX=aarch64-linux-gnu-g++
    CFLAGS_ARCH += -march=armv8-a

else
    # native, ARCH vide ou inconnu : auto-détection du CPU hôte
    ifeq ($(COMP)$(KERNEL),mingwLinux)
        $(error COMP=mingw exige ARCH=sse2|sse41|avx2|bmi2|avx512|vnni512 : -march=native viserait cette machine)
    endif
    ARCH = native
    DEFAULT_EXE = $(ZANGDAR)-$(VERSION)
    CFLAGS_ARCH += -march=native
    ifeq ($(HAS_BMI2):$(SLOW_PEXT), yes:)
        CFLAGS_ARCH += -DUSE_PEXT
    endif

endif

$(info Version   = $(VERSION))
$(info CXX       = $(CXX))
$(info ARCH      = $(ARCH))
# Capacités du CPU HÔTE (informatif ; ne décrit la cible que si ARCH=native)
$(info SSE2      = $(if $(HAS_SSE2),yes,no)   (hôte))
$(info SSE4.1    = $(if $(HAS_SSE41),yes,no)   (hôte))
$(info POPCNT    = $(if $(HAS_POPCNT),yes,no)   (hôte))
$(info AVX2      = $(if $(HAS_AVX2),yes,no)   (hôte))
$(info AVX512    = $(if $(HAS_AVX512),yes,no)   (hôte))
$(info BMI2      = $(if $(HAS_BMI2),yes,no)$(if $(and $(HAS_BMI2),$(SLOW_PEXT)), (pext microcodé))   (hôte))
# Ce qui est RÉELLEMENT compilé : lu dans CFLAGS_ARCH, pas déduit de l'hôte.
$(info SIMD      = $(if $(findstring USE_SIMD,$(CFLAGS_ARCH)),yes,no))
$(info PEXT      = $(if $(findstring USE_PEXT,$(CFLAGS_ARCH)),yes,no))
$(info OS        = $(KERNEL))
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
#---------------------------------------------------------------------

ifeq ($(findstring clang, $(CXX)), clang)

CFLAGS_REL1  = -O3 -flto -fno-exceptions -DNDEBUG -Wdisabled-optimization -Wall -Wextra \
               -fno-rtti
CFLAGS_WARN1 = -Wmissing-declarations -Wredundant-decls -Wshadow -Wundef -pedantic

CFLAGS_WARN2 = -Wcast-align -Wcast-qual -Wswitch-default -Wswitch-enum -Wenum-conversion
CFLAGS_WARN3 = -Wformat=2 -Winit-self -Wmissing-include-dirs
CFLAGS_WARN4 =
CFLAGS_WARN5 = -Wsign-promo -Wno-unused
CFLAGS_WARN6 = -Wfloat-equal -Wmissing-prototypes -Wconditional-uninitialized

PGO_GEN   = -fprofile-instr-generate
PGO_MERGE = llvm-profdata merge -output=zangdar.profdata *.profraw
PGO_USE   = -fprofile-instr-use=zangdar.profdata

else

CFLAGS_REL1  = -O3 -flto=auto -funroll-loops -fno-exceptions -DNDEBUG -Wdisabled-optimization -Wall -Wextra \
               -fno-rtti
CFLAGS_WARN1 = -Wmissing-declarations -Wredundant-decls -Wshadow -Wundef -pedantic

CFLAGS_WARN2 = -Wcast-align=strict -Wcast-qual -Wctor-dtor-privacy -Wswitch-default -Wswitch-enum -Wenum-conversion
CFLAGS_WARN3 = -Wformat=2 -Winit-self -Wlogical-op -Wmissing-include-dirs
CFLAGS_WARN4 = -Wnoexcept -Woverloaded-virtual -Warray-bounds=2
CFLAGS_WARN5 = -Wsign-promo -Wstrict-null-sentinel -Wno-unused
CFLAGS_WARN6 = -Wfloat-equal -Wbidi-chars -Warray-compare

PGO_GEN   = -fprofile-generate
PGO_USE   = -fprofile-use

endif

# Pile : 16 Mo fixés au lien sous Windows ; sous Linux elle vient du shell (ulimit -s)
LDFLAGS_STACK =
ifeq ($(target_windows),yes)
    LDFLAGS_STACK = -Wl,--stack,16777216
endif

# -ffp-contract=off : pas de FMA implicite, l'éval flottante est la même sur toutes les cibles
# -pthread : toutes les cibles, compilation et lien (CFLAGS est repris au lien)
CFLAGS_COM  = -pipe -std=c++23 -pthread -ffp-contract=off -DVERSION=\"$(VERSION)\" $(DEFS) $(CFLAGS_NNUE)
CFLAGS_REL  = $(CFLAGS_REL1) $(CFLAGS_WARN1)
CFLAGS_DBG  = -g -O2
# -Wall -Wextra en tête : le -Wno-unused de CFLAGS_WARN5 doit rester le dernier mot
CFLAGS_WARN = -Wall -Wextra $(CFLAGS_WARN1) $(CFLAGS_WARN2) $(CFLAGS_WARN3) $(CFLAGS_WARN4) $(CFLAGS_WARN5) $(CFLAGS_WARN6)
CFLAGS_PERF = $(CFLAGS_REL1) $(CFLAGS_WARN1) -g -fno-omit-frame-pointer -DUSE_PROFILING

# ASan + UBSan, sans LTO ni lien statique ; arrêt à la première erreur
CFLAGS_SAN  = -g -O1 -fno-omit-frame-pointer -fno-optimize-sibling-calls \
              -fsanitize=undefined,address -fno-sanitize-recover=all \
              $(CFLAGS_WARN1)

# TSan (incompatible avec ASan) ; les courses voulues du Lazy SMP sont dans tsan_suppressions.txt
CFLAGS_TSAN = -g -O2 -fno-omit-frame-pointer \
              -fsanitize=thread \
              $(CFLAGS_WARN1)

# Release sans LTO : isole les UB démasquées par l'optimisation inter-modules
CFLAGS_REL_NOLTO = $(filter-out -flto -flto=auto, $(CFLAGS_REL1)) \
                   $(CFLAGS_WARN1)

# -flto et -fsanitize arrivent au lien par CFLAGS
LDFLAGS_REL  = -s

#---------------------------------------------------------------------
#   Targets
#---------------------------------------------------------------------

# Statique par défaut (Windows, Linux) : un binaire Linux dynamique exige la glibc
# de la machine qui l'a compilé. STATIC=no pour un lien dynamique. Jamais pour ASan/TSan.
STATIC ?= yes
LDFLAGS_STATIC =

ifeq ($(target_windows),yes)
    LDFLAGS_STATIC += -static
else ifeq ($(KERNEL)$(STATIC),Linuxyes)
    LDFLAGS_STATIC += -static
endif

ifneq ($(filter sanitize sanitize-thread,$(MAKECMDGOALS)),)
    LDFLAGS_STATIC =
endif

$(info Static    = $(if $(LDFLAGS_STATIC),yes,no))

EXE  = $(DEFAULT_EXE)-$(COMP)$(if $(filter yes,$(SIMD)),,-nosimd)

openbench: EXEC    = $(EXE)$(SUF)
openbench: CFLAGS  = $(CFLAGS_COM) $(CFLAGS_ARCH) $(CFLAGS_REL)
openbench: LDFLAGS = $(LDFLAGS_REL) $(LDFLAGS_STATIC) $(LDFLAGS_STACK)

debug: EXEC    = $(EXE)-dbg$(SUF)
debug: CFLAGS  = $(CFLAGS_COM) $(CFLAGS_ARCH) $(CFLAGS_WARN) $(CFLAGS_DBG)
debug: LDFLAGS = $(LDFLAGS_STATIC) $(LDFLAGS_STACK)

sanitize: EXEC    = $(EXE)-san$(SUF)
sanitize: CFLAGS  = $(CFLAGS_COM) $(CFLAGS_ARCH) $(CFLAGS_SAN)
sanitize: LDFLAGS =

sanitize-thread: EXEC    = $(EXE)-tsan$(SUF)
sanitize-thread: CFLAGS  = $(CFLAGS_COM) $(CFLAGS_ARCH) $(CFLAGS_TSAN)
sanitize-thread: LDFLAGS =

release-nolto: EXEC    = $(EXE)-rel-nolto$(SUF)
release-nolto: CFLAGS  = $(CFLAGS_COM) $(CFLAGS_ARCH) $(CFLAGS_REL_NOLTO)
release-nolto: LDFLAGS = $(LDFLAGS_REL) $(LDFLAGS_STATIC) $(LDFLAGS_STACK)

# valgrind memcheck : sans LTO, -g, non strippé
release-valgrind: EXEC    = $(EXE)-rel-vg$(SUF)
release-valgrind: CFLAGS  = $(CFLAGS_COM) $(CFLAGS_ARCH) $(CFLAGS_REL_NOLTO) -g
release-valgrind: LDFLAGS = $(LDFLAGS_STATIC) $(LDFLAGS_STACK)

# Indices hors bornes, y compris dans une structure (invisibles pour ASan et valgrind)
release-bounds: EXEC    = $(EXE)-rel-bounds$(SUF)
release-bounds: CFLAGS  = $(CFLAGS_COM) $(CFLAGS_ARCH) $(CFLAGS_REL_NOLTO) -g -fsanitize=bounds,object-size -fno-sanitize-recover=all -D_FORTIFY_SOURCE=3
release-bounds: LDFLAGS = $(LDFLAGS_STATIC) $(LDFLAGS_STACK)

perf: EXEC    = $(EXE)-perf$(SUF)
perf: CFLAGS  = $(CFLAGS_COM) $(CFLAGS_ARCH) $(CFLAGS_PERF)
perf: LDFLAGS = $(LDFLAGS_STATIC) $(LDFLAGS_STACK)

pgo: EXEC    = $(EXE)-pgo$(SUF)
pgo: CFLAGS  = $(CFLAGS_COM) $(CFLAGS_ARCH) $(CFLAGS_REL)
pgo: LDFLAGS = $(LDFLAGS_REL) $(LDFLAGS_STATIC) $(LDFLAGS_STACK)

PGO_BENCH = ./$(EXEC) bench 20 > /dev/null 2>&1

.PHONY: openbench debug sanitize sanitize-thread \
        release-nolto release-valgrind release-bounds \
        perf pgo download-net clean

# CFLAGS au lien : la LTO reprend -O3, -march et -pthread (CFLAGS_COM)
openbench perf debug: $(OBJ)
	@$(CXX) -o $(EXEC) $^ $(CFLAGS) $(LDFLAGS)
	@cp $(EXEC) $(ZANGDAR_DEV)
	@echo "Génération $@ : $(EXEC)"

sanitize sanitize-thread release-nolto release-valgrind release-bounds: $(OBJ)
	@$(CXX) -o $(EXEC) $^ $(CFLAGS) $(LDFLAGS)
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
	$(CXX) $(PGO_GEN) $(CFLAGS) $(CPP_FILES) $(LDFLAGS) -o $(EXEC)
	$(PGO_BENCH)
	$(PGO_MERGE)
	$(CXX) $(PGO_USE) $(CFLAGS) $(CPP_FILES) $(LDFLAGS) -o $(EXEC)
	@rm -f *.gcda *.profdata *.profraw
	@cp $(EXEC) $(ZANGDAR_DEV)
	@echo "Génération en mode pgo : $(EXEC)"

#----------------------------------------------------------------------

src/NNUE.o: $(EVALFILE)

# pyrrhic (Fathom) est du code TIERS, on ne le modifie pas.
src/pyrrhic/tbprobe.o: CFLAGS += -Wno-cast-align -Wno-undef

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
	@rm -f Zangdar_dev Zangdar_dev.exe
	@rm -f $(ZANGDAR)-*
	@rm -f *.gcda *.profdata *.profraw
