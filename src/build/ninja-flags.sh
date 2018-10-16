#!/bin/sh
# make >= 4.2 includes -j<num> in $(MAKEFLAGS).
# make sets VERBOSE when calling this script.
ninja_flags () {
    for flag in "$@"
    do
        num_jobs=$(expr "z$flag" : 'z-j\([0-9]\+\)')
        test -n "$num_jobs" && break
    done
    test -n "$num_jobs" && opt_jobs=" -j $num_jobs"
    test -n "$VERBOSE" && opt_verbose=" -v"

    if test -n "$opt_jobs" || test -n "$opt_verbose"
    then
        printf -- '--%s%s' "$opt_jobs" "$opt_verbose"
    fi
}

ninja_flags "$@"
