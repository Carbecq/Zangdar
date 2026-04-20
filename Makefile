CXX = clang++
# CXX = /bin/g++-14

ifeq ($(CXX),clang++)
	COMP := clang
else
	COMP := g++
endif

SRC1 = src
SRC2 = src/pyrrhic

SRC = src/*.cpp src/pyrrhic/*.cpp

CPP_FILES=$(shell find $(SRC1) -name "*.cpp") $(shell find $(SRC2) -name "*.cpp")
OBJ = $(patsubst %.cpp, %.o, $(CPP_FILES))

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

# SIMD activé si au moins SSE2
SIMD =
ifeq ($(HAS_AVX512), yes)
    SIMD = -DUSE_SIMD
else ifeq ($(HAS_AVX2), yes)
    SIMD = -DUSE_SIMD
else ifeq ($(HAS_SSE2), yes)
    SIMD = -DUSE_SIMD
endif

ifeq ($(ARCH), sse2)
    DEFAULT_EXE = $(ZANGDAR)-$(VERSION)-$(ARCH)
    CFLAGS_ARCH += -msse -msse2 -mpopcnt
    CFLAGS_ARCH += $(SIMD)

else ifeq ($(ARCH), avx2)
    DEFAULT_EXE = $(ZANGDAR)-$(VERSION)-$(ARCH)
    CFLAGS_ARCH += -msse -msse2
    CFLAGS_ARCH += -msse3 -mpopcnt
    CFLAGS_ARCH += -msse4.1 -msse4.2 -msse4a
    CFLAGS_ARCH += -mssse3
    CFLAGS_ARCH += -mmmx
    CFLAGS_ARCH += -mavx
    CFLAGS_ARCH += -mavx2 -mfma
    CFLAGS_ARCH += $(SIMD)

else ifeq ($(ARCH), bmi2)
    DEFAULT_EXE = $(ZANGDAR)-$(VERSION)-$(ARCH)
    CFLAGS_ARCH += -msse -msse2
    CFLAGS_ARCH += -msse3 -mpopcnt
    CFLAGS_ARCH += -msse4.1 -msse4.2 -msse4a
    CFLAGS_ARCH += -mssse3
    CFLAGS_ARCH += -mmmx
    CFLAGS_ARCH += -mavx
    CFLAGS_ARCH += -mavx2 -mfma
    CFLAGS_ARCH += -mbmi -mbmi2
    CFLAGS_ARCH += -DUSE_PEXT $(SIMD)

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
$(info SSE2      = $(if $(HAS_SSE2),yes,no))
$(info POPCNT    = $(if $(HAS_POPCNT),yes,no))
$(info AVX2      = $(if $(HAS_AVX2),yes,no))
$(info AVX512    = $(if $(HAS_AVX512),yes,no))
$(info BMI2      = $(if $(HAS_BMI2),yes,no))
$(info SIMD      = $(if $(SIMD),yes,no))
$(info PEXT      = $(if $(HAS_BMI2),yes,no))
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

ifeq ($(target_windows),yes)
    LDFLAGS_STA = -Wl,--stack,16777216	# 16*1014*1024 = 16 Mo
else
    CFLAGS_REL1 += -fwhole-program-vtables
#	LDFLAGS_STA = -Wl,-z,stack-size=16777216
#pour Linux : utiliser : ulimit -s <size> ; ou ulimit -s unlimited
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

LDFLAGS_REL  = -s -flto -lm
LDFLAGS_DBG  = -lm
LDFLAGS_PERF = -pg -flto -lm

PGO_FLAGS = -fno-asynchronous-unwind-tables

#---------------------------------------------------------------------
#   Targets
#---------------------------------------------------------------------

### Executable statique avec Windows
ifeq ($(target_windows),yes)
    LDFLAGS_WIN = -static
endif

EXE  = $(DEFAULT_EXE)-$(COMP)

openbench: EXEC    = $(EXE)$(SUF)
openbench: CFLAGS  = $(CFLAGS_COM) $(CFLAGS_ARCH) $(CFLAGS_REL)
openbench: LDFLAGS = $(LDFLAGS_REL) $(LDFLAGS_WIN) $(LDFLAGS_STA)

release: EXEC    = $(EXE)-rel$(SUF)
release: CFLAGS  = $(CFLAGS_COM) $(CFLAGS_ARCH) $(CFLAGS_REL)
release: LDFLAGS = $(LDFLAGS_REL) $(LDFLAGS_WIN) $(LDFLAGS_STA)

debug: EXEC    = $(EXE)-dbg$(SUF)
debug: CFLAGS  = $(CFLAGS_COM) $(CFLAGS_ARCH) $(CFLAGS_WARN) $(CFLAGS_DBG)
debug: LDFLAGS = $(LDFLAGS_DBG) $(LDFLAGS_WIN) $(LDFLAGS_STA)

perf: EXEC    = $(EXE)-perf$(SUF)
perf: CFLAGS  = $(CFLAGS_COM) $(CFLAGS_ARCH) $(CFLAGS_PERF)
perf: LDFLAGS = $(LDFLAGS_PERF) $(LDFLAGS_WIN) $(LDFLAGS_STA)

pgo: EXEC    = $(EXE)-pgo$(SUF)
pgo: CFLAGS  = $(CFLAGS_COM) $(CFLAGS_ARCH) $(CFLAGS_REL)
pgo: LDFLAGS = $(LDFLAGS_REL) $(LDFLAGS_WIN) $(LDFLAGS_STA)

ifeq ($(target_windows),yes)
    PGO_BENCH = ./$(EXE) bench 20 > nul 2>&1
    PGO_CLEAN = rmdir /s /q $(PGO_DIR)
else
    PGO_BENCH = ./$(EXE) bench 20 > /dev/null 2>&1
    PGO_CLEAN = $(RM) -rf $(PGO_DIR)
endif

#---------------------------------------------------------------------
#	Dépendances
#---------------------------------------------------------------------

# Target pour OpenBench : compile directement vers $(EXE) sans renommage
# OpenBench appelle : make -j EXE=<chemin> [EVALFILE=<réseau>] [CXX=<compilateur>]
openbench: $(EXE)
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

%.o: %.cpp
	@$(CXX) -o $@ -c $< $(CFLAGS)

$(NETS_FILE):
	$(info Downloading network $(NET_NAME))
	curl -sL -o $@ $(NETS_REPO)/$(NET_NAME)/$(NET_NAME)

download-net: $(NETS_FILE)

#---------------------------------------------------------------------

clean:
	@rm -f $(OBJ) $(EXE)* $(ZANGDAR_DEV)
	@rm -f *.gcda *.profdata *.profraw

mrproper: clean
	@rm -rf $(EXE)*

