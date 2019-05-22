#!/bin/sh
# If we have a Git repository then check whether our hard-coded version
# in CMakeLists.txt matches our tags.
usage () {
    echo "Check or update the CMakeLists.txt version against Git tags."
    echo "Runs a version check when no flags are specified."
    echo
    echo "usage: $(basename "$0") [--update <vX.Y>]"
    echo "options:"
    echo "    -u|--update       Update CMakeLists.txt"
    exit 0
}

# Update CMakeLists.txt by passing the "--update" flag
main () {
    case "$1" in
    -u|--update)
        update "$2"
        ;;
    -h|--help)
        usage
        ;;
    *)
        check_version
        ;;
    esac
}

git_version () {
    git describe --first-parent --always HEAD
}

strip_version () {
    # Transform "v?A.B.C-abc" into "A.B"
    sed -e 's/^v//' | cut -d . -f 1,2
}

format_version () {
    # Transform "A.B" into "vA.B.X"
    read stdin_version
    stdin_version=$(echo "$stdin_version" | strip_version)
    printf 'v%s.X' "$stdin_version"
}


check_version () {
    if git rev-parse HEAD >/dev/null
    then
        version=$(git_version | format_version)
        if ! grep "$version" CMakeLists.txt >/dev/null
        then
            if test "$quiet" != 1
            then
                simple_version=$(git_version | strip_version)
                echo 1>&2
                echo 1>&2 error: CMakeLists.txt does not contain "$version"
                echo 1>&2 fixit: "$0" --update "$simple_version"
                echo 1>&2
            fi
            return 1
        else
            echo "CMakeLists.txt is already up to date."
            return 0
        fi
    fi
    # This test is skipped if git is unavailable
    echo 1>&2 "warning: $(basename "$0") was skipped because git is unavailable."
    return 0
}

update () {
    quiet=1
    if ! check_version
    then
        if test -n "$1"
        then
            version=$(echo "$1" | format_version)
        else
            version=$(git_version | format_version)
        fi
        perl -p -i -e "s/v\\d+\\.\\d+\\.X/$version/" CMakeLists.txt &&
        echo "CMakeLists.txt updated." &&
        git diff -- CMakeLists.txt
    fi

    exit 0
}

main "$@"
