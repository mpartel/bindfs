## Process this file with automake to produce Makefile.in

bin_PROGRAMS = bindfs

noinst_HEADERS = debug.h permchain.h userinfo.h misc.h
bindfs_SOURCES = bindfs.c permchain.c userinfo.c misc.c

AM_CFLAGS = $(fuse_CFLAGS)
bindfs_LDADD = $(fuse_LIBS)

man_MANS = bindfs.1
