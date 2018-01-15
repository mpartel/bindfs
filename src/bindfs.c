/*
    Copyright 2006,2007,2008,2009,2010,2012 Martin Pärtel <martin.partel@gmail.com>

    This file is part of bindfs.

    bindfs is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    bindfs is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with bindfs.  If not, see <http://www.gnu.org/licenses/>.


    This file is based on fusexmp_fh.c from FUSE 2.5.3,
    which had the following notice:
    ---
    FUSE: Filesystem in Userspace
    Copyright (C) 2001-2006  Miklos Szeredi <miklos@szeredi.hu>

    This program can be distributed under the terms of the GNU GPL.
    See the file COPYING.
    ---

*/

#include <config.h>

/* For >= 500 for pread/pwrite and readdir_r; >= 700 for utimensat */
#define _XOPEN_SOURCE 700

/* For flock() on FreeBSD. It otherwise gets hidden by _XOPEN_SOURCE  */
#define __BSD_VISIBLE 1

/* For stat() nanosecond precision and lutimes() */
#define _BSD_SOURCE
/* The new non-deprecated version of _BSD_SOURCE */
#define _DEFAULT_SOURCE

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#include <sys/time.h>
#include <sys/statvfs.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <getopt.h>
#include <assert.h>
#include <pwd.h>
#include <grp.h>
#include <limits.h>
#include <signal.h>
#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif

#ifdef __LINUX__
#include <linux/fs.h>  // For BLKGETSIZE64
#endif

#include <fuse.h>
#include <fuse_opt.h>

#include "debug.h"
#include "misc.h"
#include "permchain.h"
#include "rate_limiter.h"
#include "userinfo.h"
#include "usermap.h"

/* Apple Structs */
#ifdef __APPLE__
#include <sys/param.h>
#define G_PREFIX   "org"
#define G_KAUTH_FILESEC_XATTR G_PREFIX ".apple.system.Security"
#define A_PREFIX   "com"
#define A_KAUTH_FILESEC_XATTR A_PREFIX ".apple.system.Security"
#define XATTR_APPLE_PREFIX   "com.apple."
#endif

/* We pessimistically assume signed uid_t and gid_t in our overflow checks,
   mostly because supporting both cases would require a bunch more code. */
static const uid_t UID_T_MAX = ((1LL << (sizeof(uid_t)*8-1)) - 1);
static const gid_t GID_T_MAX = ((1LL << (sizeof(gid_t)*8-1)) - 1);
static const int UID_GID_OVERFLOW_ERRNO = EIO;

/* SETTINGS */
static struct Settings {
    const char *progname;
    struct permchain *permchain; /* permission bit rules. see permchain.h */
    uid_t new_uid; /* user-specified uid */
    gid_t new_gid; /* user-specified gid */
    uid_t create_for_uid;
    gid_t create_for_gid;
    char *mntsrc;
    char *mntdest;
    int mntdest_len; /* caches strlen(mntdest) */
    int mntsrc_fd;

    char *original_working_dir;
    mode_t original_umask;

    UserMap *usermap; /* From the --map option. */
    UserMap *usermap_reverse;

    RateLimiter *read_limiter;
    RateLimiter *write_limiter;

    enum CreatePolicy {
        CREATE_AS_USER,
        CREATE_AS_MOUNTER
    } create_policy;

    struct permchain *create_permchain; /* the --create-with-perms option */

    enum ChownPolicy {
        CHOWN_NORMAL,
        CHOWN_IGNORE,
        CHOWN_DENY
    } chown_policy;

    enum ChgrpPolicy {
        CHGRP_NORMAL,
        CHGRP_IGNORE,
        CHGRP_DENY
    } chgrp_policy;

    enum ChmodPolicy {
        CHMOD_NORMAL,
        CHMOD_IGNORE,
        CHMOD_DENY
    } chmod_policy;

    int chmod_allow_x;

    struct permchain *chmod_permchain; /* the --chmod-filter option */

    enum XAttrPolicy {
        XATTR_UNIMPLEMENTED,
        XATTR_READ_ONLY,
        XATTR_READ_WRITE
    } xattr_policy;

    int delete_deny;
    int rename_deny;

    int mirrored_users_only;
    uid_t *mirrored_users;
    int num_mirrored_users;
    gid_t *mirrored_members;
    int num_mirrored_members;

    int hide_hard_links;
    int resolve_symlinks;

    int block_devices_as_files;

    enum ResolvedSymlinkDeletion {
        RESOLVED_SYMLINK_DELETION_DENY,
        RESOLVED_SYMLINK_DELETION_SYMLINK_ONLY,
        RESOLVED_SYMLINK_DELETION_SYMLINK_FIRST,
        RESOLVED_SYMLINK_DELETION_TARGET_FIRST
    } resolved_symlink_deletion_policy;

    int realistic_permissions;

    int ctime_from_mtime;

    int enable_lock_forwarding;

    int enable_ioctl;
    
    uid_t uid_offset;
    gid_t gid_offset;

} settings;



/* PROTOTYPES */

static int is_mirroring_enabled();

/* Checks whether the uid is to be the mirrored owner of all files. */
static int is_mirrored_user(uid_t uid);

/* Processes the virtual path to a real path. Always free() the result. */
static char *process_path(const char *path, bool resolve_symlinks);

/* The common parts of getattr and fgetattr. */
static int getattr_common(const char *path, struct stat *stbuf);

/* Chowns a new file if necessary. */
static int chown_new_file(const char *path, struct fuse_context *fc, int (*chown_func)(const char*, uid_t, gid_t));

/* Unified implementation of unlink and rmdir. */
static int delete_file(const char *path, int (*target_delete_func)(const char *));

/* Apply offsets with overflow checking. */
static int apply_uid_offset(uid_t *uid);
static int apply_gid_offset(gid_t *gid);
static int unapply_uid_offset(uid_t *uid);
static int unapply_gid_offset(gid_t *gid);

/* FUSE callbacks */
static void *bindfs_init();
static void bindfs_destroy(void *private_data);
static int bindfs_getattr(const char *path, struct stat *stbuf);
static int bindfs_fgetattr(const char *path, struct stat *stbuf,
                           struct fuse_file_info *fi);
static int bindfs_readlink(const char *path, char *buf, size_t size);
static int bindfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                          off_t offset, struct fuse_file_info *fi);
static int bindfs_mknod(const char *path, mode_t mode, dev_t rdev);
static int bindfs_mkdir(const char *path, mode_t mode);
static int bindfs_unlink(const char *path);
static int bindfs_rmdir(const char *path);
static int bindfs_symlink(const char *from, const char *to);
static int bindfs_rename(const char *from, const char *to);
static int bindfs_link(const char *from, const char *to);
static int bindfs_chmod(const char *path, mode_t mode);
static int bindfs_chown(const char *path, uid_t uid, gid_t gid);
static int bindfs_truncate(const char *path, off_t size);
static int bindfs_ftruncate(const char *path, off_t size,
                            struct fuse_file_info *fi);
static int bindfs_utimens(const char *path, const struct timespec tv[2]);
static int bindfs_create(const char *path, mode_t mode, struct fuse_file_info *fi);
static int bindfs_open(const char *path, struct fuse_file_info *fi);
static int bindfs_read(const char *path, char *buf, size_t size, off_t offset,
                       struct fuse_file_info *fi);
static int bindfs_write(const char *path, const char *buf, size_t size,
                        off_t offset, struct fuse_file_info *fi);
static int bindfs_lock(const char *path, struct fuse_file_info *fi, int cmd,
                       struct flock *lock);
static int bindfs_flock(const char *path, struct fuse_file_info *fi, int op);
static int bindfs_ioctl(const char *path, int cmd, void *arg,
                        struct fuse_file_info *fi, unsigned int flags,
                        void *data);
static int bindfs_statfs(const char *path, struct statvfs *stbuf);
static int bindfs_release(const char *path, struct fuse_file_info *fi);
static int bindfs_fsync(const char *path, int isdatasync,
                        struct fuse_file_info *fi);


static void print_usage(const char *progname);

static int process_option(void *data, const char *arg, int key,
                          struct fuse_args *outargs);
static int parse_mirrored_users(char* mirror);
static int parse_user_map(UserMap *map, UserMap *reverse_map, char *spec);
static char *get_working_dir();
static void maybe_stdout_stderr_to_file();

/* Sets up handling of SIGUSR1. */
static void setup_signal_handling();
static void signal_handler(int sig);

static void atexit_func();

static int is_mirroring_enabled()
{
    return settings.num_mirrored_users + settings.num_mirrored_members > 0;
}

static int is_mirrored_user(uid_t uid)
{
    int i;
    for (i = 0; i < settings.num_mirrored_users; ++i) {
        if (settings.mirrored_users[i] == uid) {
            return 1;
        }
    }
    for (i = 0; i < settings.num_mirrored_members; ++i) {
        if (user_belongs_to_group(uid, settings.mirrored_members[i])) {
            return 1;
        }
    }
    return 0;
}


