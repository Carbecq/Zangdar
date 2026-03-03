TEMPLATE = app
CONFIG += console c++23
CONFIG -= app_bundle
CONFIG -= qt

CONFIG += static
QMAKE_LFLAGS += -static -static-libgcc -static-libstdc++ -lstdc++
DEFINES += STATIC

QMAKE_LFLAGS += -flto=auto

#------------------------------------------------------
DEFINES += NETWORK=\\\"/mnt/Datas/Echecs/Programmation/Zangdar/APP/networks/net_5.bin\\\"
DEFINES += USE_SIMD
DEFINES += SYZYGY=\\\"/mnt/Datas/Echecs/Syzygy\\\"
DEFINES += VERSION=\\\"0.00.00\\\"

#------------------------------------------------------
# ATTENTION : ne pas mettre "\" car ils sont reconnus comme caractères spéciaux
# pour Github
HOME_STR = "./"
# pour moi
# HOME_STR = "D:/Echecs/Programmation/Zangdar/"
DEFINES += HOME='\\"$${HOME_STR}\\"'

#------------------------------------------------------
DEFINES += USE_TUNING
#DEFINES += USE_PROFILING
#DEFINES += USE_DATAGEN


#------------------------------------------------------
# DEFINES += DEBUG_EVAL
# DEFINES += DEBUG_LOG
# DEFINES += DEBUG_TIME
# DEFINES += USE_PRETTY

#------------------------------------------------------
# https://gcc.gnu.org/onlinedocs/gcc/x86-Options.html


# si NDEBUG est défini, alors assert ne fait rien

linux-clang {
CFLAGS_REL1  = -O3 -flto=auto -ftree-vectorize -funroll-loops -fno-exceptions -DNDEBUG -pthread -Wdisabled-optimization -Wall -Wextra \
               -finline-functions -fno-rtti -fstrict-aliasing -fomit-frame-pointer
CFLAGS_WARN1 = -Wmissing-declarations -Wredundant-decls -Wshadow -Wundef -Wuninitialized

CFLAGS_WARN2 = -Wcast-align -Wcast-qual -Wctor-dtor-privacy -Wswitch -Wswitch-enum -Wenum-compare -Wenum-conversion
CFLAGS_WARN3 = -Wformat=2 -Winit-self -Wmissing-include-dirs
CFLAGS_WARN4 = -Woverloaded-virtual -Warray-bounds
CFLAGS_WARN5 = -Wsign-promo -Wstrict-overflow=5 -Wno-unused
CFLAGS_WARN6 = -Wswitch-default -Wattributes -Waddress -Wfloat-equal -Wmissing-prototypes -Wconditional-uninitialized
# CFLAGS_WARN7 = -Wsign-conversion -pedantic
}

#------------------------------------------------------
# https://gcc.gnu.org/onlinedocs/gcc/x86-Options.html

linux-g++ {
CFLAGS_REL1  = -O3 -flto=auto -ftree-vectorize -funroll-loops -fno-exceptions -DNDEBUG -pthread -fwhole-program -Wdisabled-optimization -Wall -Wextra \
               -finline-functions -fno-rtti -fstrict-aliasing -fomit-frame-pointer
CFLAGS_WARN1 = -Wmissing-declarations -Wredundant-decls -Wshadow -Wundef -Wuninitialized

CFLAGS_WARN2 = -Wcast-align=strict -Wcast-qual -Wctor-dtor-privacy -Wswitch -Wswitch-enum -Wenum-compare -Wenum-conversion
CFLAGS_WARN3 = -Wformat=2 -Winit-self -Wlogical-op -Wmissing-include-dirs
CFLAGS_WARN4 = -Wnoexcept -Woverloaded-virtual -Warray-bounds=2
CFLAGS_WARN5 = -Wsign-promo -Wstrict-null-sentinel -Wstrict-overflow=5 -Wno-unused
CFLAGS_WARN6 = -Wswitch-default -Wattributes -Waddress -Wfloat-equal -Wbidi-chars -Warray-compare -Wuninitialized
# CFLAGS_WARN7 = -Wsign-conversion -pedantic
}

# https://devblogs.microsoft.com/cppblog/even-more-new-safety-rules-in-c-code-analysis/

TARGET  = native-pext
CFLAGS_ARCH =

##------------------------------------------ 1) old
equals(TARGET, "old"){
TARGET = Zangdar-$${VERSION}-old
CFLAGS_ARCH +=
}

##------------------------------------------ 2) x86-64
equals(TARGET, "x86-64"){
TARGET = Zangdar-$${VERSION}-x86-64

## x86-64
CFLAGS_ARCH += -msse -msse2
}

##------------------------------------------ 3) popcnt
equals(TARGET, "popcnt"){
TARGET = Zangdar-$${VERSION}-popcnt

## x86-64
CFLAGS_ARCH += -msse -msse2
## popcount
CFLAGS_ARCH += -msse3 -mpopcnt
##sse 4.1
CFLAGS_ARCH += -msse4.1 -msse4.2 -msse4a
## ssse3
CFLAGS_ARCH += -mssse3
## mmx
CFLAGS_ARCH += -mmmx
## avx
CFLAGS_ARCH += -mavx
}

##------------------------------------------ 4) avx2 (2013)
equals(TARGET, "avx2"){
TARGET = Zangdar-$${VERSION}-avx2

## x86-64
CFLAGS_ARCH += -msse -msse2
## popcount
CFLAGS_ARCH += -msse3 -mpopcnt
##sse 4.1
CFLAGS_ARCH += -msse4.1 -msse4.2 -msse4a
## ssse3
CFLAGS_ARCH += -mssse3
## mmx
CFLAGS_ARCH += -mmmx
## avx
CFLAGS_ARCH += -mavx
## avx2
CFLAGS_ARCH += -mavx2 -mfma
}

