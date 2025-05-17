TEMPLATE = app
CONFIG += console c++23
CONFIG -= app_bundle
CONFIG -= qt

CONFIG += static
QMAKE_LFLAGS += -static -static-libgcc -static-libstdc++ -lstdc++
DEFINES += STATIC

QMAKE_LFLAGS += -flto=auto

#------------------------------------------------------
NETWORK = "/mnt/Datas/Echecs/Programmation/Zangdar/APP/networks/net-2-255.bin"
QMAKE_CXXFLAGS_RELEASE += -DNETWORK='\\"$${NETWORK}\\"'
DEFINES += USE_SIMD

#------------------------------------------------------
# ATTENTION : ne pas mettre "\" car ils sont reconnus comme caractères spéciaux
# pour Github
HOME_STR = "./"
# pour moi
# HOME_STR = "D:/Echecs/Programmation/Zangdar/"
DEFINES += HOME='\\"$${HOME_STR}\\"'

#------------------------------------------------------
VERSION = "$$cat(VERSION.txt)"
# message($$VERSION)
DEFINES += VERSION='\\"$${VERSION}\\"'

#------------------------------------------------------
# DEFINES += USE_TUNE

#------------------------------------------------------
# DEFINES +=  DEBUG_EVAL
# DEFINES +=  DEBUG_LOG
# DEFINES +=  DEBUG_TIME
# DEFINES += USE_PRETTY

#------------------------------------------------------
# https://gcc.gnu.org/onlinedocs/gcc/x86-Options.html

QMAKE_CXXFLAGS_RELEASE -= -g
QMAKE_CXXFLAGS_RELEASE -= -O
QMAKE_CXXFLAGS_RELEASE -= -O0
QMAKE_CXXFLAGS_RELEASE -= -O1
QMAKE_CXXFLAGS_RELEASE -= -O2

# si NDEBUG est défini, alors assert ne fait rien

QMAKE_CXXFLAGS_RELEASE += -pipe -std=c++23 -O3 -flto=auto -DNDEBUG -fwhole-program
QMAKE_CXXFLAGS_RELEASE += -pedantic -Wshadow -Wall -Wextra -Wcast-qual -Wuninitialized -Weffc++


# message($$DEFINES)
TARGET  = native-pext

##------------------------------------------ 1) old
equals(TARGET, "old"){
TARGET = Zangdar-$${VERSION}-old
QMAKE_CXXFLAGS_RELEASE +=
}

##------------------------------------------ 2) x86-64
equals(TARGET, "x86-64"){
TARGET = Zangdar-$${VERSION}-x86-64

## x86-64
QMAKE_CXXFLAGS_RELEASE += -msse -msse2
}

##------------------------------------------ 3) popcnt
equals(TARGET, "popcnt"){
TARGET = Zangdar-$${VERSION}-popcnt

## x86-64
QMAKE_CXXFLAGS_RELEASE += -msse -msse2
## popcount
QMAKE_CXXFLAGS_RELEASE += -msse3 -mpopcnt
##sse 4.1
QMAKE_CXXFLAGS_RELEASE += -msse4.1 -msse4.2 -msse4a
## ssse3
QMAKE_CXXFLAGS_RELEASE += -mssse3
## mmx
QMAKE_CXXFLAGS_RELEASE += -mmmx
## avx
QMAKE_CXXFLAGS_RELEASE += -mavx
}

##------------------------------------------ 4) avx2 (2013)
equals(TARGET, "avx2"){
TARGET = Zangdar-$${VERSION}-avx2

## x86-64
QMAKE_CXXFLAGS_RELEASE += -msse -msse2
## popcount
QMAKE_CXXFLAGS_RELEASE += -msse3 -mpopcnt
##sse 4.1
QMAKE_CXXFLAGS_RELEASE += -msse4.1 -msse4.2 -msse4a
## ssse3
QMAKE_CXXFLAGS_RELEASE += -mssse3
## mmx
QMAKE_CXXFLAGS_RELEASE += -mmmx
## avx
QMAKE_CXXFLAGS_RELEASE += -mavx
## avx2
QMAKE_CXXFLAGS_RELEASE += -mavx2 -mfma
}

