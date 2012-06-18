AC_INIT([bindfs],[1.10.4],[martin.partel@gmail.com])

AM_INIT_AUTOMAKE
AM_CONFIG_HEADER(config.h)

AC_PROG_CC
AC_LANG(C)
AC_PROG_LIBTOOL

# --enable and --with options
AC_ARG_ENABLE([debug],
	[AS_HELP_STRING([--enable-debug], [enable debug symbols])])
	AC_ARG_ENABLE([debug-output],
	[AS_HELP_STRING([--enable-debug-output], [enable debug output])])
AC_ARG_WITH([core-foundation],
	AS_HELP_STRING([--with-core-foundation], [link against Core Foundation (OS X only) @<:@default=no@:>@]))


if test x"$enable_debug" == "xyes" ; then
	CFLAGS="${CFLAGS} -g -O0"
else
	CFLAGS="${CFLAGS} -O2"
fi

if test x"$enable_debug_output" == "xyes" ; then
	AC_DEFINE([BINDFS_DEBUG], [1], [Define to 1 to enable debugging messages])
fi

if test x"$with_core_foundation" == "xyes" ; then
	AC_MSG_NOTICE([Linking with Core Foundation])
	LDFLAGS="${LDFLAGS} -framework CoreFoundation"
fi

CFLAGS="${CFLAGS} -Wall -D_REENTRANT -D_FILE_OFFSET_BITS=64 -DFUSE_USE_VERSION=25"

# Check for xattrs
AC_CHECK_FUNCS([setxattr getxattr listxattr removexattr])
AC_CHECK_FUNCS([lsetxattr lgetxattr llistxattr lremovexattr])

# Check for fuse
PKG_CHECK_MODULES([fuse], [fuse >= 2.5.3])

AC_CONFIG_FILES([Makefile \
	src/Makefile \
	tests/Makefile])
AC_OUTPUT