##------------------------------------------ 5) bmi2-nopext
equals(TARGET, "bmi2-nopext"){
TARGET = Zangdar-$${VERSION}-bmi2-nopext

## x86-64
CFLAGS_ARCH += -msse -msse2
## popcount
CFLAGS_ARCH += -msse3 -mpopcnt
##sse 4.1
CFLAGS_ARCH += -msse4.1 -msse4.2 -msse4a
## ssse3
CFLAGS_ARCH += -mssse3
## mmx
CFLAGS_ARCH += -mmmx
## avx
CFLAGS_ARCH += -mavx
## avx2
CFLAGS_ARCH += -mavx2 -mfma
## bmi2
CFLAGS_ARCH += -mbmi -mbmi2
}

##------------------------------------------ 6) bmi2-pext
equals(TARGET, "bmi2-pext"){
TARGET = Zangdar-$${VERSION}-bmi2-pext

## x86-64
CFLAGS_ARCH += -msse -msse2
## popcount
CFLAGS_ARCH += -msse3 -mpopcnt
##sse 4.1
CFLAGS_ARCH += -msse4.1 -msse4.2 -msse4a
## ssse3
CFLAGS_ARCH += -mssse3
## mmx
CFLAGS_ARCH += -mmmx
## avx
CFLAGS_ARCH += -mavx
## avx2
CFLAGS_ARCH += -mavx2 -mfma
## bmi2
CFLAGS_ARCH += -mbmi -mbmi2 -DUSE_PEXT
}

##------------------------------------------ 7) native-nopext
equals(TARGET, "native-nopext"){
TARGET = Zangdar-$${VERSION}-5950X-nopext

CFLAGS_ARCH += -march=native
}

##------------------------------------------ 8) native-pext
equals(TARGET, "native-pext"){
TARGET = Zangdar-$${VERSION}-5950X-pext

CFLAGS_ARCH += -march=native -DUSE_PEXT
}


#----------------------------------------------------------------------
CFLAGS_COM  = -pipe -std=c++23
CFLAGS_REL  = $$CFLAGS_REL1 $$CFLAGS_WARN1
CFLAGS_WARN = $$CFLAGS_WARN1 $$CFLAGS_WARN2 $$CFLAGS_WARN3 $$CFLAGS_WARN4 $$CFLAGS_WARN5 $$CFLAGS_WARN6 $$CFLAGS_WARN7
CFLAGS_DBG  = -g -O2

#----------------------------------------------------------------------
QMAKE_CXXFLAGS_RELEASE -= -g
QMAKE_CXXFLAGS_RELEASE -= -O
QMAKE_CXXFLAGS_RELEASE -= -O0
QMAKE_CXXFLAGS_RELEASE -= -O1
QMAKE_CXXFLAGS_RELEASE -= -O2

QMAKE_CXXFLAGS_RELEASE += $$CFLAGS_COM $$CFLAGS_ARCH $$CFLAGS_REL

#----------------------------------------------------------------------
QMAKE_CXXFLAGS_DEBUG += -g
QMAKE_CXXFLAGS_DEBUG -= -O
QMAKE_CXXFLAGS_DEBUG += -O0
QMAKE_CXXFLAGS_DEBUG -= -O1
QMAKE_CXXFLAGS_DEBUG -= -O2
QMAKE_CXXFLAGS_DEBUG -= -O3

QMAKE_CXXFLAGS_DEBUG += $$CFLAGS_COM $$CFLAGS_ARCH $$CFLAGS_WARN $$CFLAGS_DBG

#----------------------------------------------------------------------

DISTFILES += \
    Makefile \
    README.md \
    VERSION.txt \
    scripts/spsa_tune.py \
    src/pyrrhic/LICENSE \
    src/pyrrhic/README.md


HEADERS += \
    src/Attacks.h \
    src/Bitboard.h \
    src/Board.h \
    src/Cuckoo.h \
    src/DataGen.h \
    src/History.h \
    src/Move.h \
    src/MoveList.h \
    src/MovePicker.h \
    src/NNUE.h \
    src/Search.h \
    src/SearchInfo.h \
    src/ThreadPool.h \
    src/Timer.h \
    src/TranspositionTable.h \
    src/Tunable.h \
    src/Uci.h \
    src/bench.h \
    src/bitmask.h \
    src/defines.h \
    src/incbin/incbin.h \
    src/pyrrhic/api.h \
    src/pyrrhic/stdendian.h \
    src/pyrrhic/tbconfig.h \
    src/pyrrhic/tbprobe.h \
    src/simd.h \
    src/types.h \
    src/zobrist.h


SOURCES += \
    src/Attacks.cpp \
    src/Board.cpp \
    src/Cuckoo.cpp \
    src/DataGen.cpp \
    src/History.cpp \
    src/MovePicker.cpp \
    src/NNUE.cpp \
    src/Search.cpp \
    src/ThreadPool.cpp \
    src/Timer.cpp \
    src/TranspositionTable.cpp \
    src/Tunable.cpp \
    src/Uci.cpp \
    src/attackers.cpp \
    src/fen.cpp \
    src/legal_moves.cpp \
    src/main.cpp \
    src/makemove.cpp \
    src/perft.cpp \
    src/pyrrhic/tbprobe.cpp \
    src/quiescence.cpp \
    src/see.cpp \
    src/syzygy.cpp \
    src/tests.cpp \
    src/think.cpp \
    src/tools.cpp \
    src/undomove.cpp \
    src/valid.cpp
