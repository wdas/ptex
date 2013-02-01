ifndef CXX
CXX      = g++
LINK     = g++
else
LINK = $(CXX)
endif

# DEBUG = -g -DDEBUG
DEBUG    = -O2 -DNDEBUG
INCPATH  = -I../ptex
DEFINES  =

ifdef PRMAN_15_COMPATIBLE_PTEX
DEFINES += -DPTEX_NO_LARGE_METADATA_BLOCKS
endif

ifdef PTEX_STATIC
DEFINES += -DPTEX_STATIC
endif

CXXFLAGS = -Wall -pedantic -W -std=c++98 $(DEBUG) $(INCPATH) $(DEFINES) -fPIC
LFLAGS   =
LIBS     = -lm -lz -lpthread

SYSTEM := $(shell sh -c 'uname -s || echo unknown')

# Build universal binaries on OS X
ifeq ($(SYSTEM),Darwin)
    UNIVERSAL_FLAGS = -arch i386 -arch x86_64
    CXXFLAGS += $(UNIVERSAL_FLAGS)
    LFLAGS += $(UNIVERSAL_FLAGS)
    DSOEXT = dylib
else
    DSOEXT = so
endif
