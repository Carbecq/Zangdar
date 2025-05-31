CXX = clang++
#CXX = g++

ifeq ($(CXX),clang++)
	COMP := clang
else
	COMP := gcc
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

# needed only for tests
# just comment it if you want
HOME = \"/mnt/Datas/Echecs/Programmation/Zangdar/\"
#HOME = \"./\"
DEFS = -DHOME=$(HOME)

ifeq ($(target_windows),yes)
	SYZYGY = \"D:/Echecs/Syzygy\"
else
	SYZYGY = \"/mnt/Datas/Echecs/Syzygy\"
endif
DEFS += -DSYZYGY=$(SYZYGY)

#  Quelques defines utilisés en debug
# DEFS += -DDEBUG_EVAL
# DEFS += -DDEBUG_LOG
# DEFS += -DDEBUG_TIME

#  Affichage "lisible"
#  NE PAS UTILISER PRETTY avec
#       + Arena (score mal affiché)
#       + test STS
#DEFS += -DUSE_PRETTY

#---------------------------------------------------
#  NNUE
#---------------------------------------------------
CFLAGS_NNUE = -DNETWORK=$(NETWORK)

#---------------------------------------------------------------------
#   Architecture
#---------------------------------------------------------------------
ifeq ($(ARCH), )
   ARCH = native
endif

CFLAGS_ARCH =
ZANGDAR     = Zangdar

DEFAULT_EXE = $(ZANGDAR)

ifeq ($(ARCH), avx2)
    DEFAULT_EXE = $(ZANGDAR)-$(VERSION)-$(ARCH)
    CFLAGS_ARCH += -msse -msse2
    CFLAGS_ARCH += -msse3 -mpopcnt
    CFLAGS_ARCH += -msse4.1 -msse4.2 -msse4a
    CFLAGS_ARCH += -mssse3
    CFLAGS_ARCH += -mmmx
    CFLAGS_ARCH += -mavx
    CFLAGS_ARCH += -mavx2 -mfma
    CFLAGS_ARCH += -DUSE_SIMD

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
    CFLAGS_ARCH += -DUSE_PEXT -DUSE_SIMD

else ifeq ($(ARCH), native)
    DEFAULT_EXE = $(ZANGDAR)-$(VERSION)-5950X
    CFLAGS_ARCH += -march=native
    CFLAGS_ARCH += -DUSE_PEXT -DUSE_SIMD

else
    ARCH = native
    DEFAULT_EXE = $(ZANGDAR)-$(VERSION)-5950X
    CFLAGS_ARCH += -march=native
    CFLAGS_ARCH += -DUSE_PEXT -DUSE_SIMD

endif

$(info Version = $(VERSION))
$(info CXX     = $(CXX))
$(info ARCH    = $(ARCH))
$(info OS      = $(OS))
$(info Network = $(NETWORK))

### Executable name
ifeq ($(target_windows),yes)
	ZSUF := .exe
else
	ZSUF :=
endif
EXE := $(DEFAULT_EXE)-$(COMP)$(ZSUF)

#---------------------------------------------------------------------
#   Options de compilation
#	https://gcc.gnu.org/onlinedocs/gcc/Option-Index.html
#	https://clang.llvm.org/docs/DiagnosticsReference.html
#   Test : https://godbolt.org/
#---------------------------------------------------------------------

ifeq ($(findstring clang, $(CXX)), clang)

CFLAGS_REL1  = -O3 -flto=auto -ftree-vectorize -funroll-loops -fno-exceptions -DNDEBUG -pthread -Wdisabled-optimization -Wall -Wextra \
               -finline-functions -fno-rtti -fstrict-aliasing -fomit-frame-pointer
CFLAGS_WARN1 = -Wmissing-declarations -Wredundant-decls -Wshadow -Wundef -Wuninitialized -pedantic

CFLAGS_WARN2 = -Wcast-align=strict -Wcast-qual -Wctor-dtor-privacy
CFLAGS_WARN3 = -Wformat=2 -Winit-self
CFLAGS_WARN4 = -Woverloaded-virtual -Wsign-promo
CFLAGS_WARN5 = -Wstrict-overflow=5 -Wswitch-default -Wno-unused
CFLAGS_WARN6 = -Wattributes -Waddress -Wmissing-prototypes -Wconditional-uninitialized

ifeq ($(target_windows),yes)
	LDFLAGS_STA = -Wl,--stack,16777216	# 16*1014*1024 = 16 Mo
else
#	LDFLAGS_STA = -Wl,-z,stack-size=16777216
#pour Linux : utiliser : ulimit -s <size> ; ou ulimit -s unlimited
endif

PGO_GEN   = -fprofile-instr-generate
PGO_MERGE = llvm-profdata merge -output=zangdar.profdata *.profraw
PGO_USE   = -fprofile-instr-use=zangdar.profdata

