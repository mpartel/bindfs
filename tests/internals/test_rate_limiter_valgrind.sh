#!/bin/sh -eu
if [ ! -x ./test_rate_limiter ]; then
    cd `dirname "$0"`
fi

if [ -n "`which valgrind`" ]; then
    valgrind --error-exitcode=100 ./test_rate_limiter
else
    echo "Warning: valgrind not found. Running without."
    ./test_rate_limiter
fi
