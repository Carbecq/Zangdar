DEBUG=no
PROF=no
#CXX=clang++-18		# 18.1.3
CXX=g++-14		# 14.0.1
SRC1=src
SRC2=src/pyrrhic

FILE    := VERSION.txt
VERSION := $(file < $(FILE))

TARGET = Zangdar-$(VERSION)-5950X-pext


# needed only for tests
# just comment it if you want
#HOMEDEF = -DHOME=\"/media/philippe/Travail/Echecs/Programmation/Zangdar/APP-2/\"

ifeq ($(findstring clang, $(CXX)), clang)

CFLAGS_COM  = -pipe -std=c++20 -DVERSION=\"$(VERSION)\"
CFLAGS_ARCH = -march=native -DUSE_PEXT
CFLAGS_DEB  =
CFLAGS_OPT  = -O3 -Ofast -flto -DNDEBUG

CFLAGS_WARN1 = -pedantic -Wall -Wextra -Wcast-align -Wcast-qual -Wctor-dtor-privacy -Wdisabled-optimization
CFLAGS_WARN2 = -Wformat=2 -Winit-self -Wmissing-declarations
# -Wmissing-include-dirs -Wsign-conversion -Wold-style-cast
CFLAGS_WARN3 = -Woverloaded-virtual -Wredundant-decls -Wshadow -Wsign-promo
# -Werror
CFLAGS_WARN4 = -Wstrict-overflow=5 -Wswitch-default -Wundef -Wno-unused
CFLAGS_WARN5 = -Wuninitialized -Wattributes -Waddress

else

CFLAGS_COM = -pipe -std=c++20 -DVERSION=\"$(VERSION)\"
CFLAGS_ARCH = -march=native -DUSE_PEXT
CFLAGS_DEB = -g -O0
CFLAGS_OPT = -O3 -flto -DNDEBUG -fwhole-program

CFLAGS_WARN1 = -pedantic -Wall -Wextra -Wcast-align -Wcast-qual -Wctor-dtor-privacy -Wdisabled-optimization
CFLAGS_WARN2 = -Wformat=2 -Winit-self -Wlogical-op -Wmissing-declarations -Wnoexcept
# -Wmissing-include-dirs -Wsign-conversion -Wold-style-cast
CFLAGS_WARN3 = -Woverloaded-virtual -Wredundant-decls -Wshadow -Wsign-promo
# -Werror
CFLAGS_WARN4 = -Wstrict-null-sentinel -Wstrict-overflow=5 -Wswitch-default -Wundef -Wno-unused
CFLAGS_WARN5 = -Wuninitialized -Wbidi-chars -Warray-compare -Wattributes -Waddress

endif

# https://stackoverflow.com/questions/5088460/flags-to-enable-thorough-and-verbose-g-warnings

# -ansi -pedantic -Wall -Wextra -Weffc++


CFLAGS_WARN = $(CFLAGS_WARN1) $(CFLAGS_WARN2) $(CFLAGS_WARN3) $(CFLAGS_WARN4) $(CFLAGS_WARN5)

ifeq ($(PROF),yes)
    CFPROF=-pg
    LDPROF=-pg
else
    CFPROF=
    LDPROF=-s
endif

ifeq ($(DEBUG),yes)
    CFLAGS= $(CFLAGS_COM) $(CFLAGS_DEB) $(CFLAGS_ARCH) $(CFPROF) $(CFLAGS_WARN) -I/$(SRC1) -I/$(SRC2) $(HOMEDEF)
    LDFLAGS=$(LDPROF) -lpthread
else
    CFLAGS= $(CFLAGS_COM) $(CFLAGS_OPT) $(CFLAGS_ARCH) $(CFPROF) $(CFLAGS_WARN) -I/$(SRC1) -I/$(SRC2) $(HOMEDEF)
    LDFLAGS= $(LDPROF) -flto -lpthread -static 
endif


CPP_FILES=$(shell find $(SRC1) -name "*.cpp") $(shell find $(SRC2) -name "*.cpp")

OBJ=$(patsubst %.cpp, %.o, $(CPP_FILES))



all: $(TARGET)
ifeq ($(DEBUG),yes)
    $(info Génération en mode debug)
else
    $(info Génération en mode release)
endif

$(TARGET): $(OBJ)
	@$(CXX) -o $@ $^ $(LDFLAGS)


%.o: %.cpp
	@$(CXX) -o $@ -c $< $(CFLAGS)

.PHONY: clean mrproper

clean:
	@rm -rf $(SRC1)/*.o $(SRC2)/*.o $(TARGET)

mrproper: clean
	@rm -rf $(TARGET)

install:
	mv $(TARGET) ../../BIN/Zangdar

