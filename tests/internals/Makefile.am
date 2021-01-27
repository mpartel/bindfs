
noinst_HEADERS = test_common.h
noinst_PROGRAMS = test_internals test_rate_limiter
test_internals_SOURCES = test_internals.c test_common.c $(top_srcdir)/src/misc.c
test_rate_limiter_SOURCES = test_rate_limiter.c test_common.c $(top_srcdir)/src/rate_limiter.c

test_internals_CPPFLAGS = ${my_CPPFLAGS} ${fuse_CFLAGS} ${fuse3_CFLAGS} -I. -I$(top_srcdir)/src
test_internals_CFLAGS = ${my_CFLAGS}
test_internals_LDADD = ${my_LDFLAGS}

test_rate_limiter_CPPFLAGS = ${my_CPPFLAGS} ${fuse_CFLAGS} ${fuse3_CFLAGS} -I. -I$(top_srcdir)/src
test_rate_limiter_CFLAGS = ${my_CFLAGS}
test_rate_limiter_LDADD = ${my_LDFLAGS}

TESTS = test_internals_valgrind.sh test_rate_limiter_valgrind.sh
