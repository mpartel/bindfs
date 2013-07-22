---
id: index
nav: index
layout: default
---

# bindfs

<p class="lead">
  Mount a directory to another location and alter permission bits.
</p>

      
*bindfs* is a [FUSE](http://fuse.sourceforge.net/) filesystem for mounting a directory to another location, similarly to `mount --bind`. The permissions inside the mountpoint can be altered using various rules.

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
  `bindfs#/home/bob/stuff/website /home/bob/public_html fuse force-user=www,perms=0000:u+rD 0 0`

## Docs

* [<span>bindfs --help</span>](docs/bindfs-help.txt)
* [man 1 bindfs](docs/bindfs.1.html)
* [ChangeLog](docs/ChangeLog.utf8.txt)

## Downloads and source

bindfs is available through the software repositories of many Linux distributions, as well as MacPorts on OS X. Check there first unless there is a specific new feature that you need.

Download the latest source tarball here: [{% first_file downloads %}](downloads/{% first_file downloads %}).

Developement takes place on [GitHub](https://github.com/mpartel/bindfs).

## About

bindfs has been tested on Linux 2.4/2.6 with fuse 2.5.3+, Mac OS X 10.5 with MacFUSE and FreeBSD with fuse4bsd.

All FUSE filesystems necessarily incur a performance penalty in CPU time and memory consumption. While bindfs is very flexible, it can be quite slow as [Guy Paddock's analysis and benchmark](http://www.redbottledesign.com/node/2495) demonstrates. If all you need is to make a directory read-only then `mount --bind -r` is more efficient.

Bug reports, pull requests, comments and ideas are very welcome. Please use the [issue tracker](https://github.com/mpartel/bindfs/issues) or e-mail me directly at &lt;martin dot partel at gmail dot com&gt;.

bindfs was initially developed in 2006. I consider the program fairly feature-complete but I'll still gladly fix bugs and add some small features as people suggest them.

### Known issues

* Hard-linking a domain socket does not preserve its identity ([legacy issue 27](https://code.google.com/p/bindfs/issues/detail?id=27)).
* Multi-threading is disabled by default because it has been seen triggering a race where one user may see attributes meant to be shown to another. If you use bindfs so that all users should see the same permissions and owners then you can enable multithreading by giving `--multithreaded`.

### Old site

bindfs used to be hosted on [Google Code](https://code.google.com/p/bindfs/). The [old issue tracker](https://code.google.com/p/bindfs/issues/list) has not been migrated.

Google has [deprecated](http://google-opensource.blogspot.fi/2013/05/a-change-to-google-code-download-service.html) downloads on Google Code, so no more downloads will be uploaded there. I will keep the old downloads in Google Code at least until 2014.

