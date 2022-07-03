
## Overview ##

bindfs  -  https://bindfs.org/

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

[FUSE](https://github.com/libfuse/libfuse) 2.8.0 or above is required.
FUSE 3 is supported. When using FUSE 3, libfuse 3.10.2 or newer is
recommended to avoid a [bug with readdir](https://github.com/libfuse/libfuse/issues/583),
though it only seems to affect a few applications.

Download a [release](https://bindfs.org/downloads/) or clone this repository.

Then compile and install as usual:

    ./autogen.sh  # Only needed if you cloned the repo.
    ./configure
    make
    make install

If you want the mounts made by non-root users to be visible to other users,
you may have to add the line `user_allow_other` to `/etc/fuse.conf`.

On some systems, you may have to add your user to the `fuse` group.


## Usage ##

See the `bindfs --help` or the man-page for instructions and examples.


## MacOS note ##

The following extra options may be useful under osxfuse:

    -o local,allow_other,extended_security,noappledouble

See https://github.com/osxfuse/osxfuse/wiki/Mount-options for details.


## Test suite ##

[![Build Status](https://travis-ci.org/mpartel/bindfs.svg?branch=master)](https://travis-ci.org/mpartel/bindfs)

Bindfs comes with a (somewhat brittle and messy) test suite.
Some tests must be run as root, and some tests only work as non-root.

You can run run the tests with `./test-all.sh`.
It requires Ruby and `sudo`, and it uses `valgrind` if installed.

### Vagrant test runner ###

There is also a set of Vagrant configs for running the test suite on a variety
of systems. Run them with `vagrant/test.rb` (add `--help` for extra options).

You can destroy all bindfs Vagrant machines (but not the downloaded images)
with `make vagrant-clean`.


## License ##

GNU General Public License version 2 or any later version.
See the file COPYING.
