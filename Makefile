#!/usr/bin/env make
# prefix=<path> specifies the installation prefix (ex: /usr/local) 
# DESTDIR=<path> specifies the temporary staging directory (ex: /tmp/stage)
# V=1 or VERBOSE=1 enables verbose builds
# NO_NINJA=1 disables autodetection and use of ninja
# PRMAN_15_COMPATIBLE_PTEX=1 enables compatibility with PRman 15
# FLAVOR={opt,debug,profile} sets CMAKE_BUILD_TYPE using short aliases
# BUILD_TYPE={Release,Debug,RelWithDebInfo} sets CMAKE_BUILD_TYPE directly

FLAGS =
FLAVOR ?= optimize
uname_S := $(shell sh -c 'uname -s || echo kernel')
uname_R := $(shell sh -c '(uname -r || echo release) | cut -d- -f1')
uname_M := $(shell sh -c 'uname -m || echo machine')
platform ?= $(uname_S)-$(uname_R)-$(uname_M)-$(FLAVOR)
build = build/$(platform)
# Installation prefix
prefix = $(CURDIR)/$(platform)
CMAKE_FLAGS += -DCMAKE_INSTALL_PREFIX=$(prefix)
TEST_FLAGS = --output-on-failure --force-new-ctest-process

ifdef V
    VERBOSE = 1
endif
ifndef VERBOSE
    QUIET = @
endif

# Ninja auto-detection
NO_NINJA := $(shell sh -c '(type ninja >/dev/null && echo 0) || echo 1')
ifeq ($(NO_NINJA),1)
    CMAKE_FLAGS += -G "Unix Makefiles"
    FLAGS += -- --no-print-directory
else
    CMAKE_FLAGS += -G Ninja
    FLAGS += $(shell sh -c '\
        VERBOSE=$(VERBOSE) ./src/build/ninja-flags.sh $(MAKEFLAGS)')
endif
ifdef PRMAN_15_COMPATIBLE_PTEX
    CMAKE_FLAGS += -DPRMAN_15_COMPATIBLE_PTEX:BOOL=TRUE
endif
ifdef TOOLCHAIN
    CMAKE_FLAGS += -DCMAKE_TOOLCHAIN_FILE=$(TOOLCHAIN)
endif
ifdef BUILD_TYPE
    CMAKE_FLAGS += -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)
endif
ifdef BUILD_SHARED_LIBS
    CMAKE_FLAGS += -DPTEX_BUILD_SHARED_LIBS=$(BUILD_SHARED_LIBS)
endif
ifdef BUILD_STATIC_LIBS
    CMAKE_FLAGS += -DPTEX_BUILD_SHARED_LIBS=$(BUILD_STATIC_LIBS)
endif

# Targets
default:: test

test:: all

clean:: uninstall

configure = $(build)/cmake.conf
$(configure):
	$(QUIET)mkdir -p $(build)
	$(QUIET)cd $(build) && cmake $(CMAKE_FLAGS) $(CURDIR)
	$(QUIET)touch $@

cmake:
	$(QUIET)mkdir -p $(build)
	$(QUIET)cd $(build) && cmake $(CMAKE_FLAGS) $(CURDIR)

all clean doc doxygen install:: $(configure)
	+$(QUIET)cmake --build $(build) --target $@ $(FLAGS)

test:: $(configure)
	+$(QUIET)cd $(build) && ctest $(TEST_FLAGS) $(flags)

install::
	$(QUIET)cp $(build)/install_manifest.txt $(build).manifest.txt

uninstall: $(configure)
	$(QUIET)touch $(build).manifest.txt
	$(QUIET)xargs rm -f <$(build).manifest.txt
	$(QUIET)xargs dirname <$(build).manifest.txt 2>/dev/null | \
	sort -u -r | xargs rmdir -p >/dev/null 2>&1 || true
