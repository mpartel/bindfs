## Process this file with automake to produce Makefile.in

bin_PROGRAMS = bindfs

noinst_HEADERS = debug.h permchain.h userinfo.h misc.h usermap.h
bindfs_SOURCES = bindfs.c debug.c permchain.c userinfo.c misc.c usermap.c

AM_CPPFLAGS = ${my_CPPFLAGS} ${fuse_CFLAGS}
AM_CFLAGS = ${my_CFLAGS}
bindfs_LDADD = $(fuse_LIBS)

man_MANS = bindfs.1
