#!/bin/sh
cd `dirname "$0"`
valgrind --error-exitcode=100 ./test_rate_limiter
