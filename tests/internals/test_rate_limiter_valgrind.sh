#!/bin/sh
if [ ! -x ./test_rate_limiter ]; then
    cd `dirname "$0"`
fi
valgrind --error-exitcode=100 ./test_rate_limiter
