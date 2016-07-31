
UNAME_S := $(shell uname -s)

noinst_PROGRAMS = readdir_inode utimens_nofollow fcntl_locker
readdir_inode_SOURCES = readdir_inode.c
utimens_nofollow_SOURCES = utimens_nofollow.c
fcntl_locker_SOURCES = fcntl_locker.c

TESTS = test_bindfs.rb
SUBDIRS = internals