static char *process_path(const char *path, bool resolve_symlinks)
{
    if (path == NULL) { /* possible? */
        errno = EINVAL;
        return NULL;
    }

    while (*path == '/')
        ++path;

    if (*path == '\0')
        path = ".";

    if (resolve_symlinks && settings.resolve_symlinks) {
        char* result = realpath(path, NULL);
        if (result == NULL) {
            if (errno == ENOENT) {
                /* Broken symlink (or missing file). Don't return null because
                   we want to be able to operate on broken symlinks. */
                return strdup(path);
            }
        } else if (strncmp(result, settings.mntdest, settings.mntdest_len) == 0) {
            /* Recursive call. We cannot handle this without deadlocking,
               especially in single-threaded mode. */
            DPRINTF("Denying recursive access to mountpoint `%s'", result);
            free(result);
            errno = EPERM;
            return NULL;
        }
        return result;
    } else {
        return strdup(path);
    }
}

static int getattr_common(const char *procpath, struct stat *stbuf)
{
    struct fuse_context *fc = fuse_get_context();

    /* Copy mtime (file content modification time)
       to ctime (inode/status change time)
       if the user asked for that */
    if (settings.ctime_from_mtime) {
#ifdef HAVE_STAT_NANOSEC
        // TODO: does this work on OS X?
        stbuf->st_ctim = stbuf->st_mtim;
#else
        stbuf->st_ctime = stbuf->st_mtime;
#endif
    }

    /* Possibly map user/group */
    stbuf->st_uid = usermap_get_uid_or_default(settings.usermap, stbuf->st_uid, stbuf->st_uid);
    stbuf->st_gid = usermap_get_gid_or_default(settings.usermap, stbuf->st_gid, stbuf->st_gid);

    if (!apply_uid_offset(&stbuf->st_uid)) {
        return -UID_GID_OVERFLOW_ERRNO;
    }
    if (!apply_gid_offset(&stbuf->st_gid)) {
        return -UID_GID_OVERFLOW_ERRNO;
    }

    /* Report user-defined owner/group if specified */
    if (settings.new_uid != -1)
        stbuf->st_uid = settings.new_uid;
    if (settings.new_gid != -1)
        stbuf->st_gid = settings.new_gid;

    /* Mirrored user? */
    if (is_mirroring_enabled() && is_mirrored_user(fc->uid)) {
        stbuf->st_uid = fc->uid;
    } else if (settings.mirrored_users_only && fc->uid != 0) {
        stbuf->st_mode &= ~0777; /* Deny all access if mirror-only and not root */
        return 0;
    }

    /* Hide hard links */
    if (settings.hide_hard_links) {
        stbuf->st_nlink = 1;
    }

    /* Block files as regular files. */
    if (settings.block_devices_as_files && S_ISBLK(stbuf->st_mode)) {
        stbuf->st_mode ^= S_IFBLK | S_IFREG;  // Flip both bits
#ifdef __LINUX__
        uint64_t size;
        ioctl(file, BLKGETSIZE64, &size);
        stbuf->st_size = (off_t)size;
        if (stbuf->st_size < 0) {  // Underflow
            return -EOVERFLOW;
        }
#else
        int fd = open(procpath, O_RDONLY);
        if (fd == -1) {
            return -errno;
        }
        off_t size = lseek(fd, 0, SEEK_END);
        if (size == (off_t)-1) {
            close(fd);
            return -errno;
        }
        stbuf->st_size = size;
        close(fd);
#endif
    }

    /* Then permission bits. Symlink permissions don't matter, though. */
    if ((stbuf->st_mode & S_IFLNK) != S_IFLNK) {
        /* Apply user-defined permission bit modifications */
        stbuf->st_mode = permchain_apply(settings.permchain, stbuf->st_mode);

        /* Check that we can really do what we promise if --realistic-permissions was given */
        if (settings.realistic_permissions) {
            if (access(procpath, R_OK) == -1)
                stbuf->st_mode &= ~0444;
            if (access(procpath, W_OK) == -1)
                stbuf->st_mode &= ~0222;
            if (access(procpath, X_OK) == -1)
                stbuf->st_mode &= ~0111;
        }
    }

    return 0;
}

/* FIXME: another thread may race to see the old owner before the chown is done.
          Is there a scenario where this compromises security? Or application correctness? */
static int chown_new_file(const char *path, struct fuse_context *fc, int (*chown_func)(const char*, uid_t, gid_t))
{
    uid_t file_owner;
    gid_t file_group;

    if (settings.create_policy == CREATE_AS_USER) {
        char *path_copy;
        const char *dir_path;
        struct stat stbuf;

        file_owner = fc->uid;
        file_group = fc->gid;

        path_copy = strdup(path);
        dir_path = my_dirname(path_copy);
        if (lstat(dir_path, &stbuf) != -1 && stbuf.st_mode & S_ISGID)
            file_group = -1;
        free(path_copy);
    } else {
        file_owner = -1;
        file_group = -1;
    }

    file_owner = usermap_get_uid_or_default(settings.usermap_reverse, fc->uid, file_owner);
    file_group = usermap_get_gid_or_default(settings.usermap_reverse, fc->gid, file_group);

    if (!unapply_uid_offset(&file_owner)) {
        return -UID_GID_OVERFLOW_ERRNO;
    }
    if (!unapply_gid_offset(&file_group)) {
        return -UID_GID_OVERFLOW_ERRNO;
    }

    if (settings.create_for_uid != -1)
        file_owner = settings.create_for_uid;
    if (settings.create_for_gid != -1)
        file_group = settings.create_for_gid;

    if ((file_owner != -1) || (file_group != -1)) {
        if (chown_func(path, file_owner, file_group) == -1) {
            DPRINTF("Failed to chown new file or directory (%d)", errno);
        }
    }

    return 0;
}

static int delete_file(const char *path, int (*target_delete_func)(const char *)) {
    int res;
    char *real_path;
    struct stat st;
    char *also_try_delete = NULL;
    char *unlink_first = NULL;
    int (*main_delete_func)(const char*) = target_delete_func;

     if (settings.delete_deny)
        return -EPERM;

    real_path = process_path(path, false);
    if (real_path == NULL)
        return -errno;

    if (settings.resolve_symlinks) {
        if (lstat(real_path, &st) == -1) {
            free(real_path);
            return -errno;
        }

        if (S_ISLNK(st.st_mode)) {
            switch(settings.resolved_symlink_deletion_policy) {
            case RESOLVED_SYMLINK_DELETION_DENY:
                free(real_path);
                return -EPERM;
            case RESOLVED_SYMLINK_DELETION_SYMLINK_ONLY:
                main_delete_func = &unlink;
                break;
            case RESOLVED_SYMLINK_DELETION_SYMLINK_FIRST:
                main_delete_func = &unlink;

                also_try_delete = realpath(real_path, NULL);
                if (also_try_delete == NULL && errno != ENOENT) {
                    free(real_path);
                    return -errno;
                }
                break;
            case RESOLVED_SYMLINK_DELETION_TARGET_FIRST:
                unlink_first = realpath(real_path, NULL);
                if (unlink_first == NULL && errno != ENOENT) {
                    free(real_path);
                    return -errno;
                }

                if (unlink_first != NULL) {
                    res = unlink(unlink_first);
                    free(unlink_first);
                    if (res == -1) {
                        free(real_path);
                        return -errno;
                    }
                }
                break;
            }
        }
    }

    res = main_delete_func(real_path);
    free(real_path);
    if (res == -1) {
        free(also_try_delete);
        return -errno;
    }

    if (also_try_delete != NULL) {
        (void)target_delete_func(also_try_delete);
        free(also_try_delete);
    }

    return 0;
}

static int apply_uid_offset(uid_t *uid) {
    if (*uid > UID_T_MAX - settings.uid_offset) {
        DPRINTF("UID %lld overflowed while applying offset", (long long)*uid);
        return 0;
    }
    *uid += settings.uid_offset;
    return 1;
}

static int apply_gid_offset(gid_t *gid) {
    if (*gid > GID_T_MAX - settings.gid_offset) {
        DPRINTF("GID %lld overflowed while applying offset", (long long)*gid);
        return 0;
    }
    *gid += settings.gid_offset;
    return 1;
}

static int unapply_uid_offset(uid_t *uid) {
    if (*uid < settings.uid_offset) {
        DPRINTF("UID %lld underflowed while unapplying offset", (long long)*uid);
        return 0;
    }
    *uid -= settings.uid_offset;
    return 1;
}

static int unapply_gid_offset(gid_t *gid) {
  if (*gid < settings.gid_offset) {
        DPRINTF("GID %lld underflowed while unapplying offset", (long long)*gid);
        return 0;
    }
    *gid -= settings.gid_offset;
    return 1;
}


static void *bindfs_init()
{
    assert(settings.permchain != NULL);
    assert(settings.mntsrc_fd > 0);

    maybe_stdout_stderr_to_file();

    if (fchdir(settings.mntsrc_fd) != 0) {
        fprintf(
            stderr,
            "Could not change working directory to '%s': %s\n",
            settings.mntsrc,
            strerror(errno)
            );
        fuse_exit(fuse_get_context()->fuse);
    }

    return NULL;
}

static void bindfs_destroy(void *private_data)
{
}

static int bindfs_getattr(const char *path, struct stat *stbuf)
{
    int res;
    char *real_path;

    real_path = process_path(path, true);
    if (real_path == NULL)
        return -errno;

    if (lstat(real_path, stbuf) == -1) {
        free(real_path);
        return -errno;
    }

    res = getattr_common(real_path, stbuf);
    free(real_path);
    return res;
}

