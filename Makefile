CXX = clang++
#CXX = g++

SRC1 = src
SRC2 = src/pyrrhic

SRC = src/*.cpp src/pyrrhic/*.cpp

CPP_FILES=$(shell find $(SRC1) -name "*.cpp") $(shell find $(SRC2) -name "*.cpp")
OBJ = $(patsubst %.cpp, %.o, $(CPP_FILES))

FILE    := VERSION.txt
VERSION := $(file < $(FILE))

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

# needed only for tests
# just comment it if you want
HOMEDEF = \"/mnt/Datas/Echecs/Programmation/Zangdar/\"
DEFS = -DHOME=$(HOMEDEF)

#  Désactiver USE_HASH pour la mesure de performances brutes
DEFS += -DUSE_HASH

# DEFS += -DDEBUG_EVAL
# DEFS += -DDEBUG_LOG
# DEFS += -DDEBUG_HASH
# DEFS += -DDEBUG_TIME

#  NE PAS UTILISER PRETTY avec
#       + Arena (score mal affiché)
#       + test STS
# DEFS += -DUSE_PRETTY

#---------------------------------------------------------------------
#   Architecture
#---------------------------------------------------------------------

ifeq ($(ARCH), )
   ARCH = native-pext
endif

CFLAGS_ARCH =
DEFAULT_EXE = Zangdar

ifeq ($(ARCH), x86-64)
	DEFAULT_EXE = Zangdar-$(VERSION)-$(ARCH)
	CFLAGS_ARCH += -msse -msse2

else ifeq ($(ARCH), popcnt)
	DEFAULT_EXE = Zangdar-$(VERSION)-$(ARCH)
    CFLAGS_ARCH += -msse -msse2
    CFLAGS_ARCH += -msse3 -mpopcnt
    CFLAGS_ARCH += -msse4.1 -msse4.2 -msse4a
    CFLAGS_ARCH += -mssse3
    CFLAGS_ARCH += -mmmx
    CFLAGS_ARCH += -mavx

else ifeq ($(ARCH), avx2)
	DEFAULT_EXE = Zangdar-$(VERSION)-$(ARCH)
    CFLAGS_ARCH += -msse -msse2
    CFLAGS_ARCH += -msse3 -mpopcnt
    CFLAGS_ARCH += -msse4.1 -msse4.2 -msse4a
    CFLAGS_ARCH += -mssse3
    CFLAGS_ARCH += -mmmx
    CFLAGS_ARCH += -mavx
    CFLAGS_ARCH += -mavx2 -mfma

else ifeq ($(ARCH), bmi2-nopext)
	DEFAULT_EXE = Zangdar-$(VERSION)-$(ARCH)
    CFLAGS_ARCH += -msse -msse2
    CFLAGS_ARCH += -msse3 -mpopcnt
    CFLAGS_ARCH += -msse4.1 -msse4.2 -msse4a
    CFLAGS_ARCH += -mssse3
    CFLAGS_ARCH += -mmmx
    CFLAGS_ARCH += -mavx
    CFLAGS_ARCH += -mavx2 -mfma
    CFLAGS_ARCH += -mbmi -mbmi2

else ifeq ($(ARCH), bmi2-pext)
	DEFAULT_EXE = Zangdar-$(VERSION)-$(ARCH)
    CFLAGS_ARCH += -msse -msse2
    CFLAGS_ARCH += -msse3 -mpopcnt
    CFLAGS_ARCH += -msse4.1 -msse4.2 -msse4a
    CFLAGS_ARCH += -mssse3
    CFLAGS_ARCH += -mmmx
    CFLAGS_ARCH += -mavx
    CFLAGS_ARCH += -mavx2 -mfma
    CFLAGS_ARCH += -mbmi -mbmi2 -DUSE_PEXT

else ifeq ($(ARCH), native-nopext)
	DEFAULT_EXE = Zangdar-$(VERSION)-5950X-nopext
	CFLAGS_ARCH += -march=native

else ifeq ($(ARCH), native-pext)
	DEFAULT_EXE = Zangdar-$(VERSION)-5950X-pext
	CFLAGS_ARCH += -march=native -DUSE_PEXT

endif

$(info CXX="$(CXX)")
$(info ARCH="$(ARCH)")
$(info Windows="$(target_windows)")

### Executable name
ifeq ($(target_windows),yes)
	EXE = $(DEFAULT_EXE)-$(CXX).exe
else
	EXE = $(DEFAULT_EXE)-$(CXX)
endif

#---------------------------------------------------------------------
#   Options de compilation
#	https://gcc.gnu.org/onlinedocs/gcc/Option-Index.html
#	https://clang.llvm.org/docs/DiagnosticsReference.html
#---------------------------------------------------------------------

ifeq ($(findstring clang, $(CXX)), clang)

CFLAGS_OPT  = -O3 -flto=auto -DNDEBUG -pthread -Wdisabled-optimization

CFLAGS_WARN1 = -pedantic -Wall -Wextra -Wcast-align -Wcast-qual -Wctor-dtor-privacy
CFLAGS_WARN2 = -Wformat=2 -Winit-self -Wmissing-declarations
CFLAGS_WARN3 = -Woverloaded-virtual -Wredundant-decls -Wshadow -Wsign-promo
CFLAGS_WARN4 = -Wstrict-overflow=5 -Wswitch-default -Wundef -Wno-unused
CFLAGS_WARN5 = -Wuninitialized -Wattributes -Waddress -Wmissing-prototypes -Wconditional-uninitialized

PGO_MERGE = llvm-profdata-18 merge -output=zangdar.profdata *.profraw
PGO_GEN   = -fprofile-instr-generate
PGO_USE   = -fprofile-instr-use=zangdar.profdata

else

CFLAGS_OPT  = -O3 -flto=auto -DNDEBUG -pthread -fwhole-program -Wdisabled-optimization

CFLAGS_WARN1 = -pedantic -Wall -Wextra -Wcast-align -Wcast-qual -Wctor-dtor-privacy 
CFLAGS_WARN2 = -Wformat=2 -Winit-self -Wlogical-op -Wmissing-declarations -Wmissing-include-dirs 
CFLAGS_WARN3 = -Wnoexcept -Woverloaded-virtual -Wredundant-decls -Wshadow 
CFLAGS_WARN4 = -Wsign-conversion -Wsign-promo -Wstrict-null-sentinel -Wstrict-overflow=5 
CFLAGS_WARN5 = -Wswitch-default -Wundef -Wuninitialized -Wfloat-equal -Wbidi-chars -Warray-compare -Wattributes -Waddress
CFLAGS_WARN6 = -Weffc++ -Winline

PGO_GEN   = -fprofile-generate
PGO_USE   = -fprofile-use

endif

ifeq ($(target_windows),yes)
	PGO_BENCH = $(EXE) bench 16 > nul 2>&1
	PGO_CLEAN = rmdir /s /q $(PGO_DIR)
else
	PGO_BENCH = ./$(EXE) bench 16 > /dev/null 2>&1
	PGO_CLEAN = $(RM) -rf $(PGO_DIR)
endif

# https://stackoverflow.com/questions/5088460/flags-to-enable-thorough-and-verbose-g-warnings


CFLAGS_COM  = -pipe -std=c++20 -DVERSION=\"$(VERSION)\" $(DEFS)
CFLAGS_DBG  = -g -O0
CFLAGS_WARN = $(CFLAGS_WARN1) $(CFLAGS_WARN2) $(CFLAGS_WARN3) $(CFLAGS_WARN4) $(CFLAGS_WARN5) $(CFLAGS_WARN6)
CFLAGS_PROF = -pg
CFLAGS_TUNE = -fopenmp -DUSE_TUNER

LDFLAGS_OPT  = -s -flto=auto
LDFLAGS_DBG  = -lm
LDFLAGS_PROF = -pg
LDFLAGS_TUNE = -fopenmp

PGO_FLAGS = -fno-asynchronous-unwind-tables

#---------------------------------------------------------------------
#   Targets
#---------------------------------------------------------------------

release: CFLAGS  = $(CFLAGS_COM) $(CFLAGS_ARCH) $(CFLAGS_WARN) $(CFLAGS_OPT)
release: LDFLAGS = $(LDFLAGS_OPT)

debug: CFLAGS  = $(CFLAGS_COM) $(CFLAGS_ARCH) $(CFLAGS_WARN) $(CFLAGS_DBG)
debug: LDFLAGS = $(LDFLAGS_DBG)

prof: CFLAGS  = $(CFLAGS_COM) $(CFLAGS_ARCH) $(CFLAGS_WARN) $(CFLAGS_PROF)
prof: LDFLAGS = $(LDFLAGS_PROF)

tune: CFLAGS  = $(CFLAGS_COM) $(CFLAGS_ARCH) $(CFLAGS_WARN) $(CFLAGS_OPT) $(CFLAGS_TUNE)
tune: LDFLAGS = $(LDFLAGS_OPT) $(LDFLAGS_TUNE)

pgo: CFLAGS  = $(CFLAGS_COM) $(CFLAGS_ARCH) $(CFLAGS_WARN) $(CFLAGS_OPT)
pgo: LDFLAGS = $(LDFLAGS_OPT)

### Executable statique avec Windows
ifeq ($(target_windows),yes)
	LDFLAGS += -static
endif

#---------------------------------------------------------------------
#	Dépendances
#---------------------------------------------------------------------

release: $(EXE)
	$(info Génération en mode release)
prof: $(EXE)
	$(info Génération en mode profile)
tune: $(EXE)
	$(info Génération en mode tuning)
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

