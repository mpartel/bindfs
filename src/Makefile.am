## Process this file with automake to produce Makefile.in

bin_PROGRAMS = bindfs

noinst_HEADERS = debug.h permchain.h userinfo.h arena.h misc.h usermap.h rate_limiter.h
bindfs_SOURCES = bindfs.c debug.c permchain.c userinfo.c arena.c misc.c usermap.c rate_limiter.c

AM_CPPFLAGS = ${my_CPPFLAGS} ${fuse_CFLAGS} ${fuse3_CFLAGS}
AM_CFLAGS = ${my_CFLAGS}
bindfs_LDADD = $(fuse_LIBS) ${fuse3_LIBS} ${my_LDFLAGS}

man_MANS = bindfs.1
