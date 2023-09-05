---
id: index
nav: index
layout: default
---

# bindfs

<p class="lead">
  Mount a directory to another location and alter permission bits.
</p>

*bindfs* is a [FUSE](https://github.com/libfuse/libfuse) filesystem for mounting a directory to another location, similarly to `mount --bind`. The permissions inside the mountpoint can be altered using various rules.

## Examples

Here are some examples of what bindfs can do for you:

* Make a directory read-only for non-root users.<br />
  `bindfs --perms=a-w somedir somedir`

* Share a directory with some other users without modifying /etc/group.<br />
  `bindfs --mirror-only=joe,bob,@wheel ~/some/dir shared`

* Make all new files uploaded to an FTP share owned by you and seen by everyone.<br />
  `bindfs --create-for-user=me --create-with-perms=u+rw,a+rD /var/ftp/uploads /var/ftp/uploads`

* Make your website available to the webserver process read-only.<br />
  `bindfs --force-user=www --perms=0000:u=rD ~/stuff/website ~/public_html`

* The corresponding `/etc/fstab` entry.<br />
  `/home/bob/stuff/website /home/bob/public_html fuse.bindfs force-user=www,perms=0000:u+rD 0 0`

## Docs

* [<span>bindfs \-\-help</span>](docs/bindfs-help.txt)
* [man 1 bindfs](docs/bindfs.1.html)
* [ChangeLog](docs/ChangeLog.utf8.txt)

## Installing

bindfs is available through the software repositories of many Linux distributions, as well as [Homebrew](https://formulae.brew.sh/formula/bindfs#default) on MacOS. Check there first unless there is a specific new feature that you need.

To compile from source on Linux, first `apt install build-essential pkg-config libfuse3-dev` (or `libfuse-dev` on older systems). On MacOS, install XCode (and let it install Developer Tools), [pkg-config](https://formulae.brew.sh/formula/pkg-config#default) and either [MacFuse](https://osxfuse.github.io/) or [fuse-t](https://www.fuse-t.org/).

Download the latest bindfs source code here: [{% first_file downloads %}](downloads/{% first_file downloads %}).

Compile and install bindfs: `./configure && make && sudo make install`.

## About

bindfs is developed and tested primarily on Linux with [FUSE](https://github.com/libfuse/libfuse) 2 and 3, but it has been reported to work reasonably well on MacOS with [MacFuse](http://osxfuse.github.io/) and [fuse-t](https://www.fuse-t.org/), and on FreeBSD with [fuse4bsd](http://www.freshports.org/sysutils/fusefs-kmod/).

All FUSE filesystems necessarily incur a performance penalty in CPU time and memory consumption. If all you need is to make a directory read-only then `mount --bind -r` is more efficient.

bindfs was initially developed in 2006. I consider the program fairly feature-complete but I'll still gladly fix bugs and add some small features as people suggest them.

There is an extensive (if dated) [HowTo on Ubuntu Forums](http://ubuntuforums.org/showthread.php?t=1460472).

### Bug reports

Bug reports, pull requests, comments and ideas are very welcome. Developement takes place on [GitHub](https://github.com/mpartel/bindfs). Please use the [issue tracker](https://github.com/mpartel/bindfs/issues).

### Known issues

* When using FUSE 3, libfuse 3.10.2 or newer is recommended to avoid a [bug with readdir](https://github.com/libfuse/libfuse/issues/583), though it only seems to affect a few applications.
* Hard-linking a domain socket does not preserve its identity.
* inotify events are not triggered ([#7](https://github.com/mpartel/bindfs/issues/7)).
* There may be issues with namespaces ([#10](https://github.com/mpartel/bindfs/issues/10)).
* Multi-threading is disabled by default because it has been seen triggering a race where one user may see attributes meant to be shown to another. If you use bindfs so that all users should see the same permissions and owners then you can enable multithreading by giving `--multithreaded`.
* Some distros unmount all FUSE filesystems when the network goes down ([#72](https://github.com/mpartel/bindfs/issues/72) & [https://bugs.gentoo.org/679106](https://bugs.gentoo.org/679106))
