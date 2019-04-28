
UNAME_S := $(shell uname -s)

AM_CPPFLAGS = ${my_CPPFLAGS}
AM_CFLAGS = ${my_CFLAGS}

noinst_PROGRAMS = readdir_inode utimens_nofollow fcntl_locker odirect_read odirect_write test_dir_rewind
readdir_inode_SOURCES = readdir_inode.c
utimens_nofollow_SOURCES = utimens_nofollow.c
fcntl_locker_SOURCES = fcntl_locker.c
odirect_read_SOURCES = odirect_read.c
odirect_write_SOURCES = odirect_write.c
test_dir_rewind_SOURCES = test_dir_rewind.c

TESTS = test_bindfs.rb
SUBDIRS = internals
