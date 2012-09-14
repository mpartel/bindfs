
noinst_PROGRAMS = readdir_inode
readdir_inode_SOURCES = readdir_inode.c

test_internals_CPPFLAGS = ${my_CPPFLAGS} ${fuse_CFLAGS}
test_internals_CFLAGS = ${my_CFLAGS}

TESTS = test_bindfs.rb
SUBDIRS = internals
