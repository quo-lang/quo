#!/bin/sh

# Build script for QUO project.
# Run `./build.sh` to see the options.

# ---------- UTIL FUNCTIONS ---------- #

cmd() { echo "$@";"$@"; }

# ---------- COMMANDS FUNCTIONS ---------- #

# Build QUO CLI
# Options:
#   release
#   debug
#   sanitize
#   gperf
build() {
    mkdir -p build
    CFLAGS="-Wall -Wextra"
    case $1 in
        release) CFLAGS="$CFLAGS -O3 -DNDEBUG" ;;
        debug) CFLAGS="$CFLAGS -DQUO_DEBUG -g" ;;
        sanitize) CFLAGS="$CFLAGS -fsanitize=address" ;;
        gperf) CFLAGS="$CFLAGS -pg -O2" ;;
    esac
    cmd gcc $CFLAGS -o build/quo cli/*.c
}

run() {
    build
    cmd ./build/quo test.quo
}

# Run QUO in the GDB
debug() {
    build debug
    cmd gdb -ex run -ex bt -ex quit --args ./build/quo test.quo
}

# Run the QUO test suite in the `tests` directory
test() {
    build
    cd tests
    for f in $(ls -I utils.quo | sort -n); do
        name=$(basename "$f")
        printf "Test: %-20s " $name
        ../build/quo $name
        if [ $? -eq 0 ]
        then printf "\033[32mPASS\033[0m\n"
        else printf "\033[31mFAIL\033[0m\n"; exit 1
        fi
    done
}

# Print statistics of quo.h
stats() {
    echo "quo.h: $(cat include/quo.h | wc -l) lines"
    echo "quo-mod-*.h: $(cat include/quo-mod-*.h | wc -l) lines"
}

dist() {
    echo "Creating distribution"
    build release
    tar -czvf build/quo-$(git describe --tags --abbrev=0)-linux.tar.gz -C build quo -C .. include
    echo "Done"
}

tag() {
    echo "=== Manage git tags ==="
    printf "Select option Create(c), Delete(d): "
    read option
    case $option in
        c|C)
            echo "Create new tag"
            echo "Latest tag: $(git describe --tags --abbrev=0)"
            printf "Enter new version (e.g 0.0.0): "
            read tag
            git tag -a $tag -m "Release $tag"
            git push origin $tag
            ;;
        d|D)
            echo "Deleting tag"
            echo "Latest tag: $(git describe --tags)"
            echo "Enter tag to delete: "
            read tag
            git tag -d $tag
            git push origin --delete $tag
            ;;
        *)
            echo "Invalid option"
            exit 1
            ;;
    esac
}

clean() { rm -rf build; }

usage() {
    echo "Usage: $0 [option]"
    echo "Options:"
    echo "  build [options...]  Build the QUO cli (default)"
    echo "                      Options:"
    echo "                        release   Optimized build"
    echo "                        debug     Enable debug logs"
    echo "                        sanitize  Enable address sanitizer"
    echo "                        gperf     Enable GProf build"
    echo "  run                 Run the QUO cli on test.quo"
    echo "  dist                Create distribution archive"
    echo "  tag                 Manage git tags"
    echo "  debug               Debug the QUO cli on test.quo"
    echo "  test                Run the QUO test suite"
    echo "  stats               Print statistics of quo.h"
    echo "  clean               Clean up build artifacts"
    exit 1
}

# ---------- ARGUMENTS PARSING ---------- #

case ${1-} in
  build)   shift ; build "$@" ;;
  run)     run   ;;
  tag)     tag   ;;
  debug)   debug ;;
  dist)    dist  ;;
  test)    test  ;;
  stats)   stats ;;
  clean)   clean ;;
  *)       usage ;;
esac
