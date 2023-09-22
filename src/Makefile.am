## Process this file with automake to produce Makefile.in

bin_PROGRAMS = bindfs

noinst_HEADERS = debug.h permchain.h userinfo.h arena.h misc.h usermap.h rate_limiter.h
bindfs_SOURCES = bindfs.c debug.c permchain.c userinfo.c arena.c misc.c usermap.c rate_limiter.c

AM_CPPFLAGS = ${my_CPPFLAGS} ${fuse_CFLAGS} ${fuse3_CFLAGS} ${fuse_t_CFLAGS}
AM_CFLAGS = ${my_CFLAGS}
bindfs_LDADD = ${fuse_LIBS} ${fuse3_LIBS} ${fuse_t_LIBS} ${my_LDFLAGS}

man_MANS = bindfs.1

if BUILD_OS_IS_DARWIN
bindfs_BUNDLEDIR = /Library/Filesystems/bindfs.fs
bindfs_BUNDLEBINDIR = "${bindfs_BUNDLEDIR}/Contents/Resources"

install-exec-hook:
	(mkdir -p "${bindfs_BUNDLEBINDIR}"; ln -s "${bindir}/bindfs" "${bindfs_BUNDLEBINDIR}/mount_bindfs")

uninstall-hook:
	(rm -rf ${bindfs_BUNDLEDIR})
endif