static int bindfs_fgetattr(const char *path, struct stat *stbuf,
                           struct fuse_file_info *fi)
{
    int res;
    char *real_path;

    real_path = process_path(path, true);
    if (real_path == NULL)
        return -errno;

    if (fstat(fi->fh, stbuf) == -1) {
        free(real_path);
        return -errno;
    }
    res = getattr_common(real_path, stbuf);
    free(real_path);
    return res;
}

static int bindfs_readlink(const char *path, char *buf, size_t size)
{
    int res;
    char *real_path;

    real_path = process_path(path, true);
    if (real_path == NULL)
        return -errno;

    /* No need to check for access to the link itself, since symlink
       permissions don't matter. Access to the path components of the symlink
       are automatically queried by FUSE. */

    res = readlink(real_path, buf, size - 1);
    free(real_path);
    if (res == -1)
        return -errno;

    buf[res] = '\0';
    return 0;
}

static int bindfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                          off_t offset, struct fuse_file_info *fi)
{
    char *real_path = process_path(path, true);
    if (real_path == NULL) {
        return -errno;
    }

    DIR *dp = opendir(real_path);
    if (dp == NULL) {
        free(real_path);
        return -errno;
    }

    long pc_ret = pathconf(real_path, _PC_NAME_MAX);
    free(real_path);
    if (pc_ret < 0) {
        DPRINTF("pathconf failed: %s (%d)", strerror(errno), errno);
        pc_ret = NAME_MAX;
    } else if (pc_ret == 0) {
        // Workaround for some source filesystems erroneously returning 0
        // (see issue #54).
        pc_ret = NAME_MAX;
    }

    struct dirent *de_buf =
        malloc(offsetof(struct dirent, d_name) + pc_ret + 1);
    int result = 0;
    while (1) {
        struct dirent *de;
        result = readdir_r(dp, de_buf, &de);
        if (result != 0) {
            result = -result;
            break;
        }
        if (de == NULL) {
            break;
        }

        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_ino = de->d_ino;
        st.st_mode = de->d_type << 12;

        // See issue #28 for why we pass a 0 offset to `filler` and ignore
        // `offset`.
        //
        // Given a 0 offset, `filler` should never return non-zero, so we
        // consider it an error if it does. It is undocumented whether it sets
        // errno in that case, so we zero it first and set it ourself if it
        // doesn't.
        errno = 0;
        if (filler(buf, de->d_name, &st, 0) != 0) {
            result = errno != 0 ? -errno : -EIO;
            break;
        }
    }

    free(de_buf);
    closedir(dp);
    return result;
}

static int bindfs_mknod(const char *path, mode_t mode, dev_t rdev)
{
    int res;
    struct fuse_context *fc;
    char *real_path;

    real_path = process_path(path, true);
    if (real_path == NULL)
        return -errno;

    mode = permchain_apply(settings.create_permchain, mode);

    if (S_ISFIFO(mode))
        res = mkfifo(real_path, mode);
    else
        res = mknod(real_path, mode, rdev);
    if (res == -1) {
        free(real_path);
        return -errno;
    }

    fc = fuse_get_context();
    res = chown_new_file(real_path, fc, &chown);
    free(real_path);

    return res;
}

static int bindfs_mkdir(const char *path, mode_t mode)
{
    int res;
    struct fuse_context *fc;
    char *real_path;

    real_path = process_path(path, true);
    if (real_path == NULL)
        return -errno;

    mode |= S_IFDIR; /* tell permchain_apply this is a directory */
    mode = permchain_apply(settings.create_permchain, mode);

    res = mkdir(real_path, mode & 0777);
    if (res == -1) {
        free(real_path);
        return -errno;
    }

    fc = fuse_get_context();
    res = chown_new_file(real_path, fc, &chown);
    free(real_path);

    return res;
}

static int bindfs_unlink(const char *path)
{
    return delete_file(path, &unlink);
}

static int bindfs_rmdir(const char *path)
{
    return delete_file(path, &rmdir);
}

static int bindfs_symlink(const char *from, const char *to)
{
    int res;
    struct fuse_context *fc;
    char *real_to;

    if (settings.resolve_symlinks)
        return -EPERM;

    real_to = process_path(to, false);
    if (real_to == NULL)
        return -errno;

    res = symlink(from, real_to);
    if (res == -1) {
        free(real_to);
        return -errno;
    }

    fc = fuse_get_context();
    res = chown_new_file(real_to, fc, &lchown);
    free(real_to);

    return res;
}

static int bindfs_rename(const char *from, const char *to)
{
    int res;
    char *real_from, *real_to;

    if (settings.rename_deny)
        return -EPERM;

    real_from = process_path(from, false);
    if (real_from == NULL)
        return -errno;

    real_to = process_path(to, true);
    if (real_to == NULL) {
        free(real_from);
        return -errno;
    }

    res = rename(real_from, real_to);
    free(real_from);
    free(real_to);
    if (res == -1)
        return -errno;

    return 0;
}

static int bindfs_link(const char *from, const char *to)
{
    int res;
    char *real_from, *real_to;

    real_from = process_path(from, true);
    if (real_from == NULL)
        return -errno;

    real_to = process_path(to, true);
    if (real_to == NULL) {
        free(real_from);
        return -errno;
    }

    res = link(real_from, real_to);
    free(real_from);
    free(real_to);
    if (res == -1)
        return -errno;

    return 0;
}

static int bindfs_chmod(const char *path, mode_t mode)
{
    int file_execute_only = 0;
    struct stat st;
    mode_t diff = 0;
    char *real_path;

    real_path = process_path(path, true);
    if (real_path == NULL)
        return -errno;

    if (settings.chmod_allow_x) {
        /* Get the old permission bits and see which bits would change. */
        if (lstat(real_path, &st) == -1) {
            free(real_path);
            return -errno;
        }

        if (S_ISREG(st.st_mode)) {
            diff = (st.st_mode & 07777) ^ (mode & 07777);
            file_execute_only = 1;
        }
    }

    switch (settings.chmod_policy) {
    case CHMOD_NORMAL:
        mode = permchain_apply(settings.chmod_permchain, mode);
        if (chmod(real_path, mode) == -1) {
            free(real_path);
            return -errno;
        }
        free(real_path);
        return 0;
    case CHMOD_IGNORE:
        if (file_execute_only) {
            diff &= 00111; /* See which execute bits were flipped.
                              Forget about other differences. */
            if (chmod(real_path, st.st_mode ^ diff) == -1) {
                free(real_path);
                return -errno;
            }
        }
        free(real_path);
        return 0;
    case CHMOD_DENY:
        if (file_execute_only) {
            if ((diff & 07666) == 0) {
                /* Only execute bits have changed, so we can allow this. */
                if (chmod(real_path, mode) == -1) {
                    free(real_path);
                    return -errno;
                }
                free(real_path);
                return 0;
            }
        }
        free(real_path);
        return -EPERM;
    default:
        assert(0);
    }
}

static int bindfs_chown(const char *path, uid_t uid, gid_t gid)
{
    int res;
    char *real_path;

    if (uid != -1) {
        switch (settings.chown_policy) {
        case CHOWN_NORMAL:
            uid = usermap_get_uid_or_default(settings.usermap_reverse, uid, uid);
            if (!unapply_uid_offset(&uid)) {
                return -UID_GID_OVERFLOW_ERRNO;
            }
            break;
        case CHOWN_IGNORE:
            uid = -1;
            break;
        case CHOWN_DENY:
            return -EPERM;
        }
    }

    if (gid != -1) {
        switch (settings.chgrp_policy) {
        case CHGRP_NORMAL:
            gid = usermap_get_gid_or_default(settings.usermap_reverse, gid, gid);
            if (!unapply_gid_offset(&gid)) {
                return -UID_GID_OVERFLOW_ERRNO;
            }
            break;
        case CHGRP_IGNORE:
            gid = -1;
            break;
        case CHGRP_DENY:
            return -EPERM;
        }
    }

    if (uid != -1 || gid != -1) {
        real_path = process_path(path, true);
        if (real_path == NULL)
            return -errno;

        res = lchown(real_path, uid, gid);
        free(real_path);
        if (res == -1)
            return -errno;
    }

    return 0;
}

static int bindfs_truncate(const char *path, off_t size)
{
    int res;
    char *real_path;

    real_path = process_path(path, true);
    if (real_path == NULL)
        return -errno;

    res = truncate(real_path, size);
    free(real_path);
    if (res == -1)
        return -errno;

    return 0;
}

static int bindfs_ftruncate(const char *path, off_t size,
                            struct fuse_file_info *fi)
{
    int res;
    (void) path;

    res = ftruncate(fi->fh, size);
    if (res == -1)
        return -errno;

    return 0;
}

static int bindfs_utimens(const char *path, const struct timespec ts[2])
{
    int res;
    char *real_path;

    real_path = process_path(path, true);
    if (real_path == NULL)
        return -errno;

#ifdef HAVE_UTIMENSAT
    res = utimensat(settings.mntsrc_fd, real_path, ts, AT_SYMLINK_NOFOLLOW);
#elif HAVE_LUTIMES
    struct timeval tv[2];
    tv[0].tv_sec = ts[0].tv_sec;
    tv[0].tv_usec = ts[0].tv_nsec / 1000;
    tv[1].tv_sec = ts[1].tv_sec;
    tv[1].tv_usec = ts[1].tv_nsec / 1000;
    res = lutimes(real_path, tv);
#else
#error "No symlink-compatible utime* function available."
#endif
 
    free(real_path);
    if (res == -1)
        return -errno;

    return 0;
}

