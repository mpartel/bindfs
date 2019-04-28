#!/bin/sh -eu
if [ ! -x ./test_internals ]; then
    cd `dirname "$0"`
fi

if [ -n "`which valgrind`" ]; then
    valgrind --error-exitcode=100 ./test_internals
else
    echo "Warning: valgrind not found. Running without."
    ./test_internals
fi
