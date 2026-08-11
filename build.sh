#!/bin/sh

# Build script for QUO project.
# Run `./build.sh -h` to see the build options.

# ---------- UTIL FUNCTIONS ---------- #

cmd() { echo "$@";"$@"; }

# ---------- COMMANDS FUNCTIONS ---------- #

# Build QUO CLI
build() {
    CFLAGS="-g"
    for arg in "$@"; do
        case $arg in
            release) CFLAGS="$CFLAGS -O3" ;;
            debug) CFLAGS="$CFLAGS -DQUO_DEBUG" ;;
            sanitize) CFLAGS="$CFLAGS -fsanitize=address" ;;
        esac
    done
    cmd cc $CFLAGS -o quo cli/*.c
}

run() {
    build
    cmd ./quo test.quo
}

# Run QUO in the GDB
debug() {
    build debug
    cmd gdb -ex run -ex bt -ex quit --args ./quo test.quo
}

# Run the QUO test suite in the `tests` directory
test() {
    build
    cd tests
    for f in $(ls -I utils.quo | sort -n); do
        name=$(basename "$f")
        printf "Test: %-20s " $name
        ../quo $name
        if [ $? -eq 0 ]
        then printf "\033[32mPASS\033[0m\n"
        else printf "\033[31mFAIL\033[0m\n"; exit 1
        fi
    done
}

# Build and run example
example() {
    shift
    if [ ! $# -eq 1 ]; then
        echo "Missing example name"
        usage
    fi
    if [ ! -f examples/$1/build.quo ]; then
        echo "Example not found: examples/$1"
        exit 1
    fi
    build
    cd examples/$1
    ../../quo build.quo
}

# Print statistics of quo.h
stats() {
    echo "quo.h: $(cat include/quo.h | wc -l) lines"
    echo "quo-mod-*.h: $(cat include/quo-mod-*.h | wc -l) lines"
}

clean() { rm -f quo; }

usage() {
    echo "Usage: $0 [option]"
    echo "Options:"
    echo "  -b, --build [options...]  Build the QUO cli (default)"
    echo "                            Options (can be combined):"
    echo "                              release   Optimized build"
    echo "                              debug     Enable debug logs"
    echo "                              sanitize  Enable address sanitizer"
    echo "  -r, --run                 Run the QUO cli on test.quo"
    echo "  -e, --example <name>      Run example at examples/<name>"
    echo "  -d, --debug               Debug the QUO cli on test.quo"
    echo "  -t, --test                Run the QUO test suite"
    echo "  -s, --stats               Print statistics of quo.h"
    echo "  -c, --clean               Clean up build artifacts"
    echo "  -h, --help                Show this help message"
    exit 1
}

# ---------- ARGUMENTS PARSING ---------- #

case ${1-} in
  -b|--build)   shift ; build "$@" ;;
  -r|--run)     run   ;;
  -e|--example) example "$@" ;;
  -d|--debug)   debug ;;
  -t|--test)    test  ;;
  -s|--stats)   stats ;;
  -c|--clean)   clean ;;
  -h|--help|*)  usage ;;
esac
