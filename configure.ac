AC_INIT([bindfs],[1.15.0],[martin.partel@gmail.com])

AM_INIT_AUTOMAKE([foreign serial-tests])
AC_CONFIG_HEADERS([config.h])

AC_PROG_CC
AC_LANG(C)
AC_PROG_LIBTOOL

# --enable and --with options
AC_ARG_ENABLE([debug-output],
    [AS_HELP_STRING([--enable-debug-output], [enable debug output])])
AC_ARG_WITH([core-foundation],
    AS_HELP_STRING([--with-core-foundation], [link against Core Foundation (OS X only) @<:@default=no@:>@]))
AC_ARG_WITH([fuse2],
    [AS_HELP_STRING([--with-fuse2], [link against libfuse 2.x (default: autodetect, preferring 3.x)])])
AC_ARG_WITH([fuse3],
    [AS_HELP_STRING([--with-fuse3], [link against libfuse 3.x (default: autodetect, preferring 3.x)])])

if test x"$enable_debug_output" == "xyes" ; then
    AC_DEFINE([BINDFS_DEBUG], [1], [Define to 1 to enable debugging messages])
fi

if test x"$with_core_foundation" == "xyes" ; then
    AC_MSG_NOTICE([Linking with Core Foundation])
    LDFLAGS="${LDFLAGS} -framework CoreFoundation"
fi

AM_CONDITIONAL(BUILD_OS_IS_DARWIN, [test x"$build_os" = darwin])

my_CPPFLAGS="-D_REENTRANT -D_FILE_OFFSET_BITS=64"
my_CFLAGS="-Wall -fno-common"
my_LDFLAGS="-pthread"
AC_SUBST([my_CPPFLAGS])
AC_SUBST([my_CFLAGS])
AC_SUBST([my_LDFLAGS])

# Checks for platform-specific stuff
AC_CHECK_HEADERS([sys/file.h])
AC_CHECK_FUNCS([lutimes utimensat])
AC_CHECK_FUNCS([setxattr getxattr listxattr removexattr])
AC_CHECK_FUNCS([lsetxattr lgetxattr llistxattr lremovexattr])
AC_COMPILE_IFELSE(
    [AC_LANG_PROGRAM([[
        #define BSD_SOURCE_
        #include <sys/types.h>
        #include <sys/stat.h>
        #include <unistd.h>
        void foo() { struct stat st; st.st_mtim.tv_nsec = 123; }
    ]])],
    [AC_DEFINE(
        [HAVE_STAT_NANOSEC],
        [1],
        [Define if struct stat has st_mtim.tv_nsec etc.]
    )]
)

# Check for fuse

AC_DEFUN([CHECK_FUSE2],
    [PKG_CHECK_MODULES([fuse], [fuse >= 2.9.0],
        [
            AC_DEFINE([HAVE_FUSE_29], [1], [Have FUSE >= 2.9.0])
            AC_DEFINE([FUSE_USE_VERSION], [26], [FUSE API VERSION = 2.6])
        ],
        [PKG_CHECK_MODULES([fuse], [fuse >= 2.8.0],
            [
                AC_DEFINE([FUSE_USE_VERSION], [26], [FUSE API VERSION = 2.6])
            ],
            [$1]
        )]
    )]
)

AC_DEFUN([CHECK_FUSE3],
    [PKG_CHECK_MODULES([fuse3], [fuse3 >= 3.4.0],
        [
            AC_DEFINE([HAVE_FUSE_3], [1], [Have FUSE >= 3.0])
            AC_DEFINE([FUSE_USE_VERSION], [34], [FUSE API VERSION = 3.4])
        ],
        [$1]
    )]
)

AS_IF([test "x$with_fuse2" == "xyes"],
    [CHECK_FUSE2([AC_MSG_ERROR([FUSE 2 not found])])],
    [AS_IF([test "x$with_fuse3" == "xyes"],
        [CHECK_FUSE3([AC_MSG_ERROR([FUSE 3 not found])])],
        [CHECK_FUSE3([CHECK_FUSE2([AC_MSG_ERROR([FUSE not found])])])]
    )]
)

AC_CONFIG_FILES([Makefile \
    src/Makefile \
    tests/Makefile \
    tests/internals/Makefile])
AC_OUTPUT