static int bindfs_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    int fd;
    struct fuse_context *fc;
    char *real_path;

    real_path = process_path(path, true);
    if (real_path == NULL)
        return -errno;

    mode |= S_IFREG; /* tell permchain_apply this is a regular file */
    mode = permchain_apply(settings.create_permchain, mode);

    fd = open(real_path, fi->flags, mode & 0777);
    if (fd == -1) {
        free(real_path);
        return -errno;
    }

    fc = fuse_get_context();
    chown_new_file(real_path, fc, &chown);
    free(real_path);

    fi->fh = fd;
    return 0;
}

static int bindfs_open(const char *path, struct fuse_file_info *fi)
{
    int fd;
    char *real_path;

    real_path = process_path(path, true);
    if (real_path == NULL)
        return -errno;

    fd = open(real_path, fi->flags);
    free(real_path);
    if (fd == -1)
        return -errno;

    fi->fh = fd;
    return 0;
}

static int bindfs_read(const char *path, char *buf, size_t size, off_t offset,
                       struct fuse_file_info *fi)
{
    int res;
    (void) path;

    if (settings.read_limiter) {
        rate_limiter_wait(settings.read_limiter, size);
    }

    res = pread(fi->fh, buf, size, offset);
    if (res == -1)
        res = -errno;

    return res;
}

static int bindfs_write(const char *path, const char *buf, size_t size,
                        off_t offset, struct fuse_file_info *fi)
{
    int res;
    (void) path;

    if (settings.write_limiter) {
        rate_limiter_wait(settings.write_limiter, size);
    }

    res = pwrite(fi->fh, buf, size, offset);
    if (res == -1)
        res = -errno;

    return res;
}

/* This callback is only installed if lock forwarding is enabled. */
static int bindfs_lock(const char *path, struct fuse_file_info *fi, int cmd,
                       struct flock *lock)
{
  int res = fcntl(fi->fh, cmd, lock);
  if (res == -1) {
    return -errno;
  }
  return 0;
}

/* This callback is only installed if lock forwarding is enabled. */
static int bindfs_flock(const char *path, struct fuse_file_info *fi, int op)
{
    int res = flock(fi->fh, op);
    if (res == -1) {
        return -errno;
    }
    return 0;
}

static int bindfs_ioctl(const char *path, int cmd, void *arg,
                        struct fuse_file_info *fi, unsigned int flags,
                        void *data)
{
    int res = ioctl(fi->fh, cmd, data);
    if (res == -1) {
      return -errno;
    }
    return res;
}

static int bindfs_statfs(const char *path, struct statvfs *stbuf)
{
    int res;
    char *real_path;

    real_path = process_path(path, true);
    if (real_path == NULL)
        return -errno;

    res = statvfs(real_path, stbuf);
    free(real_path);
    if (res == -1)
        return -errno;

    return 0;
}

static int bindfs_release(const char *path, struct fuse_file_info *fi)
{
    (void) path;

    close(fi->fh);

    return 0;
}

static int bindfs_fsync(const char *path, int isdatasync,
                        struct fuse_file_info *fi)
{
    int res;
    (void) path;

#ifndef HAVE_FDATASYNC
    (void) isdatasync;
#else
    if (isdatasync)
        res = fdatasync(fi->fh);
    else
#endif
        res = fsync(fi->fh);
    if (res == -1)
        return -errno;

    return 0;
}

#ifdef HAVE_SETXATTR
/* The disgusting __APPLE__ sections below were copied without much
   understanding from the osxfuse example file:
   https://github.com/osxfuse/fuse/blob/master/example/fusexmp_fh.c */

#ifdef __APPLE__
static int bindfs_setxattr(const char *path, const char *name, const char *value,
                           size_t size, int flags, uint32_t position)
#else
static int bindfs_setxattr(const char *path, const char *name, const char *value,
                           size_t size, int flags)
#endif
{
    int res;
    char *real_path;

    DPRINTF("setxattr %s %s=%s", path, name, value);

    if (settings.xattr_policy == XATTR_READ_ONLY)
        return -EACCES;

    real_path = process_path(path, true);
    if (real_path == NULL)
        return -errno;

#if defined(__APPLE__)
    if (!strncmp(name, XATTR_APPLE_PREFIX, sizeof(XATTR_APPLE_PREFIX) - 1)) {
        flags &= ~(XATTR_NOSECURITY);
    }
    flags |= XATTR_NOFOLLOW;  // TODO: check if this is actually correct and necessary
    if (!strcmp(name, A_KAUTH_FILESEC_XATTR)) {
        char new_name[MAXPATHLEN];
        memcpy(new_name, A_KAUTH_FILESEC_XATTR, sizeof(A_KAUTH_FILESEC_XATTR));
        memcpy(new_name, G_PREFIX, sizeof(G_PREFIX) - 1);
        res = setxattr(real_path, new_name, value, size, position, flags);
    } else {
        res = setxattr(real_path, name, value, size, position, flags);
    }
#elif defined(HAVE_LSETXATTR)
    res = lsetxattr(real_path, name, value, size, flags);
#else
    res = setxattr(real_path, name, value, size, 0, flags | XATTR_NOFOLLOW);
#endif

    free(real_path);
    if (res == -1)
        return -errno;
    return 0;
}

#ifdef __APPLE__
static int bindfs_getxattr(const char *path, const char *name, char *value,
                           size_t size, uint32_t position)
#else
static int bindfs_getxattr(const char *path, const char *name, char *value,
                           size_t size)
#endif
{
    int res;
    char *real_path;

    DPRINTF("getxattr %s %s", path, name);

    real_path = process_path(path, true);
    if (real_path == NULL)
        return -errno;

#if defined(__APPLE__)
    if (strcmp(name, A_KAUTH_FILESEC_XATTR) == 0) {
        char new_name[MAXPATHLEN];
        memcpy(new_name, A_KAUTH_FILESEC_XATTR, sizeof(A_KAUTH_FILESEC_XATTR));
        memcpy(new_name, G_PREFIX, sizeof(G_PREFIX) - 1);
        res = getxattr(real_path, new_name, value, size, position, XATTR_NOFOLLOW);
    } else {
        res = getxattr(real_path, name, value, size, position, XATTR_NOFOLLOW);
    }
#elif defined(HAVE_LGETXATTR)
    res = lgetxattr(real_path, name, value, size);
#else
    res = getxattr(real_path, name, value, size, 0, XATTR_NOFOLLOW);
#endif
    free(real_path);
    if (res == -1)
        return -errno;
    return res;
}

static int bindfs_listxattr(const char *path, char* list, size_t size)
{
    char *real_path;

    DPRINTF("listxattr %s", path);

    real_path = process_path(path, true);
    if (real_path == NULL)
        return -errno;

#if defined(__APPLE__)
    ssize_t res = listxattr(real_path, list, size, XATTR_NOFOLLOW);
    if (res > 0) {
        if (list) {
            size_t len = 0;
            char* curr = list;
            do {
                size_t thislen = strlen(curr) + 1;
                if (strcmp(curr, G_KAUTH_FILESEC_XATTR) == 0) {
                    memmove(curr, curr + thislen, res - len - thislen);
                    res -= thislen;
                    break;
                }
                curr += thislen;
                len += thislen;
            } while (len < res);
        } else {
            // TODO: https://github.com/osxfuse/fuse/blob/master/example/fusexmp_fh.c
            // had this commented out bit here o_O
            /*
            ssize_t res2 = getxattr(real_path, G_KAUTH_FILESEC_XATTR, NULL, 0, 0,
                                    XATTR_NOFOLLOW);
            if (res2 >= 0) {
                res -= sizeof(G_KAUTH_FILESEC_XATTR);
            }
            */
        }
    }
#elif defined(HAVE_LLISTXATTR)
    int res = llistxattr(real_path, list, size);
#else
    int res = listxattr(real_path, list, size, XATTR_NOFOLLOW);
#endif
    free(real_path);
    if (res == -1)
        return -errno;
    return res;
}

static int bindfs_removexattr(const char *path, const char *name)
{
    int res;
    char *real_path;

    DPRINTF("removexattr %s %s", path, name);

    if (settings.xattr_policy == XATTR_READ_ONLY)
        return -EACCES;

    real_path = process_path(path, true);
    if (real_path == NULL)
        return -errno;

#if defined(__APPLE__)
    if (strcmp(name, A_KAUTH_FILESEC_XATTR) == 0) {
        char new_name[MAXPATHLEN];
        memcpy(new_name, A_KAUTH_FILESEC_XATTR, sizeof(A_KAUTH_FILESEC_XATTR));
        memcpy(new_name, G_PREFIX, sizeof(G_PREFIX) - 1);
        res = removexattr(real_path, new_name, XATTR_NOFOLLOW);
    } else {
        res = removexattr(real_path, name, XATTR_NOFOLLOW);
    }
#elif defined(HAVE_LREMOVEXATTR)
    res = lremovexattr(real_path, name);
#else
    res = removexattr(real_path, name, XATTR_NOFOLLOW);
#endif

    free(real_path);
    if (res == -1)
        return -errno;
    return 0;
}
#endif /* HAVE_SETXATTR */


