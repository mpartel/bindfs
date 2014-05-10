
UNAME_S := $(shell uname -s)
if BUILD_OS_IS_DARWIN
	noinst_PROGRAMS = readdir_inode
	readdir_inode_SOURCES = readdir_inode.c
else
	noinst_PROGRAMS = readdir_inode utimens_nofollow
	readdir_inode_SOURCES = readdir_inode.c
	utimens_nofollow_SOURCES = utimens_nofollow.c
endif

TESTS = test_bindfs.rb
SUBDIRS = internals