##------------------------------------------ 5) bmi2-nopext
equals(TARGET, "bmi2-nopext"){
TARGET = Zangdar-$${VERSION}-bmi2-nopext

## x86-64
QMAKE_CXXFLAGS_RELEASE += -msse -msse2
## popcount
QMAKE_CXXFLAGS_RELEASE += -msse3 -mpopcnt
##sse 4.1
QMAKE_CXXFLAGS_RELEASE += -msse4.1 -msse4.2 -msse4a
## ssse3
QMAKE_CXXFLAGS_RELEASE += -mssse3
## mmx
QMAKE_CXXFLAGS_RELEASE += -mmmx
## avx
QMAKE_CXXFLAGS_RELEASE += -mavx
## avx2
QMAKE_CXXFLAGS_RELEASE += -mavx2 -mfma
## bmi2
QMAKE_CXXFLAGS_RELEASE += -mbmi -mbmi2
}

##------------------------------------------ 6) bmi2-pext
equals(TARGET, "bmi2-pext"){
TARGET = Zangdar-$${VERSION}-bmi2-pext

## x86-64
QMAKE_CXXFLAGS_RELEASE += -msse -msse2
## popcount
QMAKE_CXXFLAGS_RELEASE += -msse3 -mpopcnt
##sse 4.1
QMAKE_CXXFLAGS_RELEASE += -msse4.1 -msse4.2 -msse4a
## ssse3
QMAKE_CXXFLAGS_RELEASE += -mssse3
## mmx
QMAKE_CXXFLAGS_RELEASE += -mmmx
## avx
QMAKE_CXXFLAGS_RELEASE += -mavx
## avx2
QMAKE_CXXFLAGS_RELEASE += -mavx2 -mfma
## bmi2
QMAKE_CXXFLAGS_RELEASE += -mbmi -mbmi2 -DUSE_PEXT
}

##------------------------------------------ 7) native-nopext
equals(TARGET, "native-nopext"){
TARGET = Zangdar-$${VERSION}-5950X-nopext

QMAKE_CXXFLAGS_RELEASE += -march=native
}

##------------------------------------------ 8) native-pext
equals(TARGET, "native-pext"){
TARGET = Zangdar-$${VERSION}-5950X-pext

QMAKE_CXXFLAGS_RELEASE += -march=native -DUSE_PEXT
}




#----------------------------------------------------------------------
QMAKE_CXXFLAGS_DEBUG += -g
QMAKE_CXXFLAGS_DEBUG -= -O
QMAKE_CXXFLAGS_DEBUG += -O0
QMAKE_CXXFLAGS_DEBUG -= -O1
QMAKE_CXXFLAGS_DEBUG -= -O2
QMAKE_CXXFLAGS_DEBUG -= -O3

QMAKE_CXXFLAGS_DEBUG += -pipe -std=c++20 -Wshadow -Wall -Wextra -Wcast-qual -Wuninitialized -Wcast-align=strict
QMAKE_CXXFLAGS_DEBUG += -march=native
QMAKE_CXXFLAGS_DEBUG += -DUSE_PEXT
QMAKE_CXXFLAGS_DEBUG += -DNETWORK='\\"$${NETWORK}\\"'


DISTFILES += \
    Makefile \
    README.md \
    VERSION.txt \
    src/pyrrhic/LICENSE \
    src/pyrrhic/README.md


HEADERS += \
    src/Attacks.h \
    src/Bitboard.h \
    src/Board.h \
    src/DataGen.h \
    src/History.h \
    src/Move.h \
    src/MoveList.h \
    src/MovePicker.h \
    src/NNUE.h \
    src/Search.h \
    src/ThreadData.h \
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
    src/DataGen.cpp \
    src/History.cpp \
    src/MovePicker.cpp \
    src/NNUE.cpp \
    src/Search.cpp \
    src/ThreadData.cpp \
    src/ThreadPool.cpp \
    src/Timer.cpp \
    src/TranspositionTable.cpp \
    src/Tunable.cpp \
    src/Uci.cpp \
    src/add_moves.cpp \
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
