# Ptex

Ptex is a texture mapping system developed by Walt Disney Animation Studios
for production-quality rendering:

* No UV assignment is required! Ptex applies a separate texture to each face
  of a subdivision or polygon mesh.

* The Ptex file format can efficiently store hundreds of thousands of texture
  images in a single file.

* The Ptex API provides cached file I/O and high-quality filtering -
  everything that is needed to easily add Ptex support to a production-quality
  renderer or texture authoring application.


## Quickstart

Building with make (Linux):

    make prefix=$PWD/install
    make test
    make install
    make doc

The Makefile uses cmake under the hood.

This will build, install to a local `install` directory, and
run regression tests.

Documentation is only built if the following dependencies are installed:

- [Doxygen](http://www.doxygen.nl/)
- [Graphviz](https://graphviz.org/)


## Installation Contents

    install/bin:
        ptxinfo

    install/include:
        PtexHalf.h  PtexInt.h  Ptexture.h  PtexUtils.h

    install/lib:
        libPtex.a  libPtex.so

Note: docs will be generated in the `src/doc/ptex` directory
and installed as `install/share/doc/ptex`.


# Development

## Source tree

* [Ptex library](src/ptex)
* [Command-line utilities](src/utils)
* [Test suite](src/tests)
* [Documentation](src/doc)
* The Ptex website is developed in the `gh-pages` branch.


## Tests

Unit tests are in the `src/tests/` directory.

All tests are run by default when building the default `all` target.
Use `make test` to run the tests directly.

The `flags` variable can be used to forward flags to `ctest`.
For example, to run just the `half` tests:

        make test flags='-R half'


## Packaging Notes

Ptex's build scripts will use a hardcoded version number that may or may not
match the tagged version number when Ptex is built from a tarball instead of
from a git worktree.

This version be overriden by placing a file called `version` at the root of
the source tree containing the desired version number.

For example, the following command will generate a version file that will be
used by the build in lieu of git tags and the `PTEX_VER` cmake variable:

    echo v2.3.2 >version


## Release Prep

Before creating new minor version releases, run:

        ./src/tests/check_version --update vX.Y

`check_version.sh` updates the default `PTEX_VER` version in `CMakeLists.txt`
with the specified minor version number.

Running `./src/tests/check_version.sh` without any arguments will warn when
the git tags do not match the default version.

The `gh-pages` branch contains the `ptex.us` website.  When new release are
created add an entry to `buildsite.py` and run the script to update the site.