static struct fuse_operations bindfs_oper = {
    .init       = bindfs_init,
    .destroy    = bindfs_destroy,
    .getattr    = bindfs_getattr,
    .fgetattr   = bindfs_fgetattr,
    /* no access() since we always use -o default_permissions */
    .readlink   = bindfs_readlink,
    .readdir    = bindfs_readdir,
    .mknod      = bindfs_mknod,
    .mkdir      = bindfs_mkdir,
    .symlink    = bindfs_symlink,
    .unlink     = bindfs_unlink,
    .rmdir      = bindfs_rmdir,
    .rename     = bindfs_rename,
    .link       = bindfs_link,
    .chmod      = bindfs_chmod,
    .chown      = bindfs_chown,
    .truncate   = bindfs_truncate,
    .ftruncate  = bindfs_ftruncate,
    .utimens    = bindfs_utimens,
    .create     = bindfs_create,
    .open       = bindfs_open,
    .read       = bindfs_read,
    .write      = bindfs_write,
#ifdef HAVE_FUSE_29
    .lock       = bindfs_lock,
    .flock      = bindfs_flock,
#endif
    .ioctl      = bindfs_ioctl,
    .statfs     = bindfs_statfs,
    .release    = bindfs_release,
    .fsync      = bindfs_fsync,
#ifdef HAVE_SETXATTR
    .setxattr   = bindfs_setxattr,
    .getxattr   = bindfs_getxattr,
    .listxattr  = bindfs_listxattr,
    .removexattr= bindfs_removexattr,
#endif
};

static void print_usage(const char *progname)
{
    if (progname == NULL)
        progname = "bindfs";

    printf("\n"
           "Usage: %s [options] dir mountpoint\n"
           "Information:\n"
           "  -h      --help            Print this and exit.\n"
           "  -V      --version         Print version number and exit.\n"
           "\n"
           "File ownership:\n"
           "  -u      --force-user=...  Set file owner.\n"
           "  -g      --force-group=... Set file group.\n"
           "  -m      --mirror=...      Comma-separated list of users who will see\n"
           "                            themselves as the owners of all files.\n"
           "  -M      --mirror-only=... Like --mirror but disallow access for\n"
           "                            all other users.\n"
           " --map=user1/user2:...      Let user2 see files of user1 as his own.\n"
           " --uid-offset=...           Set file uid = uid + offset.\n"
           " --gid-offset=...           Set file gid = gid + offset.\n"
           "\n"
           "Permission bits:\n"
           "  -p      --perms=...       Specify permissions, similar to chmod\n"
           "                            e.g. og-x,og+rD,u=rwX,g+rw  or  0644,a+X\n"
           "\n"
           "File creation policy:\n"
           "  --create-as-user          New files owned by creator (default for root). *\n"
           "  --create-as-mounter       New files owned by fs mounter (default for users).\n"
           "  --create-for-user=...     New files owned by specified user. *\n"
           "  --create-for-group=...    New files owned by specified group. *\n"
           "  --create-with-perms=...   Alter permissions of new files.\n"
           "\n"
           "Chown policy:\n"
           "  --chown-normal            Try to chown the original files (the default).\n"
           "  --chown-ignore            Have all chowns fail silently.\n"
           "  --chown-deny              Have all chowns fail with 'permission denied'.\n"
           "\n"
           "Chgrp policy:\n"
           "  --chgrp-normal            Try to chgrp the original files (the default).\n"
           "  --chgrp-ignore            Have all chgrps fail silently.\n"
           "  --chgrp-deny              Have all chgrps fail with 'permission denied'.\n"
           "\n"
           "Chmod policy:\n"
           "  --chmod-normal            Try to chmod the original files (the default).\n"
           "  --chmod-ignore            Have all chmods fail silently.\n"
           "  --chmod-deny              Have all chmods fail with 'permission denied'.\n"
           "  --chmod-filter=...        Change permissions of chmod requests.\n"
           "  --chmod-allow-x           Allow changing file execute bits in any case.\n"
           "\n"
           "Extended attribute policy:\n"
           "  --xattr-none              Do not implement xattr operations.\n"
           "  --xattr-ro                Read-only xattr operations.\n"
           "  --xattr-rw                Read-write xattr operations (the default).\n"
           "\n"
           "Other file operations:\n"
           "  --delete-deny             Disallow deleting files.\n"
           "  --rename-deny             Disallow renaming files (within the mount).\n"
           "\n"
           "Rate limits:\n"
           "  --read-rate=...           Limit to bytes/sec that can be read.\n"
           "  --write-rate=...          Limit to bytes/sec that can be written.\n"
           "\n"
           "Miscellaneous:\n"
           "  --no-allow-other          Do not add -o allow_other to fuse options.\n"
           "  --realistic-permissions   Hide permission bits for actions mounter can't do.\n"
           "  --ctime-from-mtime        Read file properties' change time\n"
           "                            from file content modification time.\n"
           "  --enable-lock-forwarding  Forward locks to the underlying FS.\n"
           "  --enable-ioctl            Forward ioctl() calls (as the mounter).\n"
           "  --hide-hard-links         Always report a hard link count of 1.\n"
           "  --resolve-symlinks        Resolve symbolic links.\n"
           "  --resolved-symlink-deletion=...  Decide how to delete resolved symlinks.\n"
           "  --block-devices-as-files  Show block devices as regular files.\n"
           "  --no-user-group-precaching Don't read all users and groups at startup.\n"
           "  --multithreaded           Enable multithreaded mode. See man page\n"
           "                            for security issue with current implementation.\n"
           "\n"
           "FUSE options:\n"
           "  -o opt[,opt,...]          Mount options.\n"
           "  -r      -o ro             Mount strictly read-only.\n"
           "  -d      -o debug          Enable debug output (implies -f).\n"
           "  -f                        Foreground operation.\n"
           "\n"
           "(*: root only)\n"
           "\n",
           progname);
}


enum OptionKey {
    OPTKEY_NONOPTION = -2,
    OPTKEY_UNKNOWN = -1,
    OPTKEY_HELP,
    OPTKEY_VERSION,
    OPTKEY_CREATE_AS_USER,
    OPTKEY_CREATE_AS_MOUNTER,
    OPTKEY_CHOWN_NORMAL,
    OPTKEY_CHOWN_IGNORE,
    OPTKEY_CHOWN_DENY,
    OPTKEY_CHGRP_NORMAL,
    OPTKEY_CHGRP_IGNORE,
    OPTKEY_CHGRP_DENY,
    OPTKEY_CHMOD_NORMAL,
    OPTKEY_CHMOD_IGNORE,
    OPTKEY_CHMOD_DENY,
    OPTKEY_CHMOD_ALLOW_X,
    OPTKEY_XATTR_NONE,
    OPTKEY_XATTR_READ_ONLY,
    OPTKEY_XATTR_READ_WRITE,
    OPTKEY_DELETE_DENY,
    OPTKEY_RENAME_DENY,
    OPTKEY_REALISTIC_PERMISSIONS,
    OPTKEY_CTIME_FROM_MTIME,
    OPTKEY_ENABLE_LOCK_FORWARDING,
    OPTKEY_DISABLE_LOCK_FORWARDING,
    OPTKEY_ENABLE_IOCTL,
    OPTKEY_HIDE_HARD_LINKS,
    OPTKEY_RESOLVE_SYMLINKS,
    OPTKEY_BLOCK_DEVICES_AS_FILES
};