else

CFLAGS_REL1  = -O3 -flto=auto -ftree-vectorize -funroll-loops -fno-exceptions -DNDEBUG -pthread -fwhole-program -Wdisabled-optimization -Wall -Wextra \
               -finline-functions -fno-rtti -fstrict-aliasing -fomit-frame-pointer
CFLAGS_WARN1 = -Wmissing-declarations -Wredundant-decls -Wshadow -Wundef -Wuninitialized -pedantic

CFLAGS_WARN2 = -Wcast-align=strict -Wcast-qual -Wctor-dtor-privacy
CFLAGS_WARN3 = -Wformat=2 -Winit-self -Wlogical-op -Wmissing-include-dirs
CFLAGS_WARN4 = -Wnoexcept -Woverloaded-virtual -Warray-bounds=2
CFLAGS_WARN5 = -Wsign-conversion -Wsign-promo -Wstrict-null-sentinel -Wstrict-overflow=5
CFLAGS_WARN6 = -Wswitch-default -Wfloat-equal -Wbidi-chars -Warray-compare -Wattributes -Waddress

PGO_GEN   = -fprofile-generate
PGO_USE   = -fprofile-use

endif

# https://stackoverflow.com/questions/5088460/flags-to-enable-thorough-and-verbose-g-warnings
# CFLAGS  : -fsanitize=undefined -fsanitize=memory and -fsanitize=address and maybe even -fsanitize=thread
# LDFLAGS : -fsanitize=address -static-libsan

CFLAGS_COM  = -pipe -std=c++23 -DVERSION=\"$(VERSION)\" $(DEFS) $(CFLAGS_NNUE)
CFLAGS_REL  = $(CFLAGS_REL1) $(CFLAGS_WARN1) -DNDEBUG
CFLAGS_DBG  = -g -O2
CFLAGS_WARN = $(CFLAGS_WARN1) $(CFLAGS_WARN2) $(CFLAGS_WARN3) $(CFLAGS_WARN4) $(CFLAGS_WARN5) $(CFLAGS_WARN6)
CFLAGS_PROF = -pg -DNDEBUG

LDFLAGS_REL  = -s -flto=auto -lm
LDFLAGS_DBG  = -lm
LDFLAGS_PROF = -pg -lm

PGO_FLAGS = -fno-asynchronous-unwind-tables

#---------------------------------------------------------------------
#   Targets
#---------------------------------------------------------------------

### Executable statique avec Windows
ifeq ($(target_windows),yes)
    LDFLAGS_WIN = -static
endif

release: CFLAGS  = $(CFLAGS_COM) $(CFLAGS_ARCH) $(CFLAGS_REL)
release: LDFLAGS = $(LDFLAGS_REL) $(LDFLAGS_WIN) $(LDFLAGS_STA)

debug: CFLAGS  = $(CFLAGS_COM) $(CFLAGS_ARCH) $(CFLAGS_WARN) $(CFLAGS_DBG)
debug: LDFLAGS = $(LDFLAGS_DBG) $(LDFLAGS_WIN) $(LDFLAGS_STA)

prof: CFLAGS  = $(CFLAGS_COM) $(CFLAGS_ARCH) $(CFLAGS_PROF)
prof: LDFLAGS = $(LDFLAGS_PROF) $(LDFLAGS_WIN) $(LDFLAGS_STA)

pgo: EXE := $(DEFAULT_EXE)-$(COMP)-pgo$(ZSUF)
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

release: $(EXE)
	$(info Génération en mode release)
prof: $(EXE)
	$(info Génération en mode profile)
debug: $(EXE)
	$(info Génération en mode debug)

#---------------------------------------------------------------------
#	PGO (code venant d'Ethereal)
#---------------------------------------------------------------------
pgo:
	rm -f *.gcda *.profdata *.profraw
	$(CXX) $(PGO_GEN) $(CFLAGS) $(PGO_FLAGS) $(SRC) $(LDFLAGS) -o $(EXE)
	$(PGO_BENCH)
	$(PGO_MERGE)
	$(CXX) $(PGO_USE) $(CFLAGS) $(PGO_FLAGS) $(SRC) $(LDFLAGS) -o $(EXE)
	rm -f *.gcda *.profdata *.profraw

#----------------------------------------------------------------------

$(EXE): $(OBJ)
	@$(CXX) -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	@$(CXX) -o $@ -c $< $(CFLAGS)

#---------------------------------------------------------------------

clean:
	@rm -f $(OBJ) $(EXE)
	@rm -f *.gcda *.profdata *.profraw

mrproper: clean
	@rm -rf $(EXE)

install:
	mv $(EXE) ../../BIN/Zangdar

