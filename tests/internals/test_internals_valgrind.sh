#!/bin/sh
if [ ! -x ./test_internals ]; then
    cd `dirname "$0"`
fi
valgrind --error-exitcode=100 ./test_internals