static int process_option(void *data, const char *arg, int key,
                          struct fuse_args *outargs)
{
    switch ((enum OptionKey)key)
    {
    case OPTKEY_HELP:
        print_usage(my_basename(settings.progname));
        exit(0);

    case OPTKEY_VERSION:
        printf("%s\n", PACKAGE_STRING);
        exit(0);

    case OPTKEY_CREATE_AS_USER:
        if (getuid() == 0) {
            settings.create_policy = CREATE_AS_USER;
        } else {
            fprintf(stderr, "Error: You need to be root to use --create-as-user !\n");
            return -1;
        }
        return 0;
    case OPTKEY_CREATE_AS_MOUNTER:
        settings.create_policy = CREATE_AS_MOUNTER;
        return 0;

    case OPTKEY_CHOWN_NORMAL:
        settings.chown_policy = CHOWN_NORMAL;
        return 0;
    case OPTKEY_CHOWN_IGNORE:
        settings.chown_policy = CHOWN_IGNORE;
        return 0;
    case OPTKEY_CHOWN_DENY:
        settings.chown_policy = CHOWN_DENY;
        return 0;

    case OPTKEY_CHGRP_NORMAL:
        settings.chgrp_policy = CHGRP_NORMAL;
        return 0;
    case OPTKEY_CHGRP_IGNORE:
        settings.chgrp_policy = CHGRP_IGNORE;
        return 0;
    case OPTKEY_CHGRP_DENY:
        settings.chgrp_policy = CHGRP_DENY;
        return 0;

    case OPTKEY_CHMOD_NORMAL:
        settings.chmod_policy = CHMOD_NORMAL;
        return 0;
    case OPTKEY_CHMOD_IGNORE:
        settings.chmod_policy = CHMOD_IGNORE;
        return 0;
    case OPTKEY_CHMOD_DENY:
        settings.chmod_policy = CHMOD_DENY;
        return 0;

    case OPTKEY_CHMOD_ALLOW_X:
        settings.chmod_allow_x = 1;
        return 0;

    case OPTKEY_XATTR_NONE:
        settings.xattr_policy = XATTR_UNIMPLEMENTED;
        return 0;
    case OPTKEY_XATTR_READ_ONLY:
        settings.xattr_policy = XATTR_READ_ONLY;
        return 0;
    case OPTKEY_XATTR_READ_WRITE:
        settings.xattr_policy = XATTR_READ_WRITE;
        return 0;

    case OPTKEY_DELETE_DENY:
        settings.delete_deny = 1;
        return 0;
    case OPTKEY_RENAME_DENY:
        settings.rename_deny= 1;
        return 0;

    case OPTKEY_REALISTIC_PERMISSIONS:
        settings.realistic_permissions = 1;
        return 0;
    case OPTKEY_CTIME_FROM_MTIME:
        settings.ctime_from_mtime = 1;
        return 0;
    case OPTKEY_ENABLE_LOCK_FORWARDING:
        settings.enable_lock_forwarding = 1;
        return 0;
    case OPTKEY_DISABLE_LOCK_FORWARDING:
        settings.enable_lock_forwarding = 0;
        return 0;
    case OPTKEY_ENABLE_IOCTL:
        settings.enable_ioctl = 1;
        return 0;
    case OPTKEY_HIDE_HARD_LINKS:
        settings.hide_hard_links = 1;
        return 0;
    case OPTKEY_RESOLVE_SYMLINKS:
        settings.resolve_symlinks = 1;
        return 0;
    case OPTKEY_BLOCK_DEVICES_AS_FILES:
        settings.block_devices_as_files = 1;
        return 0;

    case OPTKEY_NONOPTION:
        if (!settings.mntsrc) {
            settings.mntsrc = realpath(arg, NULL);
            if (settings.mntsrc == NULL) {
                fprintf(stderr, "Failed to resolve source directory `%s': ", arg);
                perror(NULL);
                return -1;
            }
            return 0;
        } else if (!settings.mntdest) {
            settings.mntdest = realpath(arg, NULL);
            if (settings.mntdest == NULL) {
                fprintf(stderr, "Failed to resolve mount point `%s': ", arg);
                perror(NULL);
                return -1;
            }
            settings.mntdest_len = strlen(settings.mntdest);
            return 1; /* leave this argument for fuse_main */
        } else {
            fprintf(stderr, "Too many arguments given\n");
            return -1;
        }

    default:
        return 1;
    }
}

static int parse_mirrored_users(char* mirror)
{
    int i;
    int j;
    char *p, *tmpstr;

    settings.num_mirrored_users = count_chars(mirror, ',') +
                                  count_chars(mirror, ':') + 1;
    settings.num_mirrored_members = ((*mirror == '@') ? 1 : 0) +
                                    count_substrs(mirror, ",@") +
                                    count_substrs(mirror, ":@");
    settings.num_mirrored_users -= settings.num_mirrored_members;
    settings.mirrored_users = malloc(settings.num_mirrored_users*sizeof(uid_t));
    settings.mirrored_members = malloc(settings.num_mirrored_members*sizeof(gid_t));

    i = 0; /* iterate over mirrored_users */
    j = 0; /* iterate over mirrored_members */
    p = mirror;
    while (i < settings.num_mirrored_users || j < settings.num_mirrored_members) {
        tmpstr = strdup_until(p, ",:");

        if (*tmpstr == '@') { /* This is a group name */
            if (!group_gid(tmpstr + 1, &settings.mirrored_members[j++])) {
                fprintf(stderr, "Invalid group ID: '%s'\n", tmpstr + 1);
                free(tmpstr);
                return 0;
            }
        } else {
            if (!user_uid(tmpstr, &settings.mirrored_users[i++])) {
                fprintf(stderr, "Invalid user ID: '%s'\n", tmpstr);
                free(tmpstr);
                return 0;
            }
        }
        free(tmpstr);

        while (*p != '\0' && *p != ',' && *p != ':') {
            ++p;
        }
        if (*p != '\0') {
            ++p;
        } else {
            /* Done. The counters should match. */
            assert(i == settings.num_mirrored_users);
            assert(j == settings.num_mirrored_members);
        }
    }

    return 1;
}

static int parse_user_map(UserMap *map, UserMap *reverse_map, char *spec)
{
    char *p = spec;
    char *tmpstr = NULL;
    char *q;
    uid_t uid_from, uid_to;
    gid_t gid_from, gid_to;
    UsermapStatus status;

    while (*p != '\0') {
        free(tmpstr);
        tmpstr = strdup_until(p, ",:");

        if (tmpstr[0] == '@') { /* group */
            q = strstr(tmpstr, "/@");
            if (!q) {
                fprintf(stderr, "Invalid syntax: expected @group1/@group2 but got `%s`\n", tmpstr);
                goto fail;
            }
            *q = '\0';
            if (!group_gid(tmpstr + 1, &gid_from)) {
                fprintf(stderr, "Invalid group: %s\n", tmpstr + 1);
                goto fail;
            }
            q += strlen("/@");
            if (!group_gid(q, &gid_to)) {
                fprintf(stderr, "Invalid group: %s\n", q);
                goto fail;
            }

            status = usermap_add_gid(map, gid_from, gid_to);
            if (status != 0) {
                fprintf(stderr, "%s\n", usermap_errorstr(status));
                goto fail;
            }
            status = usermap_add_gid(reverse_map, gid_to, gid_from);
            if (status != 0) {
                fprintf(stderr, "%s\n", usermap_errorstr(status));
                goto fail;
            }

        } else {

            q = strstr(tmpstr, "/");
            if (!q) {
                fprintf(stderr, "Invalid syntax: expected user1/user2 but got `%s`\n", tmpstr);
                goto fail;
            }
            *q = '\0';
            if (!user_uid(tmpstr, &uid_from)) {
                fprintf(stderr, "Invalid username: %s\n", tmpstr);
                goto fail;
            }
            q += strlen("/");
            if (!user_uid(q, &uid_to)) {
                fprintf(stderr, "Invalid username: %s\n", q);
                goto fail;
            }

            status = usermap_add_uid(map, uid_from, uid_to);
            if (status != 0) {
                fprintf(stderr, "%s\n", usermap_errorstr(status));
                goto fail;
            }
            status = usermap_add_uid(reverse_map, uid_to, uid_from);
            if (status != 0) {
                fprintf(stderr, "%s\n", usermap_errorstr(status));
                goto fail;
            }
        }

        while (*p != '\0' && *p != ',' && *p != ':') {
            ++p;
        }
        if (*p != '\0') {
            ++p;
        }
    }

    free(tmpstr);
    return 1;

fail:
    free(tmpstr);
    return 0;
}

static void maybe_stdout_stderr_to_file()
{
    /* TODO: make this a command line option. */
#if 0
    int fd;

    const char *filename = "bindfs.log";
    char *path = malloc(strlen(settings.original_working_dir) + 1 + strlen(filename) + 1);
    strcpy(path, settings.original_working_dir);
    strcat(path, "/");
    strcat(path, filename);

    fd = open(path, O_CREAT | O_WRONLY, 0666);
    free(path);

    fchmod(fd, 0777 & ~settings.original_umask);
    fflush(stdout);
    fflush(stderr);
    dup2(fd, 1);
    dup2(fd, 2);
#endif
}

static char *get_working_dir()
{
    size_t buf_size = 4096;
    char* buf = malloc(buf_size);
    while (!getcwd(buf, buf_size)) {
        buf_size *= 2;
        buf = realloc(buf, buf_size);
    }
    return buf;
}

static void setup_signal_handling()
{
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    sigaction(SIGUSR1, &sa, NULL);
}

static void signal_handler(int sig)
{
    invalidate_user_caches();
}

static void atexit_func()
{
    // This cleanup is mostly unnecessary, but we do it anyway to
    // reduce the "still reachable" allocations that Valgrind shows.

    free(settings.mntsrc);
    free(settings.mntdest);
    free(settings.original_working_dir);
    settings.original_working_dir = NULL;
    if (settings.read_limiter) {
        rate_limiter_destroy(settings.read_limiter);
        free(settings.read_limiter);
        settings.read_limiter = NULL;
    }
    if (settings.write_limiter) {
        rate_limiter_destroy(settings.write_limiter);
        free(settings.write_limiter);
        settings.write_limiter = NULL;
    }
    usermap_destroy(settings.usermap);
    settings.usermap = NULL;
    usermap_destroy(settings.usermap_reverse);
    settings.usermap_reverse = NULL;
    permchain_destroy(settings.permchain);
    settings.permchain = NULL;
    permchain_destroy(settings.create_permchain);
    settings.create_permchain = NULL;
    permchain_destroy(settings.chmod_permchain);
    settings.chmod_permchain = NULL;
    free(settings.mirrored_users);
    settings.mirrored_users = NULL;
    free(settings.mirrored_members);
    settings.mirrored_members = NULL;

    clear_user_caches();
}

