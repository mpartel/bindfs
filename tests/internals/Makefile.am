
noinst_PROGRAMS = test_internals
test_internals_SOURCES = test_internals.c $(top_builddir)/src/misc.c

test_internals_CPPFLAGS = ${my_CPPFLAGS} ${fuse_CFLAGS} -I$(top_builddir)/src
test_internals_CFLAGS = ${my_CFLAGS}

TESTS = test_internals_valgrind.sh
