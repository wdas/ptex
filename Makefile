#!/usr/bin/env make
SH ?= sh
uname_S := $(shell $(SH) -c 'uname -s || echo kernel')
uname_R := $(shell $(SH) -c 'uname -r | cut -d- -f1 || echo release')
uname_M := $(shell $(SH) -c 'uname -m || echo machine')
FLAVOR ?= optimize
platform_dir ?= $(uname_S)-$(uname_R)-$(uname_M)-$(FLAVOR)
cmake_dir ?= $(CURDIR)/build/$(platform_dir)
cmake_done ?= $(cmake_dir)/cmake.done

prefix ?= $(CURDIR)/$(platform_dir)
#DESTDIR =

CMAKE_FLAGS ?= -DCMAKE_INSTALL_PREFIX=$(prefix)

ifdef PRMAN_15_COMPATIBLE_PTEX
    CMAKE_FLAGS += -DPRMAN_15_COMPATIBLE_PTEX:BOOL=TRUE
endif

ifdef TOOLCHAIN
    CMAKE_FLAGS += -DCMAKE_TOOLCHAIN_FILE=$(TOOLCHAIN)
endif

ifdef BUILD_TYPE
    CMAKE_FLAGS += -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)
endif

# make V=1 shortcut for VERBOSE=1
ifdef V
    VERBOSE=1
    export VERBOSE
endif

all::

$(cmake_done):
	mkdir -p $(cmake_dir)
	cd $(cmake_dir) && cmake $(CMAKE_FLAGS) ../..
	touch $@

cmake: $(cmake_done)
.PHONY: cmake

all:: cmake
	$(MAKE) -C $(cmake_dir) --no-print-directory $(MAKEARGS) all
	$(MAKE) -C $(cmake_dir) --no-print-directory $(MAKEARGS) test

clean: cmake
	$(MAKE) -C $(cmake_dir) --no-print-directory $(MAKEARGS) clean

doc: cmake
	$(MAKE) -C $(cmake_dir) --no-print-directory $(MAKEARGS) doc

install: cmake
	$(MAKE) -C $(cmake_dir) --no-print-directory $(MAKEARGS) install

test: cmake
	$(MAKE) -C $(cmake_dir) --no-print-directory $(MAKEARGS) test