int main(int argc, char *argv[])
{
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

    /* Fuse's option parser will store things here. */
    struct OptionData {
        char *user;
        char *deprecated_user;
        char *group;
        char *deprecated_group;
        char *perms;
        char *mirror;
        char *mirror_only;
        char *map;
        char *read_rate;
        char *write_rate;
        char *create_for_user;
        char *create_for_group;
        char *create_with_perms;
        char *chmod_filter;
        char *resolved_symlink_deletion;
        int no_allow_other;
        int no_user_group_precaching;
        int multithreaded;
        char *uid_offset;
        char *gid_offset;
    } od;

    #define OPT2(one, two, key) \
            FUSE_OPT_KEY(one, key), \
            FUSE_OPT_KEY(two, key)
    #define OPT_OFFSET2(one, two, offset, key) \
            {one, offsetof(struct OptionData, offset), key}, \
            {two, offsetof(struct OptionData, offset), key}
    #define OPT_OFFSET3(one, two, three, offset, key) \
            {one, offsetof(struct OptionData, offset), key}, \
            {two, offsetof(struct OptionData, offset), key}, \
            {three, offsetof(struct OptionData, offset), key}
    static const struct fuse_opt options[] = {
        OPT2("-h", "--help", OPTKEY_HELP),
        OPT2("-V", "--version", OPTKEY_VERSION),

        OPT_OFFSET3("-u %s", "--force-user=%s", "force-user=%s", user, -1),
        OPT_OFFSET3("-g %s", "--force-group=%s", "force-group=%s", group, -1),

        OPT_OFFSET3("--user=%s", "--owner=%s", "owner=%s", deprecated_user, -1),
        OPT_OFFSET2("--group=%s", "group=%s", deprecated_group, -1),

        OPT_OFFSET3("-p %s", "--perms=%s", "perms=%s", perms, -1),
        OPT_OFFSET3("-m %s", "--mirror=%s", "mirror=%s", mirror, -1),
        OPT_OFFSET3("-M %s", "--mirror-only=%s", "mirror-only=%s", mirror_only, -1),
        OPT_OFFSET2("--map=%s", "map=%s", map, -1),
        OPT_OFFSET3("-n", "--no-allow-other", "no-allow-other", no_allow_other, -1),

        OPT_OFFSET2("--read-rate=%s", "read-rate=%s", read_rate, -1),
        OPT_OFFSET2("--write-rate=%s", "write-rate=%s", write_rate, -1),

        OPT2("--create-as-user", "create-as-user", OPTKEY_CREATE_AS_USER),
        OPT2("--create-as-mounter", "create-as-mounter", OPTKEY_CREATE_AS_MOUNTER),
        OPT_OFFSET2("--create-for-user=%s", "create-for-user=%s", create_for_user, -1),
        OPT_OFFSET2("--create-for-group=%s", "create-for-group=%s", create_for_group, -1),
        OPT_OFFSET2("--create-with-perms=%s", "create-with-perms=%s", create_with_perms, -1),

        OPT2("--chown-normal", "chown-normal", OPTKEY_CHOWN_NORMAL),
        OPT2("--chown-ignore", "chown-ignore", OPTKEY_CHOWN_IGNORE),
        OPT2("--chown-deny", "chown-deny", OPTKEY_CHOWN_DENY),

        OPT2("--chgrp-normal", "chgrp-normal", OPTKEY_CHGRP_NORMAL),
        OPT2("--chgrp-ignore", "chgrp-ignore", OPTKEY_CHGRP_IGNORE),
        OPT2("--chgrp-deny", "chgrp-deny", OPTKEY_CHGRP_DENY),

        OPT2("--chmod-normal", "chmod-normal", OPTKEY_CHMOD_NORMAL),
        OPT2("--chmod-ignore", "chmod-ignore", OPTKEY_CHMOD_IGNORE),
        OPT2("--chmod-deny", "chmod-deny", OPTKEY_CHMOD_DENY),
        OPT_OFFSET2("--chmod-filter=%s", "chmod-filter=%s", chmod_filter, -1),
        OPT2("--chmod-allow-x", "chmod-allow-x", OPTKEY_CHMOD_ALLOW_X),

        OPT2("--xattr-none", "xattr-none", OPTKEY_XATTR_NONE),
        OPT2("--xattr-ro", "xattr-ro", OPTKEY_XATTR_READ_ONLY),
        OPT2("--xattr-rw", "xattr-rw", OPTKEY_XATTR_READ_WRITE),

        OPT2("--delete-deny", "delete-deny", OPTKEY_DELETE_DENY),
        OPT2("--rename-deny", "rename-deny", OPTKEY_RENAME_DENY),

        OPT2("--hide-hard-links", "hide-hard-links", OPTKEY_HIDE_HARD_LINKS),
        OPT2("--resolve-symlinks", "resolve-symlinks", OPTKEY_RESOLVE_SYMLINKS),
        OPT_OFFSET2("--resolved-symlink-deletion=%s", "resolved-symlink-deletion=%s", resolved_symlink_deletion, -1),
        OPT2("--block-devices-as-files", "block-devices-as-files", OPTKEY_BLOCK_DEVICES_AS_FILES),

        OPT2("--realistic-permissions", "realistic-permissions", OPTKEY_REALISTIC_PERMISSIONS),
        OPT2("--ctime-from-mtime", "ctime-from-mtime", OPTKEY_CTIME_FROM_MTIME),
        OPT2("--enable-lock-forwarding", "enable-lock-forwarding", OPTKEY_ENABLE_LOCK_FORWARDING),
        OPT2("--disable-lock-forwarding", "disable-lock-forwarding", OPTKEY_DISABLE_LOCK_FORWARDING),
        OPT2("--enable-ioctl", "enable-ioctl", OPTKEY_ENABLE_IOCTL),
        OPT_OFFSET2("--no-user-group-precaching", "no-user-group-precaching", no_user_group_precaching, -1),
        OPT_OFFSET2("--multithreaded", "multithreaded", multithreaded, -1),
        OPT_OFFSET2("--uid-offset=%s", "uid-offset=%s", uid_offset, 0),
        OPT_OFFSET2("--gid-offset=%s", "gid-offset=%s", gid_offset, 0),

        FUSE_OPT_END
    };

    int fuse_main_return;


    /* Initialize settings */
    memset(&od, 0, sizeof(od));
    settings.progname = argv[0];
    settings.permchain = permchain_create();
    settings.usermap = usermap_create();
    settings.usermap_reverse = usermap_create();
    settings.read_limiter = NULL;
    settings.write_limiter = NULL;
    settings.new_uid = -1;
    settings.new_gid = -1;
    settings.create_for_uid = -1;
    settings.create_for_gid = -1;
    settings.mntsrc = NULL;
    settings.mntdest = NULL;
    settings.mntdest_len = 0;
    settings.original_working_dir = get_working_dir();
    settings.create_policy = (getuid() == 0) ? CREATE_AS_USER : CREATE_AS_MOUNTER;
    settings.create_permchain = permchain_create();
    settings.chown_policy = CHOWN_NORMAL;
    settings.chgrp_policy = CHGRP_NORMAL;
    settings.chmod_policy = CHMOD_NORMAL;
    settings.chmod_allow_x = 0;
    settings.chmod_permchain = permchain_create();
    settings.xattr_policy = XATTR_READ_WRITE;
    settings.delete_deny = 0;
    settings.rename_deny = 0;
    settings.mirrored_users_only = 0;
    settings.mirrored_users = NULL;
    settings.num_mirrored_users = 0;
    settings.mirrored_members = NULL;
    settings.num_mirrored_members = 0;
    settings.hide_hard_links = 0;
    settings.resolve_symlinks = 0;
    settings.resolved_symlink_deletion_policy = RESOLVED_SYMLINK_DELETION_SYMLINK_ONLY;
    settings.block_devices_as_files = 0;
    settings.realistic_permissions = 0;
    settings.ctime_from_mtime = 0;
    settings.enable_lock_forwarding = 0;
    settings.enable_ioctl = 0;
    settings.uid_offset = 0;
    settings.gid_offset = 0;

    atexit(&atexit_func);

    /* Parse options */
    if (fuse_opt_parse(&args, &od, options, &process_option) == -1)
        return 1;

    /* Check that a source directory and a mount point was given */
    if (!settings.mntsrc || !settings.mntdest) {
        print_usage(my_basename(argv[0]));
        return 1;
    }

    /* Check for deprecated options */
    if (od.deprecated_user) {
        fprintf(stderr, "Deprecation warning: please use --force-user instead of --user or --owner.\n");
        fprintf(stderr, "The new option has the same effect. See the man page for details.\n");
        if (!od.user) {
            od.user = od.deprecated_user;
        }
    }
    if (od.deprecated_group) {
        fprintf(stderr, "Deprecation warning: please use --force-group instead of --group.\n");
        fprintf(stderr, "The new option has the same effect. See the man page for details.\n");
        if (!od.group) {
            od.group = od.deprecated_group;
        }
    }

    /* Parse new owner and group */
    if (od.user) {
        if (!user_uid(od.user, &settings.new_uid)) {
            fprintf(stderr, "Not a valid user ID: %s\n", od.user);
            return 1;
        }
    }
    if (od.group) {
        if (!group_gid(od.group, &settings.new_gid)) {
            fprintf(stderr, "Not a valid group ID: %s\n", od.group);
            return 1;
        }
    }

    /* Parse rate limits */
    if (od.read_rate) {
        double rate;
        if (parse_byte_count(od.read_rate, &rate) && rate > 0) {
            settings.read_limiter = malloc(sizeof(RateLimiter));
            rate_limiter_init(settings.read_limiter, rate, &gettimeofday_clock);
        } else {
            fprintf(stderr, "Error: Invalid --read-rate.\n");
            return 1;
        }
    }
    if (od.write_rate) {
        double rate;
        if (parse_byte_count(od.write_rate, &rate) && rate > 0) {
            settings.write_limiter = malloc(sizeof(RateLimiter));
            rate_limiter_init(settings.write_limiter, rate, &gettimeofday_clock);
        } else {
            fprintf(stderr, "Error: Invalid --write-rate.\n");
            return 1;
        }
    }

    /* Parse usermap */
    if (od.map) {
        if (getuid() != 0) {
            fprintf(stderr, "Error: You need to be root to use --map !\n");
            return 1;
        }
        if (!parse_user_map(settings.usermap, settings.usermap_reverse, od.map)) {
            /* parse_user_map printed an error */
            return 1;
        }
    }

    if (od.uid_offset) {
        if (getuid() != 0) {
            fprintf(stderr, "Error: You need to be root to use --uid-offset !\n");
            return 1;
        }
        if (od.map) {
            fprintf(stderr, "Error: Cannot use --uid-offset and --map together!\n");
            return 1;
        }
        char* endptr = od.uid_offset;
        settings.uid_offset = strtoul(od.uid_offset, &endptr, 10);
        if (*endptr != '\0') {
            fprintf(stderr, "Error: Value of --uid-offset must be a positive integer.\n");
            return 1;
        }
    }

    if (od.gid_offset) {
        if (getuid() != 0) {
            fprintf(stderr, "Error: You need to be root to use --gid-offset !\n");
            return 1;
        }
        if (od.map) {
            fprintf(stderr, "Error: Cannot use --gid-offset and --map together!\n");
            return 1;
        }
        char* endptr = od.gid_offset;
        settings.gid_offset = strtoul(od.gid_offset, &endptr, 10);
        if (*endptr != '\0') {
            fprintf(stderr, "Error: Value of --gid-offset must be a positive integer.\n");
            return 1;
        }
    }

    /* Parse user and group for new creates */
    if (od.create_for_user) {
        if (getuid() != 0) {
            fprintf(stderr, "Error: You need to be root to use --create-for-user !\n");
            return 1;
        }
        if (!user_uid(od.create_for_user, &settings.create_for_uid)) {
            fprintf(stderr, "Not a valid user ID: %s\n", od.create_for_user);
            return 1;
        }
    }
    if (od.create_for_group) {
        if (getuid() != 0) {
            fprintf(stderr, "Error: You need to be root to use --create-for-group !\n");
            return 1;
        }
        if (!group_gid(od.create_for_group, &settings.create_for_gid)) {
            fprintf(stderr, "Not a valid group ID: %s\n", od.create_for_group);
            return 1;
        }
    }

    /* Parse mirrored users and groups */
    if (od.mirror && od.mirror_only) {
        fprintf(stderr, "Cannot specify both -m|--mirror and -M|--mirror-only\n");
        return 1;
    }
    if (od.mirror_only) {
        settings.mirrored_users_only = 1;
        od.mirror = od.mirror_only;
    }
    if (od.mirror) {
        if (!parse_mirrored_users(od.mirror)) {
            return 0;
        }
    }

    /* Precache entire user/group database for use by --mirror by default */
    if (!od.no_user_group_precaching) {
        rebuild_user_caches();
    }

    /* Parse permission bits */
    if (od.perms) {
        if (add_chmod_rules_to_permchain(od.perms, settings.permchain) != 0) {
            fprintf(stderr, "Invalid permission specification: '%s'\n", od.perms);
            return 1;
        }
    }
    if (od.create_with_perms) {
        if (add_chmod_rules_to_permchain(od.create_with_perms, settings.create_permchain) != 0) {
            fprintf(stderr, "Invalid permission specification: '%s'\n", od.create_with_perms);
            return 1;
        }
    }
    if (od.chmod_filter) {
        if (add_chmod_rules_to_permchain(od.chmod_filter, settings.chmod_permchain) != 0) {
            fprintf(stderr, "Invalid permission specification: '%s'\n", od.chmod_filter);
            return 1;
        }
    }


    /* Parse resolved_symlink_deletion */
    if (od.resolved_symlink_deletion) {
        if (strcmp(od.resolved_symlink_deletion, "deny") == 0) {
            settings.resolved_symlink_deletion_policy = RESOLVED_SYMLINK_DELETION_DENY;
        } else if (strcmp(od.resolved_symlink_deletion, "symlink-only") == 0) {
            settings.resolved_symlink_deletion_policy = RESOLVED_SYMLINK_DELETION_SYMLINK_ONLY;
        } else if (strcmp(od.resolved_symlink_deletion, "symlink-first") == 0) {
            settings.resolved_symlink_deletion_policy = RESOLVED_SYMLINK_DELETION_SYMLINK_FIRST;
        } else if (strcmp(od.resolved_symlink_deletion, "target-first") == 0) {
            settings.resolved_symlink_deletion_policy = RESOLVED_SYMLINK_DELETION_TARGET_FIRST;
        } else {
            fprintf(stderr, "Invalid setting for --resolved-symlink-deletion: '%s'\n", od.resolved_symlink_deletion);
            return 1;
        }
    }


    /* Single-threaded mode by default */
    if (!od.multithreaded) {
        fuse_opt_add_arg(&args, "-s");
    }

    /* Add default fuse options */
    if (!od.no_allow_other) {
        fuse_opt_add_arg(&args, "-oallow_other");
    }

    /* We want the kernel to do our access checks for us based on what getattr gives it. */
    fuse_opt_add_arg(&args, "-odefault_permissions");

    /* We want to mirror inodes. */
    fuse_opt_add_arg(&args, "-ouse_ino");
    fuse_opt_add_arg(&args, "-oreaddir_ino");

    /* Show the source dir in the first field on /etc/mtab, to be consistent
       with "real" filesystems.

       We don't do this if the source dir contains some special characters.
       Comma is on this list because it would mess up FUSE's option parsing
       (issue #47). The character blacklist is likely not complete, which is
       acceptable since this is not a security check. The aim is to avoid giving
       the user a confusing error. */
    if (strpbrk(settings.mntsrc, ", \t\n") == NULL) {
        char *tmp = sprintf_new("-ofsname=%s", settings.mntsrc);
        fuse_opt_add_arg(&args, tmp);
        free(tmp);
    }

    /* We need to disable the attribute cache whenever two users
       can see different attributes. For now, only mirroring can do that. */
    if (is_mirroring_enabled()) {
        fuse_opt_add_arg(&args, "-oattr_timeout=0");
    }

    /* If the mount source and destination directories are the same
       then don't require that the directory be empty. */
    if (strcmp(settings.mntsrc, settings.mntdest) == 0)
        fuse_opt_add_arg(&args, "-ononempty");

    /* Open mount source for chrooting in bindfs_init */
    settings.mntsrc_fd = open(settings.mntsrc, O_RDONLY);
    if (settings.mntsrc_fd == -1) {
        fprintf(stderr, "Could not open source directory\n");
        return 1;
    }

    /* Ignore the umask of the mounter on file creation */
    settings.original_umask = umask(0);

    /* Remove xattr implementation if the user doesn't want it */
    if (settings.xattr_policy == XATTR_UNIMPLEMENTED) {
        bindfs_oper.setxattr = NULL;
        bindfs_oper.getxattr = NULL;
        bindfs_oper.listxattr = NULL;
        bindfs_oper.removexattr = NULL;
    }

#ifdef HAVE_FUSE_29
    /* Check that lock forwarding is not enabled in single-threaded mode. */
    if (settings.enable_lock_forwarding && !od.multithreaded) {
        fprintf(stderr, "To use --enable-lock-forwarding, you must use "
                        "--multithreaded, but see the man page for caveats!\n");
        return 1;
    }

    /* Remove the locking implementation unless the user has enabled lock
       forwarding. FUSE implements locking inside the mountpoint by default. */
    if (!settings.enable_lock_forwarding) {
        bindfs_oper.lock = NULL;
        bindfs_oper.flock = NULL;
    }
#else
    if (settings.enable_lock_forwarding) {
        fprintf(stderr, "To use --enable-lock-forwarding, bindfs must be "
                        "compiled with FUSE 2.9.0 or newer.\n");
        return 1;
    }
#endif

    /* Remove the ioctl implementation unless the user has enabled it */
    if (!settings.enable_ioctl) {
        bindfs_oper.ioctl = NULL;
    }

    /* fuse_main will daemonize by fork()'ing. The signal handler will persist. */
    setup_signal_handling();

    fuse_main_return = fuse_main(args.argc, args.argv, &bindfs_oper, NULL);

    fuse_opt_free_args(&args);
    close(settings.mntsrc_fd);

    return fuse_main_return;
}
