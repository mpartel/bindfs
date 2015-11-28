
## Overview ##

bindfs  -  http://bindfs.org/

bindfs is a FUSE filesystem for mirroring a directory to another
directory, similarly to `mount --bind`. The permissions of the mirrored
directory can be altered in various ways.

Some things bindfs can be used for:
- Making a directory read-only.
- Making all executables non-executable.
- Sharing a directory with a list of users (or groups).
- Modifying permission bits using rules with chmod-like syntax.
- Changing the permissions with which files are created.

Non-root users can use almost all features, but most interesting
use-cases need `user_allow_other` to be defined in `/etc/fuse.conf`.


## Installation ##

Make sure FUSE 2.6.0 or above is installed (http://fuse.sf.net/).
Then compile and install as usual:

    ./configure
    make
    make install

If you want the mounts made by non-root users to be visible to other users,
you may have to add the line `user_allow_other` to `/etc/fuse.conf`.

In Linux-based OSes, you may have to add your user to the `fuse` group.


## Usage ##

See the `bindfs --help` or the man-page for instructions and examples.


## OS X note ##

The following extra options may be useful under osxfuse:

    -o local,allow_other,extended_security,noappledouble

See https://github.com/osxfuse/osxfuse/wiki/Mount-options for details.


## Test suite ##

[![Build Status](https://travis-ci.org/mpartel/bindfs.svg?branch=master)](https://travis-ci.org/mpartel/bindfs)

Bindfs comes with a (somewhat brittle and messy) test suite.
The test suite has two kinds of tests: those that have to be run as root and
those that have to be run as non-root. To run all of the tests, do
`make check` both as root and as non-root.

The test suite requires Ruby 2.0+ (1.9+ might also work). If you're using
[RVM](https://rvm.io/) then you may need to use `rvmsudo` instead of plain
`sudo` to run the root tests.


## License ##

GNU General Public License version 2 or any later version.
See the file COPYING.
