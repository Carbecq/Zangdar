#CXX = clang++-18		# 18.1.3
CXX = g++-14		# 14.0.1

SRC1 = src
SRC2 = src/pyrrhic

SRC = src/*.cpp src/pyrrhic/*.cpp

CPP_FILES=$(shell find $(SRC1) -name "*.cpp") $(shell find $(SRC2) -name "*.cpp")
OBJ = $(patsubst %.cpp, %.o, $(CPP_FILES))

FILE    := VERSION.txt
VERSION := $(file < $(FILE))

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

ifeq ($(ARCH), x86-64)
	EXE = Zangdar-$(VERSION)-$(ARCH)
	CFLAGS_ARCH += -msse -msse2

else ifeq ($(ARCH), popcnt)
	EXE = Zangdar-$(VERSION)-$(ARCH)
	CFLAGS_ARCH += -msse -msse2			## x86-64
	CFLAGS_ARCH += -msse3 -mpopcnt			## popcount
	CFLAGS_ARCH += -msse4.1 -msse4.2 -msse4a	## sse 4.1
	CFLAGS_ARCH += -mssse3				## ssse3
	CFLAGS_ARCH += -mmmx				## mmx
	CFLAGS_ARCH += -mavx				## avx

else ifeq ($(ARCH), avx2)
	EXE = Zangdar-$(VERSION)-$(ARCH)
	CFLAGS_ARCH += -msse -msse2			## x86-64
	CFLAGS_ARCH += -msse3 -mpopcnt			## popcount
	CFLAGS_ARCH += -msse4.1 -msse4.2 -msse4a	## sse 4.1
	CFLAGS_ARCH += -mssse3				## ssse3
	CFLAGS_ARCH += -mmmx				## mmx
	CFLAGS_ARCH += -mavx				## avx
	CFLAGS_ARCH += -mavx2 -mfma			## avx2

else ifeq ($(ARCH), bmi2-nopext)
	EXE = Zangdar-$(VERSION)-$(ARCH)
	CFLAGS_ARCH += -msse -msse2			## x86-64
	CFLAGS_ARCH += -msse3 -mpopcnt			## popcount
	CFLAGS_ARCH += -msse4.1 -msse4.2 -msse4a	## sse 4.1
	CFLAGS_ARCH += -mssse3				## ssse3
	CFLAGS_ARCH += -mmmx				## mmx
	CFLAGS_ARCH += -mavx				## avx
	CFLAGS_ARCH += -mavx2 -mfma			## avx2
	CFLAGS_ARCH += -mbmi -mbmi2			## bmi2

else ifeq ($(ARCH), bmi2-pext)
	EXE = Zangdar-$(VERSION)-$(ARCH)
	CFLAGS_ARCH += -msse -msse2			## x86-64
	CFLAGS_ARCH += -msse3 -mpopcnt			## popcount
	CFLAGS_ARCH += -msse4.1 -msse4.2 -msse4a	## sse 4.1
	CFLAGS_ARCH += -mssse3				## ssse3
	CFLAGS_ARCH += -mmmx				## mmx
	CFLAGS_ARCH += -mavx				## avx
	CFLAGS_ARCH += -mavx2 -mfma			## avx2
	CFLAGS_ARCH += -mbmi -mbmi2 -DUSE_PEXT		## bmi2

else ifeq ($(ARCH), native-nopext)
	EXE = Zangdar-$(VERSION)-5950X-nopext
	CFLAGS_ARCH += -march=native			## native

else ifeq ($(ARCH), native-pext)
	EXE = Zangdar-$(VERSION)-5950X-pext
	CFLAGS_ARCH += -march=native -DUSE_PEXT		## native

endif

$(info ARCH="$(ARCH)")

#---------------------------------------------------------------------
#   Options de compilation
#	https://gcc.gnu.org/onlinedocs/gcc/Option-Index.html
#	https://clang.llvm.org/docs/DiagnosticsReference.html
#---------------------------------------------------------------------

ifeq ($(findstring clang, $(CXX)), clang)

CFLAGS_COM  = -pipe -std=c++20 -DVERSION=\"$(VERSION)\" $(DEFS)
CFLAGS_OPT  = -O3 -flto=auto -DNDEBUG -pthread
CFLAGS_DBG  = -g -O0

CFLAGS_WARN1 = -pedantic -Wall -Wextra -Wcast-align -Wcast-qual -Wctor-dtor-privacy -Wdisabled-optimization
CFLAGS_WARN2 = -Wformat=2 -Winit-self -Wmissing-declarations
CFLAGS_WARN3 = -Woverloaded-virtual -Wredundant-decls -Wshadow -Wsign-promo
CFLAGS_WARN4 = -Wstrict-overflow=5 -Wswitch-default -Wundef -Wno-unused
CFLAGS_WARN5 = -Wuninitialized -Wattributes -Waddress -Wmissing-prototypes -Wconditional-uninitialized

PGO_MERGE = llvm-profdata-18 merge -output=zangdar.profdata *.profraw
PGO_GEN   = -fprofile-instr-generate
PGO_USE   = -fprofile-instr-use=zangdar.profdata

else

CFLAGS_COM  = -pipe -std=c++20 -DVERSION=\"$(VERSION)\" $(DEFS)
CFLAGS_OPT  = -O3 -Ofast -flto=auto -DNDEBUG -pthread -fwhole-program
CFLAGS_DBG  = -g -O0

CFLAGS_WARN1 = -pedantic -Wall -Wextra -Wcast-align -Wcast-qual -Wctor-dtor-privacy -Wdisabled-optimization
CFLAGS_WARN2 = -Wformat=2 -Winit-self -Wlogical-op -Wmissing-declarations -Wnoexcept
CFLAGS_WARN3 = -Woverloaded-virtual -Wredundant-decls -Wshadow -Wsign-promo
CFLAGS_WARN4 = -Wstrict-null-sentinel -Wstrict-overflow=5 -Wswitch-default -Wundef -Wno-unused
CFLAGS_WARN5 = -Wuninitialized -Wbidi-chars -Warray-compare -Wattributes -Waddress

PGO_GEN   = -fprofile-generate
PGO_USE   = -fprofile-use

endif

# Try to detect windows environment by seeing
# whether the shell filters out " or not.
ifeq ($(shell echo "test"), "test")
	PGO_BENCH = $(EXE) bench 16 > nul 2>&1
	PGO_CLEAN = rmdir /s /q $(PGO_DIR)
else
	PGO_BENCH = ./$(EXE) bench 16 > /dev/null 2>&1
	PGO_CLEAN = $(RM) -rf $(PGO_DIR)
endif

# https://stackoverflow.com/questions/5088460/flags-to-enable-thorough-and-verbose-g-warnings
# -ansi -pedantic -Wall -Wextra -Weffc++


CFLAGS_WARN = $(CFLAGS_WARN1) $(CFLAGS_WARN2) $(CFLAGS_WARN3) $(CFLAGS_WARN4) $(CFLAGS_WARN5)
CFLAGS_PROF = -pg
CFLAGS_TUNE = -fopenmp -DUSE_TUNER

LDFLAGS_OPT  = -s -flto=auto
LDFLAGS_DBG  = -lm
LDFLAGS_PROF = -pg
LDFLAGS_TUNE = -fopenmp

CFLAGS  = $(CFLAGS_COM) $(CFLAGS_ARCH) $(CFLAGS_WARN) $(CFLAGS_OPT)
LDFLAGS = $(LDFLAGS_OPT)

PGO_FLAGS = -fno-asynchronous-unwind-tables

#---------------------------------------------------------------------
#   Targets
#---------------------------------------------------------------------

basic: CFLAGS  = $(CFLAGS_COM) $(CFLAGS_ARCH) $(CFLAGS_WARN) $(CFLAGS_OPT)
basic: LDFLAGS = $(LDFLAGS_OPT)

debug: CFLAGS  = $(CFLAGS_COM) $(CFLAGS_ARCH) $(CFLAGS_WARN) $(CFLAGS_DBG)
debug: LDFLAGS = $(LDFLAGS_DBG)

prof: CFLAGS  = $(CFLAGS_COM) $(CFLAGS_ARCH) $(CFLAGS_WARN) $(CFLAGS_PROF)
prof: LDFLAGS = $(LDFLAGS_PROF)

tune: CFLAGS  = $(CFLAGS_COM) $(CFLAGS_ARCH) $(CFLAGS_WARN) $(CFLAGS_OPT) $(CFLAGS_TUNE)
tune: LDFLAGS = $(LDFLAGS_OPT) $(LDFLAGS_TUNE)

pgo: CFLAGS  = $(CFLAGS_COM) $(CFLAGS_ARCH) $(CFLAGS_WARN) $(CFLAGS_OPT)
pgo: LDFLAGS = $(LDFLAGS_OPT)

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

#---------------------------------------------------------------------
#	Dépendances
#---------------------------------------------------------------------

all: $(EXE)

basic: $(EXE)
	$(info Génération en mode basic)
prof: $(EXE)
	$(info Génération en mode profile)
tune: $(EXE)
	$(info Génération en mode tuning)
debug: $(EXE)
	$(info Génération en mode debug)

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

